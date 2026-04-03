#include "pulseport/database.h"
#include "pulseport/migration_runner.h"

#include <sqlite3.h>
#include <spdlog/spdlog.h>
#include <filesystem>

namespace pulseport {

Database::Database() = default;

Database::~Database() {
    close();
}

bool Database::open(const std::filesystem::path& db_path,
                    const std::filesystem::path& migrations_dir) {
    close();
    db_path_ = db_path;

    // Ensure parent directory exists
    std::filesystem::create_directories(db_path.parent_path());

    int rc = sqlite3_open_v2(
        db_path.string().c_str(), &db_,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr
    );
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to open database '{}': {}", db_path.string(),
                      sqlite3_errmsg(db_));
        db_ = nullptr;
        return false;
    }

    spdlog::info("Opened database: {}", db_path.string());

    if (!apply_pragmas()) {
        close();
        return false;
    }

    // Run migrations
    int applied = MigrationRunner::apply(db_, migrations_dir);
    if (applied < 0) {
        spdlog::error("Migration failed");
        close();
        return false;
    }
    if (applied > 0) {
        spdlog::info("Applied {} migration(s), schema version: {}",
                      applied, MigrationRunner::current_version(db_));
    }

    return true;
}

void Database::close() {
    if (db_) {
        checkpoint();
        sqlite3_close_v2(db_);
        db_ = nullptr;
        spdlog::info("Database closed");
    }
}

bool Database::apply_pragmas() {
    const char* pragmas[] = {
        "PRAGMA journal_mode = WAL;",
        "PRAGMA synchronous = NORMAL;",
        "PRAGMA foreign_keys = ON;",
        "PRAGMA temp_store = MEMORY;",
        "PRAGMA busy_timeout = 5000;",
        "PRAGMA page_size = 4096;",
        "PRAGMA mmap_size = 67108864;",  // 64 MB mmap for read performance
        nullptr
    };

    for (int i = 0; pragmas[i]; ++i) {
        char* err = nullptr;
        int rc = sqlite3_exec(db_, pragmas[i], nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            spdlog::warn("Pragma failed: {} ({})", pragmas[i], err ? err : "unknown");
            sqlite3_free(err);
            // Non-fatal for most pragmas
        }
    }
    return true;
}

bool Database::checkpoint() {
    if (!db_) return false;
    int rc = sqlite3_wal_checkpoint_v2(db_, nullptr,
                                        SQLITE_CHECKPOINT_TRUNCATE,
                                        nullptr, nullptr);
    if (rc != SQLITE_OK && rc != SQLITE_BUSY) {
        spdlog::warn("WAL checkpoint failed: {}", sqlite3_errmsg(db_));
        return false;
    }
    return true;
}

int64_t Database::file_size_bytes() const {
    try {
        return static_cast<int64_t>(std::filesystem::file_size(db_path_));
    } catch (...) {
        return -1;
    }
}

int64_t Database::wal_size_bytes() const {
    try {
        auto wal_path = db_path_;
        wal_path += "-wal";
        if (std::filesystem::exists(wal_path)) {
            return static_cast<int64_t>(std::filesystem::file_size(wal_path));
        }
    } catch (...) {}
    return 0;
}

} // namespace pulseport
