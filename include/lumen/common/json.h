#ifndef LUMEN_COMMON_JSON_H
#define LUMEN_COMMON_JSON_H

#include <lumen/types.h>
#include <string>
#include <stdexcept>
#include <sstream>
#include <cctype>

namespace lumen {
namespace json {

// JSON parsing error
class JsonParseError : public std::runtime_error {
public:
    JsonParseError(const std::string& msg, size_t position)
        : std::runtime_error("JSON parse error at position " + std::to_string(position) + ": " + msg),
          position_(position) {}
    
    size_t position() const { return position_; }
    
private:
    size_t position_;
};

// Parse JSON string to Value
Value parse(const std::string& json_str);

// Serialize Value to JSON string
std::string stringify(const Value& value, bool pretty = false, int indent_level = 0);

// Implementation details
namespace detail {

class JsonParser {
public:
    explicit JsonParser(const std::string& input) 
        : input_(input), pos_(0) {}
    
    Value parse() {
        skip_whitespace();
        Value result = parse_value();
        skip_whitespace();
        
        if (pos_ < input_.length()) {
            throw JsonParseError("Unexpected characters after JSON value", pos_);
        }
        
        return result;
    }
    
private:
    const std::string& input_;
    size_t pos_;
    
    void skip_whitespace() {
        while (pos_ < input_.length() && std::isspace(input_[pos_])) {
            pos_++;
        }
    }
    
    char peek() const {
        if (pos_ >= input_.length()) {
            throw JsonParseError("Unexpected end of input", pos_);
        }
        return input_[pos_];
    }
    
    char consume() {
        char c = peek();
        pos_++;
        return c;
    }
    
    bool consume_if(char expected) {
        if (pos_ < input_.length() && input_[pos_] == expected) {
            pos_++;
            return true;
        }
        return false;
    }
    
    void expect(char expected) {
        if (!consume_if(expected)) {
            throw JsonParseError(std::string("Expected '") + expected + "'", pos_);
        }
    }
    
    Value parse_value() {
        skip_whitespace();
        
        char c = peek();
        
        switch (c) {
            case 'n': return parse_null();
            case 't': return parse_true();
            case 'f': return parse_false();
            case '"': return parse_string();
            case '[': return parse_array();
            case '{': return parse_object();
            case '-':
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                return parse_number();
            default:
                throw JsonParseError("Unexpected character", pos_);
        }
    }
    
    Value parse_null() {
        if (input_.substr(pos_, 4) == "null") {
            pos_ += 4;
            return Value();
        }
        throw JsonParseError("Invalid null value", pos_);
    }
    
    Value parse_true() {
        if (input_.substr(pos_, 4) == "true") {
            pos_ += 4;
            return Value(true);
        }
        throw JsonParseError("Invalid true value", pos_);
    }
    
    Value parse_false() {
        if (input_.substr(pos_, 5) == "false") {
            pos_ += 5;
            return Value(false);
        }
        throw JsonParseError("Invalid false value", pos_);
    }
    
    Value parse_string() {
        expect('"');
        
        std::string result;
        while (pos_ < input_.length() && input_[pos_] != '"') {
            if (input_[pos_] == '\\') {
                pos_++;
                if (pos_ >= input_.length()) {
                    throw JsonParseError("Unexpected end in string escape", pos_);
                }
                
                switch (input_[pos_]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        // Unicode escape (simplified - only basic plane)
                        if (pos_ + 4 >= input_.length()) {
                            throw JsonParseError("Invalid unicode escape", pos_);
                        }
                        pos_++; // Skip 'u'
                        int codepoint = 0;
                        for (int i = 0; i < 4; i++) {
                            char hex = input_[pos_ + i];
                            if (hex >= '0' && hex <= '9') {
                                codepoint = codepoint * 16 + (hex - '0');
                            } else if (hex >= 'a' && hex <= 'f') {
                                codepoint = codepoint * 16 + (hex - 'a' + 10);
                            } else if (hex >= 'A' && hex <= 'F') {
                                codepoint = codepoint * 16 + (hex - 'A' + 10);
                            } else {
                                throw JsonParseError("Invalid unicode escape", pos_);
                            }
                        }
                        pos_ += 3; // Will be incremented by 1 more at the end
                        if (codepoint < 128) {
                            result += static_cast<char>(codepoint);
                        } else {
                            // For simplicity, we only support ASCII
                            result += '?';
                        }
                        break;
                    }
                    default:
                        throw JsonParseError("Invalid escape sequence", pos_);
                }
                pos_++;
            } else {
                result += input_[pos_];
                pos_++;
            }
        }
        
        expect('"');
        return Value(result);
    }
    
    Value parse_number() {
        size_t start = pos_;
        bool has_minus = consume_if('-');
        
        // Parse integer part
        if (peek() == '0') {
            consume();
        } else if (peek() >= '1' && peek() <= '9') {
            while (pos_ < input_.length() && input_[pos_] >= '0' && input_[pos_] <= '9') {
                pos_++;
            }
        } else {
            throw JsonParseError("Invalid number", pos_);
        }
        
        // Check for decimal part
        bool has_decimal = false;
        if (pos_ < input_.length() && input_[pos_] == '.') {
            has_decimal = true;
            pos_++;
            
            if (pos_ >= input_.length() || input_[pos_] < '0' || input_[pos_] > '9') {
                throw JsonParseError("Invalid decimal number", pos_);
            }
            
            while (pos_ < input_.length() && input_[pos_] >= '0' && input_[pos_] <= '9') {
                pos_++;
            }
        }
        
        // Check for exponent
        if (pos_ < input_.length() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
            has_decimal = true;
            pos_++;
            
            if (pos_ < input_.length() && (input_[pos_] == '+' || input_[pos_] == '-')) {
                pos_++;
            }
            
            if (pos_ >= input_.length() || input_[pos_] < '0' || input_[pos_] > '9') {
                throw JsonParseError("Invalid exponent", pos_);
            }
            
            while (pos_ < input_.length() && input_[pos_] >= '0' && input_[pos_] <= '9') {
                pos_++;
            }
        }
        
        std::string num_str = input_.substr(start, pos_ - start);
        
        if (has_decimal) {
            return Value(std::stod(num_str));
        } else {
            int64_t val = std::stoll(num_str);
            // Try to fit in smaller integer types
            if (val >= INT32_MIN && val <= INT32_MAX) {
                return Value(static_cast<int32_t>(val));
            } else {
                return Value(val);
            }
        }
    }
    
