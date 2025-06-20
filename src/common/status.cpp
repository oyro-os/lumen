#include <lumen/common/status.h>
#include <sstream>

namespace lumen {

std::string Status::to_string() const {
    if (is_ok()) {
        return "OK";
    }
    
    std::ostringstream oss;
    
    // Convert error code to string
    switch (code_) {
        case ErrorCode::OK: oss << "OK"; break;
        case ErrorCode::UNKNOWN: oss << "UNKNOWN"; break;
        case ErrorCode::INVALID_ARGUMENT: oss << "INVALID_ARGUMENT"; break;
        case ErrorCode::NOT_FOUND: oss << "NOT_FOUND"; break;
        case ErrorCode::ALREADY_EXISTS: oss << "ALREADY_EXISTS"; break;
        case ErrorCode::PERMISSION_DENIED: oss << "PERMISSION_DENIED"; break;
        case ErrorCode::RESOURCE_EXHAUSTED: oss << "RESOURCE_EXHAUSTED"; break;
        case ErrorCode::FAILED_PRECONDITION: oss << "FAILED_PRECONDITION"; break;
        case ErrorCode::ABORTED: oss << "ABORTED"; break;
        case ErrorCode::OUT_OF_RANGE: oss << "OUT_OF_RANGE"; break;
        case ErrorCode::UNIMPLEMENTED: oss << "UNIMPLEMENTED"; break;
        case ErrorCode::INTERNAL: oss << "INTERNAL"; break;
        case ErrorCode::UNAVAILABLE: oss << "UNAVAILABLE"; break;
        case ErrorCode::DATA_LOSS: oss << "DATA_LOSS"; break;
        case ErrorCode::CORRUPTION: oss << "CORRUPTION"; break;
        case ErrorCode::IO_ERROR: oss << "IO_ERROR"; break;
        case ErrorCode::DISK_FULL: oss << "DISK_FULL"; break;
        case ErrorCode::MEMORY_LIMIT: oss << "MEMORY_LIMIT"; break;
        case ErrorCode::PAGE_NOT_FOUND: oss << "PAGE_NOT_FOUND"; break;
        case ErrorCode::TRANSACTION_CONFLICT: oss << "TRANSACTION_CONFLICT"; break;
        case ErrorCode::LOCK_TIMEOUT: oss << "LOCK_TIMEOUT"; break;
        case ErrorCode::CHECKSUM_MISMATCH: oss << "CHECKSUM_MISMATCH"; break;
        case ErrorCode::VERSION_MISMATCH: oss << "VERSION_MISMATCH"; break;
        case ErrorCode::VALUE_TOO_LARGE: oss << "VALUE_TOO_LARGE"; break;
        case ErrorCode::KEY_TOO_LARGE: oss << "KEY_TOO_LARGE"; break;
        case ErrorCode::INVALID_PATH: oss << "INVALID_PATH"; break;
        case ErrorCode::INDEX_VERSION_MISMATCH: oss << "INDEX_VERSION_MISMATCH"; break;
        default: oss << "UNKNOWN_CODE(" << static_cast<int>(code_) << ")"; break;
    }
    
    if (!message_.empty()) {
        oss << ": " << message_;
    }
    
    return oss.str();
}

} // namespace lumen