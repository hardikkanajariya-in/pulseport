#include <gtest/gtest.h>
#include "pulseport/migration_runner.h"
#include <filesystem>
#include <fstream>

using namespace pulseport;

class MigrationRunnerTest : public ::testing::Test {
protected:
    std::filesystem::path temp_dir;
    std::filesystem::path db_path;

    void SetUp() override {
        temp_dir = std::filesystem::temp_directory_path() / "pulseport_test_migrations";
        std::filesystem::create_directories(temp_dir);

        // Create a simple test migration
        std::ofstream f(temp_dir / "001_test.sql");
        f << "-- UP\n"
          << "CREATE TABLE test_table (id INTEGER PRIMARY KEY, name TEXT);\n"
          << "-- DOWN\n"
          << "DROP TABLE test_table;\n";
        f.close();

        db_path = temp_dir / "test.db";
    }

    void TearDown() override {
        std::filesystem::remove_all(temp_dir);
    }
};

TEST_F(MigrationRunnerTest, DiscoversMigrationFiles) {
    MigrationRunner runner(temp_dir.string());
    auto files = runner.discover();

    ASSERT_EQ(files.size(), 1u);
    EXPECT_EQ(files[0].version, 1);
    EXPECT_EQ(files[0].name, "test");
}

TEST_F(MigrationRunnerTest, ParsesUpAndDownSections) {
    MigrationRunner runner(temp_dir.string());
    auto files = runner.discover();

    ASSERT_EQ(files.size(), 1u);
    EXPECT_FALSE(files[0].up_sql.empty());
    EXPECT_FALSE(files[0].down_sql.empty());
    EXPECT_NE(files[0].up_sql.find("CREATE TABLE"), std::string::npos);
    EXPECT_NE(files[0].down_sql.find("DROP TABLE"), std::string::npos);
}

TEST_F(MigrationRunnerTest, MultipleFilesOrderedByVersion) {
    // Create a second migration
    std::ofstream f(temp_dir / "002_another.sql");
    f << "-- UP\n"
      << "ALTER TABLE test_table ADD COLUMN value REAL;\n"
      << "-- DOWN\n"
      << "-- Cannot undo ALTER in SQLite\n";
    f.close();

    MigrationRunner runner(temp_dir.string());
    auto files = runner.discover();

    ASSERT_EQ(files.size(), 2u);
    EXPECT_EQ(files[0].version, 1);
    EXPECT_EQ(files[1].version, 2);
}
