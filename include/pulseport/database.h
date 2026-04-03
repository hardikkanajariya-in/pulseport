#pragma once

#include <string>
#include <filesystem>

struct sqlite3;

namespace pulseport {

/// Manages the SQLite database connection and lifecycle.
class Database {
public:
    Database();
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /// Open (or create) the database at the given path.
    /// Applies pragmas and runs pending migrations.
    bool open(const std::filesystem::path& db_path,
              const std::filesystem::path& migrations_dir);

    /// Close the database.
    void close();

    /// Get the raw sqlite3 handle for queries. Caller must not close it.
    sqlite3* handle() const { return db_; }

    /// Returns true if the database is open.
    bool is_open() const { return db_ != nullptr; }

    /// Run a WAL checkpoint.
    bool checkpoint();

    /// Get database file size in bytes.
    int64_t file_size_bytes() const;

    /// Get WAL file size in bytes.
    int64_t wal_size_bytes() const;

private:
    bool apply_pragmas();

    sqlite3* db_ = nullptr;
    std::filesystem::path db_path_;
};

} // namespace pulseport
