#include <lumen/common/logging.h>
#include <gtest/gtest.h>
#include <sstream>
#include <regex>

using namespace lumen;

// Helper to capture stderr output
class StderrCapture {
public:
    StderrCapture() {
        old_buf_ = std::cerr.rdbuf();
        std::cerr.rdbuf(buffer_.rdbuf());
    }
    
    ~StderrCapture() {
        std::cerr.rdbuf(old_buf_);
    }
    
    std::string get() const {
        return buffer_.str();
    }
    
    void clear() {
        buffer_.str("");
        buffer_.clear();
    }
    
private:
    std::streambuf* old_buf_;
    std::stringstream buffer_;
};

TEST(LoggingTest, CanLogMessages) {
    // Save original log level
    auto original_level = Logger::instance().get_level();
    
    // Set to INFO level
    Logger::instance().set_level(LogLevel::INFO);
    
    StderrCapture capture;
    
    LUMEN_LOG_INFO("test message");
    
    std::string output = capture.get();
    EXPECT_NE(output.find("test message"), std::string::npos);
    EXPECT_NE(output.find("[INFO ]"), std::string::npos);
    
    // Restore original level
    Logger::instance().set_level(original_level);
}

TEST(LoggingTest, LogLevels) {
    auto original_level = Logger::instance().get_level();
    Logger::instance().set_level(LogLevel::WARN);
    
    StderrCapture capture;
    
    // These should not appear
    LUMEN_LOG_TRACE("trace");
    LUMEN_LOG_DEBUG("debug");
    LUMEN_LOG_INFO("info");
    
    // These should appear
    LUMEN_LOG_WARN("warning");
    LUMEN_LOG_ERROR("error");
    
    std::string output = capture.get();
    
    // Check what's not there
    EXPECT_EQ(output.find("trace"), std::string::npos);
    EXPECT_EQ(output.find("debug"), std::string::npos);
    EXPECT_EQ(output.find("info"), std::string::npos);
    
    // Check what's there
    EXPECT_NE(output.find("warning"), std::string::npos);
    EXPECT_NE(output.find("error"), std::string::npos);
    EXPECT_NE(output.find("[WARN ]"), std::string::npos);
    EXPECT_NE(output.find("[ERROR]"), std::string::npos);
    
    Logger::instance().set_level(original_level);
}

TEST(LoggingTest, LogWithStreaming) {
    auto original_level = Logger::instance().get_level();
    Logger::instance().set_level(LogLevel::INFO);
    
    StderrCapture capture;
    
    int value = 42;
    std::string str = "hello";
    LUMEN_LOG_INFO("Value is " << value << " and string is '" << str << "'");
    
    std::string output = capture.get();
    EXPECT_NE(output.find("Value is 42 and string is 'hello'"), std::string::npos);
    
    Logger::instance().set_level(original_level);
}

TEST(LoggingTest, ConditionalLogging) {
    auto original_level = Logger::instance().get_level();
    Logger::instance().set_level(LogLevel::INFO);
    
    StderrCapture capture;
    
    bool condition_true = true;
    bool condition_false = false;
    
    LUMEN_LOG_IF(LogLevel::INFO, condition_true, "This should appear");
    LUMEN_LOG_IF(LogLevel::INFO, condition_false, "This should not appear");
    
    std::string output = capture.get();
    EXPECT_NE(output.find("This should appear"), std::string::npos);
    EXPECT_EQ(output.find("This should not appear"), std::string::npos);
    
    Logger::instance().set_level(original_level);
}

TEST(LoggingTest, DebugLogging) {
    // Debug logs should only appear in debug builds
    auto original_level = Logger::instance().get_level();
    Logger::instance().set_level(LogLevel::TRACE);
    
    StderrCapture capture;
    
    LUMEN_DLOG_DEBUG("debug message");
    
    std::string output = capture.get();
#ifdef NDEBUG
    // In release mode, debug logs should not appear
    EXPECT_EQ(output.find("debug message"), std::string::npos);
#else
    // In debug mode, debug logs should appear
    EXPECT_NE(output.find("debug message"), std::string::npos);
#endif
    
    Logger::instance().set_level(original_level);
}

TEST(LoggingTest, CheckMacros) {
    // Note: We can't test LUMEN_CHECK failure because it would terminate
    // Instead, we'll just verify compilation and basic usage
    
    int x = 5;
    int y = 10;
    
    // These should pass without issues
    LUMEN_CHECK(x < y);
    LUMEN_CHECK_EQ(x, 5);
    LUMEN_CHECK_NE(x, y);
    LUMEN_CHECK_LT(x, y);
    LUMEN_CHECK_LE(x, y);
    LUMEN_CHECK_GT(y, x);
    LUMEN_CHECK_GE(y, x);
    
    // Debug checks
    LUMEN_DCHECK(x < y);
    LUMEN_DCHECK_EQ(x, 5);
}

TEST(LoggingTest, LogFormat) {
    auto original_level = Logger::instance().get_level();
    Logger::instance().set_level(LogLevel::INFO);
    
    StderrCapture capture;
    
    LUMEN_LOG_INFO("test");
    
    std::string output = capture.get();
    
    // Check format: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [file:line] func: message
    // Using regex to match the timestamp pattern
    std::regex pattern(R"(\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}\] \[INFO \] \[test_logging\.cpp:\d+\] .+: test)");
    EXPECT_TRUE(std::regex_search(output, pattern));
    
    Logger::instance().set_level(original_level);
}