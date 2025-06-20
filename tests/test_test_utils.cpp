#include <gtest/gtest.h>
#include <filesystem>
#include <thread>
#include "test_utils.h"

using namespace lumen::test;

TEST(TestUtilsTest, TempFile) {
    std::string file_path;

    {
        auto temp = TempPath::create_temp_file("test_file");
        file_path = temp.path();

        // File path should exist
        EXPECT_FALSE(file_path.empty());

        // Write to the file
        write_file(file_path, "test content");

        // File should exist
        EXPECT_TRUE(std::filesystem::exists(file_path));

        // Read back
        auto content = read_file(file_path);
        EXPECT_EQ(content, "test content");
    }

    // File should be deleted after TempPath destructor
    EXPECT_FALSE(std::filesystem::exists(file_path));
}

TEST(TestUtilsTest, TempDirectory) {
    std::string dir_path;

    {
        auto temp = TempPath::create_temp_dir("test_dir");
        dir_path = temp.path();

        // Directory should exist
        EXPECT_TRUE(std::filesystem::exists(dir_path));
        EXPECT_TRUE(std::filesystem::is_directory(dir_path));

        // Create a file in the directory
        auto file_path = std::filesystem::path(dir_path) / "test.txt";
        write_file(file_path.string(), "test");
        EXPECT_TRUE(std::filesystem::exists(file_path));
    }

    // Directory and its contents should be deleted
    EXPECT_FALSE(std::filesystem::exists(dir_path));
}

TEST(TestUtilsTest, KeepTempFile) {
    std::string file_path;

    {
        auto temp = TempPath::create_temp_file("keep_test");
        file_path = temp.path();
        write_file(file_path, "keep me");

        // Tell it not to cleanup
        temp.keep();
    }

    // File should still exist
    EXPECT_TRUE(std::filesystem::exists(file_path));

    // Clean up manually
    std::filesystem::remove(file_path);
}

TEST(TestUtilsTest, RandomData) {
    auto data1 = generate_random_data(100);
    auto data2 = generate_random_data(100);

    EXPECT_EQ(data1.size(), 100);
    EXPECT_EQ(data2.size(), 100);

    // Should be different (extremely unlikely to be the same)
    EXPECT_NE(data1, data2);
}

TEST(TestUtilsTest, RandomString) {
    auto str1 = generate_random_string(20);
    auto str2 = generate_random_string(20);

    EXPECT_EQ(str1.size(), 20);
    EXPECT_EQ(str2.size(), 20);

    // Should contain only alphanumeric characters
    for (char c : str1) {
        EXPECT_TRUE(std::isalnum(c));
    }

    // Should be different
    EXPECT_NE(str1, str2);
}

TEST(TestUtilsTest, Timer) {
    Timer timer;

    // Sleep for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto elapsed_ms = timer.elapsed_milliseconds();
    EXPECT_GE(elapsed_ms, 10.0);
    EXPECT_LT(elapsed_ms, 50.0);  // Should not take too long

    // Reset and measure again
    timer.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    elapsed_ms = timer.elapsed_milliseconds();
    EXPECT_GE(elapsed_ms, 5.0);
    EXPECT_LT(elapsed_ms, 20.0);
}

TEST(TestUtilsTest, MemoryTracker) {
    MemoryTracker tracker;

    // Track some allocations
    tracker.track_allocation(100);
    tracker.track_allocation(200);
    tracker.track_allocation(300);

    auto stats = tracker.get_stats();
    EXPECT_EQ(stats.current_usage, 600);
    EXPECT_EQ(stats.peak_usage, 600);
    EXPECT_EQ(stats.allocation_count, 3);
    EXPECT_EQ(stats.deallocation_count, 0);

    // Track deallocations
    tracker.track_deallocation(200);

    stats = tracker.get_stats();
    EXPECT_EQ(stats.current_usage, 400);
    EXPECT_EQ(stats.peak_usage, 600);  // Peak remains the same
    EXPECT_EQ(stats.allocation_count, 3);
    EXPECT_EQ(stats.deallocation_count, 1);

    // Reset
    tracker.reset();
    stats = tracker.get_stats();
    EXPECT_EQ(stats.current_usage, 0);
    EXPECT_EQ(stats.peak_usage, 0);
}

TEST(TestUtilsTest, TestDataGenerator) {
    // Generate sequence
    auto seq = TestDataGenerator<int>::generate_sequence(10, 100);
    EXPECT_EQ(seq.size(), 10);
    for (size_t i = 0; i < seq.size(); ++i) {
        EXPECT_EQ(seq[i], 100 + i);
    }

    // Generate random integers
    auto random_ints = TestDataGenerator<int>::generate_random(100, 0, 10);
    EXPECT_EQ(random_ints.size(), 100);
    for (int val : random_ints) {
        EXPECT_GE(val, 0);
        EXPECT_LE(val, 10);
    }

    // Generate random floats
    auto random_floats = TestDataGenerator<float>::generate_random(50, 0.0f, 1.0f);
    EXPECT_EQ(random_floats.size(), 50);
    for (float val : random_floats) {
        EXPECT_GE(val, 0.0f);
        EXPECT_LE(val, 1.0f);
    }
}