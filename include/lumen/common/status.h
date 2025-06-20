#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <memory>
#include <utility>

namespace lumen {

// Error codes for different failure types
enum class ErrorCode {
    OK = 0,
    
    // General errors
    UNKNOWN = 1,
    INVALID_ARGUMENT = 2,
    NOT_FOUND = 3,
    ALREADY_EXISTS = 4,
    PERMISSION_DENIED = 5,
    RESOURCE_EXHAUSTED = 6,
    FAILED_PRECONDITION = 7,
    ABORTED = 8,
    OUT_OF_RANGE = 9,
    UNIMPLEMENTED = 10,
    INTERNAL = 11,
    UNAVAILABLE = 12,
    DATA_LOSS = 13,
    
    // Database-specific errors
    CORRUPTION = 100,
    IO_ERROR = 101,
    DISK_FULL = 102,
    MEMORY_LIMIT = 103,
    PAGE_NOT_FOUND = 104,
    TRANSACTION_CONFLICT = 105,
    LOCK_TIMEOUT = 106,
    CHECKSUM_MISMATCH = 107,
    VERSION_MISMATCH = 108,
    VALUE_TOO_LARGE = 109,
    KEY_TOO_LARGE = 110,
    INVALID_PATH = 111,
    INDEX_VERSION_MISMATCH = 112,
};

// Status represents the result of an operation
class Status {
public:
    // Constructors
    Status() noexcept : code_(ErrorCode::OK) {}
    Status(ErrorCode code, std::string_view message = "") 
        : code_(code), message_(message) {}
    
    // Factory methods for common statuses
    static Status ok() noexcept { return Status(); }
    static Status error(std::string_view message) {
        return Status(ErrorCode::UNKNOWN, message);
    }
    static Status invalid_argument(std::string_view message) {
        return Status(ErrorCode::INVALID_ARGUMENT, message);
    }
    static Status not_found(std::string_view message) {
        return Status(ErrorCode::NOT_FOUND, message);
    }
    static Status corruption(std::string_view message) {
        return Status(ErrorCode::CORRUPTION, message);
    }
    static Status io_error(std::string_view message) {
        return Status(ErrorCode::IO_ERROR, message);
    }
    
    // Check status
    bool is_ok() const noexcept { return code_ == ErrorCode::OK; }
    bool is_error() const noexcept { return code_ != ErrorCode::OK; }
    explicit operator bool() const noexcept { return is_ok(); }
    
    // Get error details
    ErrorCode code() const noexcept { return code_; }
    const std::string& message() const noexcept { return message_; }
    
    // String representation
    std::string to_string() const;
    
private:
    ErrorCode code_;
    std::string message_;
};

// Result<T> represents either a value or an error
template<typename T>
class Result {
public:
    // Constructors
    Result(T value) : value_(std::move(value)), status_(Status::ok()) {}
    Result(Status status) : status_(std::move(status)) {
        if (status_.is_ok()) {
            throw std::invalid_argument("Cannot create ok Result without value");
        }
    }
    
    // Factory methods
    static Result<T> ok(T value) {
        return Result<T>(std::move(value));
    }
    
    static Result<T> error(Status status) {
        return Result<T>(std::move(status));
    }
    
    static Result<T> error(ErrorCode code, std::string_view message) {
        return Result<T>(Status(code, message));
    }
    
    // Check result
    bool is_ok() const noexcept { return status_.is_ok(); }
    bool is_error() const noexcept { return status_.is_error(); }
    explicit operator bool() const noexcept { return is_ok(); }
    
    // Access value (throws if error)
    T& value() & {
        if (!is_ok()) {
            throw std::runtime_error("Result contains error: " + status_.to_string());
        }
        return value_.value();
    }
    
    const T& value() const & {
        if (!is_ok()) {
            throw std::runtime_error("Result contains error: " + status_.to_string());
        }
        return value_.value();
    }
    
    T&& value() && {
        if (!is_ok()) {
            throw std::runtime_error("Result contains error: " + status_.to_string());
        }
        return std::move(value_.value());
    }
    
    // Access value with default
    T value_or(T default_value) const {
        return is_ok() ? value_.value() : std::move(default_value);
    }
    
    // Access error
    const Status& error() const noexcept { return status_; }
    
    // Monadic operations
    template<typename F>
    auto and_then(F&& f) -> decltype(f(std::declval<T>())) {
        using ReturnType = decltype(f(std::declval<T>()));
        if (is_ok()) {
            return f(std::move(value_.value()));
        }
        return ReturnType::error(status_);
    }
    
    template<typename F>
    auto or_else(F&& f) -> Result<T> {
        if (is_error()) {
            return f(status_);
        }
        return std::move(*this);
    }
    
private:
    std::optional<T> value_;
    Status status_;
};

// Specialization for void
template<>
class Result<void> {
public:
    Result() : status_(Status::ok()) {}
    Result(Status status) : status_(std::move(status)) {}
    
    static Result<void> ok() { return Result<void>(); }
    static Result<void> error(Status status) { return Result<void>(std::move(status)); }
    static Result<void> error(ErrorCode code, std::string_view message) {
        return Result<void>(Status(code, message));
    }
    
    bool is_ok() const noexcept { return status_.is_ok(); }
    bool is_error() const noexcept { return status_.is_error(); }
    explicit operator bool() const noexcept { return is_ok(); }
    
    const Status& error() const noexcept { return status_; }
    
private:
    Status status_;
};

} // namespace lumen