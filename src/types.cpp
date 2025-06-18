#include <lumen/types.h>

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace lumen {

// Helper to write data to buffer
template <typename T>
static void writeToBuffer(byte*& buffer, const T& value) {
    std::memcpy(buffer, &value, sizeof(T));
    buffer += sizeof(T);
}

// Helper to read data from buffer
template <typename T>
static T readFromBuffer(const byte*& buffer) {
    T value;
    std::memcpy(&value, buffer, sizeof(T));
    buffer += sizeof(T);
    return value;
}

DataType Value::type() const {
    return std::visit(
        [](const auto& v) -> DataType {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return DataType::Null;
            } else if constexpr (std::is_same_v<T, bool>) {
                return DataType::Boolean;
            } else if constexpr (std::is_same_v<T, int8_t>) {
                return DataType::Int8;
            } else if constexpr (std::is_same_v<T, int16_t>) {
                return DataType::Int16;
            } else if constexpr (std::is_same_v<T, int32_t>) {
                return DataType::Int32;
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return DataType::Int64;
            } else if constexpr (std::is_same_v<T, uint8_t>) {
                return DataType::UInt8;
            } else if constexpr (std::is_same_v<T, uint16_t>) {
                return DataType::UInt16;
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                return DataType::UInt32;
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                return DataType::UInt64;
            } else if constexpr (std::is_same_v<T, float>) {
                return DataType::Float32;
            } else if constexpr (std::is_same_v<T, double>) {
                return DataType::Float64;
            } else if constexpr (std::is_same_v<T, std::string>) {
                return DataType::String;
            } else if constexpr (std::is_same_v<T, std::vector<byte>>) {
                return DataType::Blob;
            } else if constexpr (std::is_same_v<T, std::vector<float>>) {
                return DataType::Vector;
            } else if constexpr (std::is_same_v<T, Timestamp>) {
                return DataType::Timestamp;
            } else {
                return DataType::Json;
            }
        },
        data_);
}

bool Value::asBool() const {
    if (auto* v = std::get_if<bool>(&data_)) {
        return *v;
    }
    throw std::runtime_error("Value is not a boolean");
}

int64_t Value::asInt() const {
    return std::visit(
        [](const auto& v) -> int64_t {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> ||
                          std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t>) {
                return static_cast<int64_t>(v);
            } else {
                throw std::runtime_error("Value is not an integer");
            }
        },
        data_);
}

uint64_t Value::asUInt() const {
    return std::visit(
        [](const auto& v) -> uint64_t {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
                          std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>) {
                return static_cast<uint64_t>(v);
            } else {
                throw std::runtime_error("Value is not an unsigned integer");
            }
        },
        data_);
}

double Value::asFloat() const {
    return std::visit(
        [](const auto& v) -> double {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
                return static_cast<double>(v);
            } else {
                throw std::runtime_error("Value is not a float");
            }
        },
        data_);
}

const std::string& Value::asString() const {
    if (auto* v = std::get_if<std::string>(&data_)) {
        return *v;
    }
    throw std::runtime_error("Value is not a string");
}

const std::vector<byte>& Value::asBlob() const {
    if (auto* v = std::get_if<std::vector<byte>>(&data_)) {
        return *v;
    }
    throw std::runtime_error("Value is not a blob");
}

const std::vector<float>& Value::asVector() const {
    if (auto* v = std::get_if<std::vector<float>>(&data_)) {
        return *v;
    }
    throw std::runtime_error("Value is not a vector");
}

Timestamp Value::asTimestamp() const {
    if (auto* v = std::get_if<Timestamp>(&data_)) {
        return *v;
    }
    throw std::runtime_error("Value is not a timestamp");
}

bool Value::getBool(bool defaultValue) const {
    if (auto* v = std::get_if<bool>(&data_)) {
        return *v;
    }
    return defaultValue;
}

int64_t Value::getInt(int64_t defaultValue) const {
    try {
        return asInt();
    } catch (...) {
        return defaultValue;
    }
}

