#pragma once

#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace lumen {
namespace test {

// Temporary file/directory management
class TempPath {
   public:
    // Create a temporary file with optional prefix
    static TempPath create_temp_file(const std::string& prefix = "lumen_test") {
        return TempPath(generate_temp_path(prefix, ".tmp"), true);
    }

    // Create a temporary directory with optional prefix
    static TempPath create_temp_dir(const std::string& prefix = "lumen_test") {
        auto path = generate_temp_path(prefix, "_dir");
        std::filesystem::create_directories(path);
        return TempPath(path, false);
    }

    // Get the path
    const std::string& path() const {
        return path_;
    }

    // Destructor removes the temporary file/directory
    ~TempPath() {
        if (!path_.empty() && cleanup_) {
            std::error_code ec;
            if (is_file_) {
                std::filesystem::remove(path_, ec);
            } else {
                std::filesystem::remove_all(path_, ec);
            }
        }
    }

    // Move constructor
    TempPath(TempPath&& other) noexcept
        : path_(std::move(other.path_)), is_file_(other.is_file_), cleanup_(other.cleanup_) {
        other.cleanup_ = false;
    }

    // Disable copy
    TempPath(const TempPath&) = delete;
    TempPath& operator=(const TempPath&) = delete;

    // Disable cleanup (useful for debugging)
    void keep() {
        cleanup_ = false;
    }

   private:
    TempPath(const std::string& path, bool is_file)
        : path_(path), is_file_(is_file), cleanup_(true) {}

    static std::string generate_temp_path(const std::string& prefix, const std::string& suffix) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(100000, 999999);

        auto temp_dir = std::filesystem::temp_directory_path();
        auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

        return (temp_dir / (prefix + "_" + std::to_string(timestamp) + "_" +
                            std::to_string(dis(gen)) + suffix))
            .string();
    }

    std::string path_;
    bool is_file_;
    bool cleanup_;
};

// Write data to a file
inline void write_file(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to create file: " + path);
    }
    out.write(content.data(), content.size());
}

// Read data from a file
inline std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// Generate random data
inline std::string generate_random_data(size_t size) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 255);

    std::string data;
    data.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        data.push_back(static_cast<char>(dis(gen)));
    }
    return data;
}

// Generate random alphanumeric string
inline std::string generate_random_string(size_t length) {
    static const char charset[] = "0123456789"
                                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                  "abcdefghijklmnopqrstuvwxyz";
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result.push_back(charset[dis(gen)]);
    }
    return result;
}

// Performance timer
class Timer {
   public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}

    double elapsed_seconds() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(end - start_).count();
    }

    double elapsed_milliseconds() const {
        return elapsed_seconds() * 1000.0;
    }

    double elapsed_microseconds() const {
        return elapsed_seconds() * 1000000.0;
    }

    void reset() {
        start_ = std::chrono::high_resolution_clock::now();
    }

   private:
    std::chrono::high_resolution_clock::time_point start_;
};

// Memory usage tracker
class MemoryTracker {
   public:
    struct Stats {
        size_t current_usage = 0;
        size_t peak_usage = 0;
        size_t allocation_count = 0;
        size_t deallocation_count = 0;
    };

    MemoryTracker() = default;

    void track_allocation(size_t size) {
        stats_.current_usage += size;
        stats_.allocation_count++;
        if (stats_.current_usage > stats_.peak_usage) {
            stats_.peak_usage = stats_.current_usage;
        }
    }

    void track_deallocation(size_t size) {
        stats_.current_usage -= size;
        stats_.deallocation_count++;
    }

    const Stats& get_stats() const {
        return stats_;
    }

    void reset() {
        stats_ = Stats{};
    }

   private:
    Stats stats_;
};

// Test data generator
template<typename T>
class TestDataGenerator {
   public:
    static std::vector<T> generate_sequence(size_t count, T start = T{}) {
        std::vector<T> result;
        result.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            result.push_back(start + static_cast<T>(i));
        }
        return result;
    }

    static std::vector<T> generate_random(size_t count, T min, T max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());

        std::vector<T> result;
        result.reserve(count);

        if constexpr (std::is_integral_v<T>) {
            std::uniform_int_distribution<T> dis(min, max);
            for (size_t i = 0; i < count; ++i) {
                result.push_back(dis(gen));
            }
        } else if constexpr (std::is_floating_point_v<T>) {
            std::uniform_real_distribution<T> dis(min, max);
            for (size_t i = 0; i < count; ++i) {
                result.push_back(dis(gen));
            }
        }

        return result;
    }
};

// Common test patterns
#define EXPECT_OK(status) EXPECT_TRUE((status).is_ok())
#define EXPECT_ERROR(status) EXPECT_FALSE((status).is_ok())
#define ASSERT_OK(status) ASSERT_TRUE((status).is_ok())
#define ASSERT_ERROR(status) ASSERT_FALSE((status).is_ok())

}  // namespace test
}  // namespace lumen