    Value parse_array() {
        expect('[');
        skip_whitespace();
        
        // For now, we'll parse arrays as JSON objects with numeric keys
        // This is a limitation of our current Value type
        std::vector<std::pair<std::string, Value>> array_obj;
        
        size_t index = 0;
        while (pos_ < input_.length() && peek() != ']') {
            if (index > 0) {
                expect(',');
                skip_whitespace();
            }
            
            Value element = parse_value();
            array_obj.push_back({std::to_string(index), element});
            index++;
            
            skip_whitespace();
        }
        
        expect(']');
        return Value(std::move(array_obj));
    }
    
    Value parse_object() {
        expect('{');
        skip_whitespace();
        
        std::vector<std::pair<std::string, Value>> obj;
        
        while (pos_ < input_.length() && peek() != '}') {
            if (!obj.empty()) {
                expect(',');
                skip_whitespace();
            }
            
            // Parse key
            if (peek() != '"') {
                throw JsonParseError("Expected string key", pos_);
            }
            Value key_val = parse_string();
            std::string key = key_val.asString();
            
            skip_whitespace();
            expect(':');
            skip_whitespace();
            
            // Parse value
            Value value = parse_value();
            
            obj.push_back({key, value});
            
            skip_whitespace();
        }
        
        expect('}');
        return Value(std::move(obj));
    }
};

class JsonStringifier {
public:
    static std::string stringify(const Value& value, bool pretty, int indent_level) {
        std::ostringstream oss;
        stringify_value(oss, value, pretty, indent_level);
        return oss.str();
    }
    
private:
    static void write_indent(std::ostream& os, bool pretty, int level) {
        if (pretty) {
            os << '\n';
            for (int i = 0; i < level * 2; i++) {
                os << ' ';
            }
        }
    }
    
    static void escape_string(std::ostream& os, const std::string& str) {
        os << '"';
        for (char c : str) {
            switch (c) {
                case '"': os << "\\\""; break;
                case '\\': os << "\\\\"; break;
                case '\b': os << "\\b"; break;
                case '\f': os << "\\f"; break;
                case '\n': os << "\\n"; break;
                case '\r': os << "\\r"; break;
                case '\t': os << "\\t"; break;
                default:
                    if (c >= 32 && c <= 126) {
                        os << c;
                    } else {
                        // Non-printable characters
                        os << "\\u00" << std::hex 
                           << static_cast<int>(static_cast<unsigned char>(c) >> 4)
                           << static_cast<int>(static_cast<unsigned char>(c) & 0xF)
                           << std::dec;
                    }
            }
        }
        os << '"';
    }
    
    static void stringify_value(std::ostream& os, const Value& value, bool pretty, int indent_level) {
        if (value.isNull()) {
            os << "null";
        } else if (value.isBool()) {
            os << (value.asBool() ? "true" : "false");
        } else if (value.isInt()) {
            os << value.asInt();
        } else if (value.isUInt()) {
            os << value.asUInt();
        } else if (value.isFloat()) {
            os << value.asFloat();
        } else if (value.isString()) {
            escape_string(os, value.asString());
        } else if (value.isJson()) {
            const auto& obj = value.asJson();
            
            // Check if it's an array (all keys are sequential numbers starting from 0)
            bool is_array = true;
            for (size_t i = 0; i < obj.size(); i++) {
                if (obj[i].first != std::to_string(i)) {
                    is_array = false;
                    break;
                }
            }
            
            if (is_array) {
                os << '[';
                for (size_t i = 0; i < obj.size(); i++) {
                    if (i > 0) {
                        os << ',';
                        if (pretty) os << ' ';
                    }
                    if (pretty && obj[i].second.isJson()) {
                        write_indent(os, pretty, indent_level + 1);
                    }
                    stringify_value(os, obj[i].second, pretty, indent_level + 1);
                }
                if (pretty && !obj.empty() && obj.back().second.isJson()) {
                    write_indent(os, pretty, indent_level);
                }
                os << ']';
            } else {
                os << '{';
                bool first = true;
                for (const auto& [key, val] : obj) {
                    if (!first) {
                        os << ',';
                    }
                    write_indent(os, pretty, indent_level + 1);
                    escape_string(os, key);
                    os << ':';
                    if (pretty) os << ' ';
                    stringify_value(os, val, pretty, indent_level + 1);
                    first = false;
                }
                if (pretty && !obj.empty()) {
                    write_indent(os, pretty, indent_level);
                }
                os << '}';
            }
        } else {
            // Handle other types as strings
            escape_string(os, value.toString());
        }
    }
};

} // namespace detail

inline Value parse(const std::string& json_str) {
    detail::JsonParser parser(json_str);
    return parser.parse();
}

inline std::string stringify(const Value& value, bool pretty, int indent_level) {
    return detail::JsonStringifier::stringify(value, pretty, indent_level);
}

} // namespace json
} // namespace lumen

#endif // LUMEN_COMMON_JSON_H