uint64_t Value::getUInt(uint64_t defaultValue) const {
    try {
        return asUInt();
    } catch (...) {
        return defaultValue;
    }
}

double Value::getFloat(double defaultValue) const {
    try {
        return asFloat();
    } catch (...) {
        return defaultValue;
    }
}

std::string Value::getString(const std::string& defaultValue) const {
    if (auto* v = std::get_if<std::string>(&data_)) {
        return *v;
    }
    return defaultValue;
}

size_t Value::serializedSize() const {
    size_t size = sizeof(uint8_t);  // Type byte
    std::visit(
        [&size](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                // No additional data for NULL
            } else if constexpr (std::is_same_v<T, std::string>) {
                size += sizeof(uint32_t) + v.size();
            } else if constexpr (std::is_same_v<T, std::vector<byte>>) {
                size += sizeof(uint32_t) + v.size();
            } else if constexpr (std::is_same_v<T, std::vector<float>>) {
                size += sizeof(uint32_t) + v.size() * sizeof(float);
            } else if constexpr (std::is_same_v<T, std::vector<std::pair<std::string, Value>>>) {
                // JSON - not implemented yet
                size += sizeof(uint32_t);
            } else if constexpr (std::is_same_v<T, Timestamp>) {
                size += sizeof(int64_t);
            } else {
                size += sizeof(T);
            }
        },
        data_);
    return size;
}

void Value::serialize(byte* buffer) const {
    *buffer++ = static_cast<uint8_t>(type());

    std::visit(
        [&buffer](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                // No data for NULL
            } else if constexpr (std::is_same_v<T, std::string>) {
                writeToBuffer(buffer, static_cast<uint32_t>(v.size()));
                std::memcpy(buffer, v.data(), v.size());
                buffer += v.size();
            } else if constexpr (std::is_same_v<T, std::vector<byte>>) {
                writeToBuffer(buffer, static_cast<uint32_t>(v.size()));
                std::memcpy(buffer, v.data(), v.size());
                buffer += v.size();
            } else if constexpr (std::is_same_v<T, std::vector<float>>) {
                writeToBuffer(buffer, static_cast<uint32_t>(v.size()));
                std::memcpy(buffer, v.data(), v.size() * sizeof(float));
                buffer += v.size() * sizeof(float);
            } else if constexpr (std::is_same_v<T, std::vector<std::pair<std::string, Value>>>) {
                // JSON - not implemented yet
                writeToBuffer(buffer, uint32_t(0));
            } else if constexpr (std::is_same_v<T, Timestamp>) {
                writeToBuffer(buffer, v.value);
            } else {
                writeToBuffer(buffer, v);
            }
        },
        data_);
}

