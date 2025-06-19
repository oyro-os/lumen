#ifndef LUMEN_TYPES_H
#define LUMEN_TYPES_H

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace lumen {

// Forward declarations
class Value;
class Row;

// Type aliases for clarity
using byte = uint8_t;
using PageID = uint32_t;
using SlotID = uint16_t;
using FrameID = uint32_t;
using TransactionID = uint64_t;
// Timestamp wrapper to avoid conflict with int64_t in variant
struct Timestamp {
    int64_t value;
    explicit Timestamp(int64_t v = 0) : value(v) {}
    operator int64_t() const {
        return value;
    }
    bool operator==(const Timestamp& other) const {
        return value == other.value;
    }
    bool operator<(const Timestamp& other) const {
        return value < other.value;
    }
};

// Constants
constexpr size_t kPageSize = 16384;  // 16KB pages
constexpr size_t kCacheLineSize = 64;
constexpr PageID kInvalidPageID = 0;
constexpr FrameID kInvalidFrameID = UINT32_MAX;
constexpr TransactionID kInvalidTransactionID = 0;

// Value variant type that can hold any supported data type
using ValueVariant = std::variant<std::monostate,  // NULL
                                  bool, int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t,
                                  uint32_t, uint64_t, float, double, std::string,
                                  std::vector<byte>,              // BLOB
                                  std::vector<float>, Timestamp,  // Timestamp in microseconds
                                  std::vector<std::pair<std::string, Value>>  // JSON
                                  >;

// Enum matching the C API data types
enum class DataType : uint8_t {
    Null = 0,
    Int8 = 1,
    Int16 = 2,
    Int32 = 3,
    Int64 = 4,
    UInt8 = 5,
    UInt16 = 6,
    UInt32 = 7,
    UInt64 = 8,
    Float32 = 9,
    Float64 = 10,
    Boolean = 11,
    String = 12,
    Blob = 13,
    Timestamp = 14,
    Vector = 15,
    Json = 16
};

// Value class that wraps the variant with type safety
class Value {
   public:
    Value() : data_(std::monostate{}) {}
    explicit Value(bool v) : data_(v) {}
    explicit Value(int8_t v) : data_(v) {}
    explicit Value(int16_t v) : data_(v) {}
    explicit Value(int32_t v) : data_(v) {}
    explicit Value(int64_t v) : data_(v) {}
    explicit Value(uint8_t v) : data_(v) {}
    explicit Value(uint16_t v) : data_(v) {}
    explicit Value(uint32_t v) : data_(v) {}
    explicit Value(uint64_t v) : data_(v) {}
    explicit Value(float v) : data_(v) {}
    explicit Value(double v) : data_(v) {}
    explicit Value(const std::string& v) : data_(v) {}
    explicit Value(std::string&& v) : data_(std::move(v)) {}
    explicit Value(const char* v) : data_(std::string(v)) {}
    explicit Value(const std::vector<byte>& v) : data_(v) {}
    explicit Value(std::vector<byte>&& v) : data_(std::move(v)) {}
    explicit Value(const std::vector<float>& v) : data_(v) {}
    explicit Value(std::vector<float>&& v) : data_(std::move(v)) {}
    explicit Value(const Timestamp& v) : data_(v) {}

    // Type checking
    bool isNull() const {
        return std::holds_alternative<std::monostate>(data_);
    }
    bool isBool() const {
        return std::holds_alternative<bool>(data_);
    }
    bool isInt() const {
        return std::holds_alternative<int8_t>(data_) || std::holds_alternative<int16_t>(data_) ||
               std::holds_alternative<int32_t>(data_) || std::holds_alternative<int64_t>(data_);
    }
    bool isUInt() const {
        return std::holds_alternative<uint8_t>(data_) || std::holds_alternative<uint16_t>(data_) ||
               std::holds_alternative<uint32_t>(data_) || std::holds_alternative<uint64_t>(data_);
    }
    bool isFloat() const {
        return std::holds_alternative<float>(data_) || std::holds_alternative<double>(data_);
    }
    bool isString() const {
        return std::holds_alternative<std::string>(data_);
    }
    bool isBlob() const {
        return std::holds_alternative<std::vector<byte>>(data_);
    }
    bool isVector() const {
        return std::holds_alternative<std::vector<float>>(data_);
    }
    bool isTimestamp() const {
        return std::holds_alternative<Timestamp>(data_);
    }

    // Get data type
    DataType type() const;

    // Value getters (throw if wrong type)
    bool asBool() const;
    int64_t asInt() const;
    uint64_t asUInt() const;
    double asFloat() const;
    const std::string& asString() const;
    const std::vector<byte>& asBlob() const;
    const std::vector<float>& asVector() const;
    Timestamp asTimestamp() const;

    // Safe getters with defaults
    bool getBool(bool defaultValue = false) const;
    int64_t getInt(int64_t defaultValue = 0) const;
    uint64_t getUInt(uint64_t defaultValue = 0) const;
    double getFloat(double defaultValue = 0.0) const;
    std::string getString(const std::string& defaultValue = "") const;

    // Serialization
    size_t serializedSize() const;
    void serialize(byte* buffer) const;
    static Value deserialize(const byte* buffer, size_t& offset);

    // Comparison operators
    bool operator==(const Value& other) const {
        return data_ == other.data_;
    }
    bool operator!=(const Value& other) const {
        return data_ != other.data_;
    }
    bool operator<(const Value& other) const;
    bool operator<=(const Value& other) const {
        return *this < other || *this == other;
    }
    bool operator>(const Value& other) const {
        return other < *this;
    }
    bool operator>=(const Value& other) const {
        return other <= *this;
    }

    // String representation for debugging
    std::string toString() const;

   private:
    ValueVariant data_;
};

// Row represents a collection of values
class Row {
   public:
    Row() = default;
    explicit Row(std::vector<Value> values) : values_(std::move(values)) {}

    // Access
    size_t size() const {
        return values_.size();
    }
    bool empty() const {
        return values_.empty();
    }
    const Value& operator[](size_t index) const {
        return values_[index];
    }
    Value& operator[](size_t index) {
        return values_[index];
    }

    // Modification
    void append(const Value& value) {
        values_.push_back(value);
    }
    void append(Value&& value) {
        values_.push_back(std::move(value));
    }
    void clear() {
        values_.clear();
    }
    void resize(size_t size) {
        values_.resize(size);
    }

    // Iterators
    auto begin() {
        return values_.begin();
    }
    auto end() {
        return values_.end();
    }
    auto begin() const {
        return values_.begin();
    }
    auto end() const {
        return values_.end();
    }

    // Serialization
    size_t serializedSize() const;
    void serialize(byte* buffer) const;
    static Row deserialize(const byte* buffer, size_t& offset);

   private:
    std::vector<Value> values_;
};

// Utility functions
inline size_t align(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

// Type traits for compile-time type checking
template<typename T>
struct is_lumen_numeric
    : std::integral_constant<bool, std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> ||
                                       std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t> ||
                                       std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
                                       std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t> ||
                                       std::is_same_v<T, float> || std::is_same_v<T, double>> {};

template<typename T>
inline constexpr bool is_lumen_numeric_v = is_lumen_numeric<T>::value;

}  // namespace lumen

#endif  // LUMEN_TYPES_H
