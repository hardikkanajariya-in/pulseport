#include <gtest/gtest.h>
#include "pulseport/database.h"
#include <filesystem>

using namespace pulseport;

class DatabaseTest : public ::testing::Test {
protected:
    std::filesystem::path temp_dir;
    std::filesystem::path db_path;
    std::filesystem::path migrations_path;

    void SetUp() override {
        temp_dir = std::filesystem::temp_directory_path() / "pulseport_test_db";
        std::filesystem::create_directories(temp_dir);
        db_path = temp_dir / "test.db";

        // Point to the actual migrations directory
        migrations_path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "db" / "migrations";
    }

    void TearDown() override {
        std::filesystem::remove_all(temp_dir);
    }
};

TEST_F(DatabaseTest, OpensAndCreatesDatabase) {
    Database db;
    ASSERT_TRUE(db.open(db_path.string(), migrations_path.string()));
    EXPECT_TRUE(std::filesystem::exists(db_path));
    db.close();
}

TEST_F(DatabaseTest, RunsMigrations) {
    Database db;
    ASSERT_TRUE(db.open(db_path.string(), migrations_path.string()));

    // After migrations, schema_version table should exist with version 1
    auto* handle = db.handle();
    ASSERT_NE(handle, nullptr);

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle,
        "SELECT MAX(version) FROM schema_version WHERE success = 1", -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);

    rc = sqlite3_step(stmt);
    ASSERT_EQ(rc, SQLITE_ROW);
    int version = sqlite3_column_int(stmt, 0);
    EXPECT_GE(version, 1);

    sqlite3_finalize(stmt);
    db.close();
}

TEST_F(DatabaseTest, FileSize) {
    Database db;
    ASSERT_TRUE(db.open(db_path.string(), migrations_path.string()));

    auto size = db.file_size();
    EXPECT_GT(size, 0);

    db.close();
}

TEST_F(DatabaseTest, Checkpoint) {
    Database db;
    ASSERT_TRUE(db.open(db_path.string(), migrations_path.string()));

    // Should not throw
    EXPECT_NO_THROW(db.checkpoint());

    db.close();
}