Value Value::deserialize(const byte* buffer, size_t& offset) {
    const byte* ptr = buffer + offset;
    DataType type = static_cast<DataType>(*ptr++);
    offset++;

    switch (type) {
        case DataType::Null:
            return Value();
        case DataType::Boolean: {
            bool val = readFromBuffer<bool>(ptr);
            offset += sizeof(bool);
            return Value(val);
        }
        case DataType::Int8: {
            int8_t val = readFromBuffer<int8_t>(ptr);
            offset += sizeof(int8_t);
            return Value(val);
        }
        case DataType::Int16: {
            int16_t val = readFromBuffer<int16_t>(ptr);
            offset += sizeof(int16_t);
            return Value(val);
        }
        case DataType::Int32: {
            int32_t val = readFromBuffer<int32_t>(ptr);
            offset += sizeof(int32_t);
            return Value(val);
        }
        case DataType::Int64: {
            int64_t val = readFromBuffer<int64_t>(ptr);
            offset += sizeof(int64_t);
            return Value(val);
        }
        case DataType::UInt8: {
            uint8_t val = readFromBuffer<uint8_t>(ptr);
            offset += sizeof(uint8_t);
            return Value(val);
        }
        case DataType::UInt16: {
            uint16_t val = readFromBuffer<uint16_t>(ptr);
            offset += sizeof(uint16_t);
            return Value(val);
        }
        case DataType::UInt32: {
            uint32_t val = readFromBuffer<uint32_t>(ptr);
            offset += sizeof(uint32_t);
            return Value(val);
        }
        case DataType::UInt64: {
            uint64_t val = readFromBuffer<uint64_t>(ptr);
            offset += sizeof(uint64_t);
            return Value(val);
        }
        case DataType::Float32: {
            float val = readFromBuffer<float>(ptr);
            offset += sizeof(float);
            return Value(val);
        }
        case DataType::Float64: {
            double val = readFromBuffer<double>(ptr);
            offset += sizeof(double);
            return Value(val);
        }
        case DataType::String: {
            uint32_t size = readFromBuffer<uint32_t>(ptr);
            offset += sizeof(uint32_t);
            std::string str(reinterpret_cast<const char*>(buffer + offset), size);
            offset += size;
            return Value(std::move(str));
        }
        case DataType::Blob: {
            uint32_t size = readFromBuffer<uint32_t>(ptr);
            offset += sizeof(uint32_t);
            std::vector<byte> blob(buffer + offset, buffer + offset + size);
            offset += size;
            return Value(std::move(blob));
        }
        case DataType::Vector: {
            uint32_t size = readFromBuffer<uint32_t>(ptr);
            offset += sizeof(uint32_t);
            std::vector<float> vec(size);
            std::memcpy(vec.data(), buffer + offset, size * sizeof(float));
            offset += size * sizeof(float);
            return Value(std::move(vec));
        }
        case DataType::Timestamp: {
            int64_t ts_value = readFromBuffer<int64_t>(ptr);
            offset += sizeof(int64_t);
            return Value(Timestamp(ts_value));
        }
        case DataType::Json:
            // Not implemented yet
            offset += sizeof(uint32_t);
            return Value();
    }
    return Value();
}

bool Value::operator<(const Value& other) const {
    // NULL is less than everything
    if (isNull()) return !other.isNull();
    if (other.isNull()) return false;

    // Same type comparison
    if (type() == other.type()) {
        return data_ < other.data_;
    }

    // Cross-type comparison based on type order
    return static_cast<uint8_t>(type()) < static_cast<uint8_t>(other.type());
}

std::string Value::toString() const {
    return std::visit(
        [](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return "NULL";
            } else if constexpr (std::is_same_v<T, bool>) {
                return v ? "true" : "false";
            } else if constexpr (std::is_same_v<T, std::string>) {
                return v;
            } else if constexpr (std::is_same_v<T, std::vector<byte>>) {
                return "<blob:" + std::to_string(v.size()) + " bytes>";
            } else if constexpr (std::is_same_v<T, std::vector<float>>) {
                return "<vector:" + std::to_string(v.size()) + " dims>";
            } else if constexpr (std::is_same_v<T, std::vector<std::pair<std::string, Value>>>) {
                return "<json>";
            } else if constexpr (std::is_same_v<T, Timestamp>) {
                return std::to_string(v.value);
            } else {
                return std::to_string(v);
            }
        },
        data_);
}

// Row implementation
size_t Row::serializedSize() const {
    size_t size = sizeof(uint32_t);  // Number of values
    for (const auto& value : values_) {
        size += value.serializedSize();
    }
    return size;
}

void Row::serialize(byte* buffer) const {
    writeToBuffer(buffer, static_cast<uint32_t>(values_.size()));
    for (const auto& value : values_) {
        value.serialize(buffer);
        buffer += value.serializedSize();
    }
}

Row Row::deserialize(const byte* buffer, size_t& offset) {
    const byte* ptr = buffer + offset;
    uint32_t count = readFromBuffer<uint32_t>(ptr);
    offset += sizeof(uint32_t);
    Row row;
    row.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        row[i] = Value::deserialize(buffer, offset);
    }
    return row;
}

}  // namespace lumen