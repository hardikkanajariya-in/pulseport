#pragma once

#include <filesystem>
#include <vector>
#include <string>

struct sqlite3;

namespace pulseport {

/// Applies numbered SQL migration files to the database.
class MigrationRunner {
public:
    struct MigrationFile {
        int         version;
        std::string filename;
        std::string up_sql;
        std::string down_sql;
    };

    /// Discover and sort migration files from the given directory.
    static std::vector<MigrationFile> discover(const std::filesystem::path& dir);

    /// Apply all pending migrations (version > current) to the database.
    /// Returns the number of migrations applied, or -1 on error.
    static int apply(sqlite3* db, const std::filesystem::path& migrations_dir);

    /// Get the current schema version from the database. Returns 0 if uninitialized.
    static int current_version(sqlite3* db);

private:
    static bool ensure_version_table(sqlite3* db);
    static bool execute_sql(sqlite3* db, const std::string& sql);
    static bool set_version(sqlite3* db, int version);
};

} // namespace pulseport
