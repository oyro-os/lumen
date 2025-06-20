#pragma once

#include <string>
#include <string_view>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace lumen {

enum class LogLevel {
    TRACE = 0,
    DEBUG_LEVEL = 1,  // Renamed to avoid conflict with DEBUG macro
    INFO = 2,
    WARN = 3,
    ERROR_LEVEL = 4,  // Renamed to avoid potential conflicts
    FATAL = 5,
    OFF = 6
};

// Global log level (can be changed at runtime)
extern LogLevel g_log_level;

// Simple logger class
class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }
    
    void set_level(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        g_log_level = level;
    }
    
    LogLevel get_level() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return g_log_level;
    }
    
    void log(LogLevel level, const std::string& file, int line, 
             const std::string& func, const std::string& message) {
        if (level < g_log_level) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Get current time
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        // Format timestamp
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &time_t);
#else
        localtime_r(&time_t, &tm);
#endif
        
        // Output format: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [file:line] func: message
        std::cerr << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") 
                  << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
                  << "[" << level_to_string(level) << "] "
                  << "[" << basename(file) << ":" << line << "] "
                  << func << ": " << message << std::endl;
    }
    
private:
    Logger() = default;
    mutable std::mutex mutex_;
    
    static const char* level_to_string(LogLevel level) {
        switch (level) {
            case LogLevel::TRACE: return "TRACE";
            case LogLevel::DEBUG_LEVEL: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERROR_LEVEL: return "ERROR";
            case LogLevel::FATAL: return "FATAL";
            default: return "?????";
        }
    }
    
    static const char* basename(const std::string& path) {
        size_t pos = path.find_last_of("/\\");
        return (pos == std::string::npos) ? path.c_str() : path.c_str() + pos + 1;
    }
};

// Log message builder
class LogMessage {
public:
    LogMessage(LogLevel level, const char* file, int line, const char* func)
        : level_(level), file_(file), line_(line), func_(func) {}
    
    ~LogMessage() {
        Logger::instance().log(level_, file_, line_, func_, stream_.str());
    }
    
    template<typename T>
    LogMessage& operator<<(const T& value) {
        stream_ << value;
        return *this;
    }
    
private:
    LogLevel level_;
    const char* file_;
    int line_;
    const char* func_;
    std::ostringstream stream_;
};

// Logging macros
#define LUMEN_LOG(level) \
    if (level >= ::lumen::g_log_level) \
        ::lumen::LogMessage(level, __FILE__, __LINE__, __func__)

#define LUMEN_LOG_TRACE(msg) LUMEN_LOG(::lumen::LogLevel::TRACE) << msg
#define LUMEN_LOG_DEBUG(msg) LUMEN_LOG(::lumen::LogLevel::DEBUG_LEVEL) << msg
#define LUMEN_LOG_INFO(msg)  LUMEN_LOG(::lumen::LogLevel::INFO) << msg
#define LUMEN_LOG_WARN(msg)  LUMEN_LOG(::lumen::LogLevel::WARN) << msg
#define LUMEN_LOG_ERROR(msg) LUMEN_LOG(::lumen::LogLevel::ERROR_LEVEL) << msg
#define LUMEN_LOG_FATAL(msg) LUMEN_LOG(::lumen::LogLevel::FATAL) << msg

// Conditional logging
#define LUMEN_LOG_IF(level, condition, msg) \
    if ((condition) && (level >= ::lumen::g_log_level)) \
        ::lumen::LogMessage(level, __FILE__, __LINE__, __func__) << msg

// Debug-only logging (compiled out in release)
#ifdef NDEBUG
    #define LUMEN_DLOG_TRACE(msg) ((void)0)
    #define LUMEN_DLOG_DEBUG(msg) ((void)0)
    #define LUMEN_DLOG_INFO(msg)  ((void)0)
#else
    #define LUMEN_DLOG_TRACE(msg) LUMEN_LOG_TRACE(msg)
    #define LUMEN_DLOG_DEBUG(msg) LUMEN_LOG_DEBUG(msg)
    #define LUMEN_DLOG_INFO(msg)  LUMEN_LOG_INFO(msg)
#endif

// Check macros with logging
#define LUMEN_CHECK(condition) \
    if (!(condition)) \
        ::lumen::LogMessage(::lumen::LogLevel::FATAL, __FILE__, __LINE__, __func__) \
            << "Check failed: " #condition

#define LUMEN_CHECK_EQ(a, b) LUMEN_CHECK((a) == (b))
#define LUMEN_CHECK_NE(a, b) LUMEN_CHECK((a) != (b))
#define LUMEN_CHECK_LT(a, b) LUMEN_CHECK((a) < (b))
#define LUMEN_CHECK_LE(a, b) LUMEN_CHECK((a) <= (b))
#define LUMEN_CHECK_GT(a, b) LUMEN_CHECK((a) > (b))
#define LUMEN_CHECK_GE(a, b) LUMEN_CHECK((a) >= (b))

// Debug-only checks
#ifdef NDEBUG
    #define LUMEN_DCHECK(condition) ((void)0)
    #define LUMEN_DCHECK_EQ(a, b) ((void)0)
    #define LUMEN_DCHECK_NE(a, b) ((void)0)
    #define LUMEN_DCHECK_LT(a, b) ((void)0)
    #define LUMEN_DCHECK_LE(a, b) ((void)0)
    #define LUMEN_DCHECK_GT(a, b) ((void)0)
    #define LUMEN_DCHECK_GE(a, b) ((void)0)
#else
    #define LUMEN_DCHECK(condition) LUMEN_CHECK(condition)
    #define LUMEN_DCHECK_EQ(a, b) LUMEN_CHECK_EQ(a, b)
    #define LUMEN_DCHECK_NE(a, b) LUMEN_CHECK_NE(a, b)
    #define LUMEN_DCHECK_LT(a, b) LUMEN_CHECK_LT(a, b)
    #define LUMEN_DCHECK_LE(a, b) LUMEN_CHECK_LE(a, b)
    #define LUMEN_DCHECK_GT(a, b) LUMEN_CHECK_GT(a, b)
    #define LUMEN_DCHECK_GE(a, b) LUMEN_CHECK_GE(a, b)
#endif

} // namespace lumen