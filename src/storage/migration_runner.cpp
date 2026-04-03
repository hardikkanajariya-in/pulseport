#include "pulseport/migration_runner.h"

#include <sqlite3.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <regex>

namespace pulseport {

bool MigrationRunner::ensure_version_table(sqlite3* db) {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            version INTEGER NOT NULL
        );
    )";
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to create schema_version table: {}", err ? err : "unknown");
        sqlite3_free(err);
        return false;
    }

    // Ensure at least one row exists
    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM schema_version", -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) == 0) {
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "INSERT INTO schema_version (version) VALUES (0)",
                         nullptr, nullptr, nullptr);
        } else {
            sqlite3_finalize(stmt);
        }
    }
    return true;
}

int MigrationRunner::current_version(sqlite3* db) {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, "SELECT version FROM schema_version LIMIT 1",
                                 -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return 0;

    int version = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        version = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return version;
}

bool MigrationRunner::set_version(sqlite3* db, int version) {
    char* err = nullptr;
    std::string sql = "UPDATE schema_version SET version = " + std::to_string(version);
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to set schema version: {}", err ? err : "unknown");
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool MigrationRunner::execute_sql(sqlite3* db, const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        spdlog::error("SQL execution failed: {}", err ? err : "unknown");
        sqlite3_free(err);
        return false;
    }
    return true;
}

std::vector<MigrationRunner::MigrationFile> MigrationRunner::discover(
    const std::filesystem::path& dir) {
    std::vector<MigrationFile> files;

    if (!std::filesystem::exists(dir)) {
        spdlog::warn("Migrations directory not found: {}", dir.string());
        return files;
    }

    std::regex pattern(R"(^(\d+)_.+\.sql$)");

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;

        std::string filename = entry.path().filename().string();
        std::smatch match;
        if (!std::regex_match(filename, match, pattern)) continue;

        int version = std::stoi(match[1].str());

        // Read file contents
        std::ifstream file(entry.path());
        if (!file.is_open()) {
            spdlog::warn("Cannot read migration file: {}", filename);
            continue;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        // Split into UP and DOWN sections
        std::string up_sql, down_sql;
        auto up_pos = content.find("-- UP");
        auto down_pos = content.find("-- DOWN");

        if (up_pos != std::string::npos) {
            size_t start = content.find('\n', up_pos);
            if (start != std::string::npos) {
                size_t end = (down_pos != std::string::npos) ? down_pos : content.size();
                up_sql = content.substr(start + 1, end - start - 1);
            }
        } else if (down_pos != std::string::npos) {
            // No explicit UP marker; everything before DOWN is UP
            up_sql = content.substr(0, down_pos);
        } else {
            // No markers; entire file is UP
            up_sql = content;
        }

        if (down_pos != std::string::npos) {
            size_t start = content.find('\n', down_pos);
            if (start != std::string::npos) {
                down_sql = content.substr(start + 1);
            }
        }

        files.push_back({version, filename, up_sql, down_sql});
    }

    std::sort(files.begin(), files.end(),
              [](const MigrationFile& a, const MigrationFile& b) {
                  return a.version < b.version;
              });

    return files;
}

int MigrationRunner::apply(sqlite3* db, const std::filesystem::path& migrations_dir) {
    if (!ensure_version_table(db)) return -1;

    int current = current_version(db);
    auto files = discover(migrations_dir);

    int applied = 0;
    for (const auto& mig : files) {
        if (mig.version <= current) continue;

        spdlog::info("Applying migration {}: {}", mig.version, mig.filename);

        // Wrap in transaction
        if (!execute_sql(db, "BEGIN TRANSACTION")) return -1;

        if (!execute_sql(db, mig.up_sql)) {
            execute_sql(db, "ROLLBACK");
            spdlog::error("Migration {} failed, rolled back", mig.version);
            return -1;
        }

        if (!set_version(db, mig.version)) {
            execute_sql(db, "ROLLBACK");
            return -1;
        }

        if (!execute_sql(db, "COMMIT")) return -1;

        spdlog::info("Migration {} applied successfully", mig.version);
        ++applied;
    }

    return applied;
}

} // namespace pulseport
