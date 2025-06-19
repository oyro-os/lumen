#ifndef LUMEN_COMMON_LOGGING_H
#define LUMEN_COMMON_LOGGING_H

#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <mutex>

// Undefine macros that might conflict with our enums
#ifdef DEBUG
#undef DEBUG
#endif
#ifdef ERROR
#undef ERROR
#endif

namespace lumen {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    FATAL = 4,
    NONE = 5
};

class Logger {
public:
    static Logger& instance() {
        static Logger instance;
        return instance;
    }

    void set_level(LogLevel level) {
        current_level_ = level;
    }

    LogLevel get_level() const {
        return current_level_;
    }

    bool should_log(LogLevel level) const {
        return level >= current_level_;
    }

    void log(LogLevel level, const std::string& message, const char* file, int line) {
        if (!should_log(level)) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::cerr << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] "
                  << "[" << level_to_string(level) << "] "
                  << "[" << file << ":" << line << "] "
                  << message << std::endl;
    }

private:
    Logger() : current_level_(LogLevel::INFO) {}
    
    LogLevel current_level_;
    mutable std::mutex mutex_;

    const char* level_to_string(LogLevel level) const {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::FATAL: return "FATAL";
            default: return "UNKNOWN";
        }
    }
};

class LogStream {
public:
    LogStream(LogLevel level, const char* file, int line)
        : level_(level), file_(file), line_(line) {}

    ~LogStream() {
        Logger::instance().log(level_, stream_.str(), file_, line_);
    }

    template<typename T>
    LogStream& operator<<(const T& value) {
        stream_ << value;
        return *this;
    }

private:
    LogLevel level_;
    const char* file_;
    int line_;
    std::ostringstream stream_;
};

// Logging macros that can be completely disabled in production
#ifdef NDEBUG
    // Production build - logging can be disabled
    #define LOG_DEBUG if (false) lumen::LogStream(lumen::LogLevel::DEBUG, __FILE__, __LINE__)
    #define LOG_INFO if (false) lumen::LogStream(lumen::LogLevel::INFO, __FILE__, __LINE__)
    #define LOG_WARNING if (false) lumen::LogStream(lumen::LogLevel::WARNING, __FILE__, __LINE__)
    #define LOG_ERROR lumen::LogStream(lumen::LogLevel::ERROR, __FILE__, __LINE__)
    #define LOG_FATAL lumen::LogStream(lumen::LogLevel::FATAL, __FILE__, __LINE__)
#else
    // Debug build - all logging enabled
    #define LOG_DEBUG if (lumen::Logger::instance().should_log(lumen::LogLevel::DEBUG)) \
        lumen::LogStream(lumen::LogLevel::DEBUG, __FILE__, __LINE__)
    #define LOG_INFO if (lumen::Logger::instance().should_log(lumen::LogLevel::INFO)) \
        lumen::LogStream(lumen::LogLevel::INFO, __FILE__, __LINE__)
    #define LOG_WARNING if (lumen::Logger::instance().should_log(lumen::LogLevel::WARNING)) \
        lumen::LogStream(lumen::LogLevel::WARNING, __FILE__, __LINE__)
    #define LOG_ERROR if (lumen::Logger::instance().should_log(lumen::LogLevel::ERROR)) \
        lumen::LogStream(lumen::LogLevel::ERROR, __FILE__, __LINE__)
    #define LOG_FATAL lumen::LogStream(lumen::LogLevel::FATAL, __FILE__, __LINE__)
#endif

// Convenience macro to set log level
#define SET_LOG_LEVEL(level) lumen::Logger::instance().set_level(level)

} // namespace lumen

#endif // LUMEN_COMMON_LOGGING_H