// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <stdexcept>
#include <cstdint>
#include <iomanip>

namespace kick::clap::json {

enum class Type {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

class Value {
public:
    Value() : type_(Type::Null) {}
    Value(std::nullptr_t) : type_(Type::Null) {}
    Value(bool b) : type_(Type::Boolean), bool_val_(b) {}
    Value(int n) : type_(Type::Number), num_val_(n), is_int_(true), int_val_(n) {}
    Value(int64_t n) : type_(Type::Number), num_val_(static_cast<double>(n)), is_int_(true), int_val_(n) {}
    Value(uint64_t n) : type_(Type::Number), num_val_(static_cast<double>(n)), is_int_(true), int_val_(static_cast<int64_t>(n)) {}
    Value(double d) : type_(Type::Number), num_val_(d), is_int_(false), int_val_(0) {}
    Value(const char* s) : type_(Type::String), str_val_(s ? s : "") {}
    Value(std::string s) : type_(Type::String), str_val_(std::move(s)) {}
    Value(std::vector<Value> arr) : type_(Type::Array), arr_val_(std::move(arr)) {}
    Value(std::map<std::string, Value> obj) : type_(Type::Object), obj_val_(std::move(obj)) {}

    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }
    bool is_bool() const { return type_ == Type::Boolean; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

    bool as_bool(bool defaultVal = false) const {
        return (type_ == Type::Boolean) ? bool_val_ : defaultVal;
    }

    double as_double(double defaultVal = 0.0) const {
        return (type_ == Type::Number) ? num_val_ : defaultVal;
    }

    int64_t as_int(int64_t defaultVal = 0) const {
        if (type_ == Type::Number) {
            return is_int_ ? int_val_ : static_cast<int64_t>(num_val_);
        }
        return defaultVal;
    }

    const std::string& as_string() const {
        static const std::string empty;
        return (type_ == Type::String) ? str_val_ : empty;
    }

    std::string get_string_or(const std::string& defaultVal) const {
        return (type_ == Type::String) ? str_val_ : defaultVal;
    }

    const std::vector<Value>& as_array() const {
        static const std::vector<Value> empty;
        return (type_ == Type::Array) ? arr_val_ : empty;
    }

    std::vector<Value>& as_array_mut() {
        if (type_ != Type::Array) {
            type_ = Type::Array;
            arr_val_.clear();
        }
        return arr_val_;
    }

    const std::map<std::string, Value>& as_object() const {
        static const std::map<std::string, Value> empty;
        return (type_ == Type::Object) ? obj_val_ : empty;
    }

    std::map<std::string, Value>& as_object_mut() {
        if (type_ != Type::Object) {
            type_ = Type::Object;
            obj_val_.clear();
        }
        return obj_val_;
    }

    bool has(const std::string& key) const {
        if (type_ != Type::Object) return false;
        return obj_val_.find(key) != obj_val_.end();
    }

    const Value& get(const std::string& key) const {
        static const Value nullVal;
        if (type_ != Type::Object) return nullVal;
        auto it = obj_val_.find(key);
        return (it != obj_val_.end()) ? it->second : nullVal;
    }

    void set(const std::string& key, Value val) {
        if (type_ != Type::Object) {
            type_ = Type::Object;
            obj_val_.clear();
        }
        obj_val_[key] = std::move(val);
    }

    void push_back(Value val) {
        if (type_ != Type::Array) {
            type_ = Type::Array;
            arr_val_.clear();
        }
        arr_val_.push_back(std::move(val));
    }

    std::string serialize(bool canonical = true, int indent = 0) const {
        std::ostringstream ss;
        serialize_internal(ss, canonical, indent, 0);
        return ss.str();
    }

    static bool parse(const std::string& str, Value& out, std::string& error) {
        size_t pos = 0;
        skip_ws(str, pos);
        if (pos >= str.size()) {
            error = "Empty JSON input";
            return false;
        }
        try {
            out = parse_value(str, pos);
            skip_ws(str, pos);
            if (pos < str.size()) {
                error = "Trailing characters after JSON value at position " + std::to_string(pos);
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }

private:
    Type type_{Type::Null};
    bool bool_val_{false};
    double num_val_{0.0};
    bool is_int_{false};
    int64_t int_val_{0};
    std::string str_val_;
    std::vector<Value> arr_val_;
    std::map<std::string, Value> obj_val_;

    static void skip_ws(const std::string& s, size_t& pos) {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) {
            ++pos;
        }
    }

    static Value parse_value(const std::string& s, size_t& pos) {
        skip_ws(s, pos);
        if (pos >= s.size()) throw std::runtime_error("Unexpected end of JSON input");

        char c = s[pos];
        if (c == 'n') return parse_null(s, pos);
        if (c == 't' || c == 'f') return parse_bool(s, pos);
        if (c == '"') return parse_string(s, pos);
        if (c == '[') return parse_array(s, pos);
        if (c == '{') return parse_object(s, pos);
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number(s, pos);

        throw std::runtime_error(std::string("Unexpected token '") + c + "' at position " + std::to_string(pos));
    }

    static Value parse_null(const std::string& s, size_t& pos) {
        if (s.substr(pos, 4) == "null") {
            pos += 4;
            return Value();
        }
        throw std::runtime_error("Expected 'null' at position " + std::to_string(pos));
    }

    static Value parse_bool(const std::string& s, size_t& pos) {
        if (s.substr(pos, 4) == "true") {
            pos += 4;
            return Value(true);
        }
        if (s.substr(pos, 5) == "false") {
            pos += 5;
            return Value(false);
        }
        throw std::runtime_error("Expected boolean at position " + std::to_string(pos));
    }

    static Value parse_number(const std::string& s, size_t& pos) {
        size_t start = pos;
        bool isFloat = false;
        if (s[pos] == '-') ++pos;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos;
        if (pos < s.size() && s[pos] == '.') {
            isFloat = true;
            ++pos;
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos;
        }
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
            isFloat = true;
            ++pos;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) ++pos;
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos;
        }

        std::string numStr = s.substr(start, pos - start);
        if (isFloat) {
            double d = std::stod(numStr);
            return Value(d);
        } else {
            int64_t n = std::stoll(numStr);
            return Value(n);
        }
    }

    static std::string parse_string_raw(const std::string& s, size_t& pos) {
        if (s[pos] != '"') throw std::runtime_error("Expected '\"' at position " + std::to_string(pos));
        ++pos;
        std::string result;
        while (pos < s.size()) {
            char c = s[pos++];
            if (c == '"') return result;
            if (c == '\\') {
                if (pos >= s.size()) throw std::runtime_error("Unterminated escape sequence");
                char esc = s[pos++];
                switch (esc) {
                    case '"': result.push_back('"'); break;
                    case '\\': result.push_back('\\'); break;
                    case '/': result.push_back('/'); break;
                    case 'b': result.push_back('\b'); break;
                    case 'f': result.push_back('\f'); break;
                    case 'n': result.push_back('\n'); break;
                    case 'r': result.push_back('\r'); break;
                    case 't': result.push_back('\t'); break;
                    case 'u': {
                        if (pos + 4 > s.size()) throw std::runtime_error("Invalid unicode escape");
                        std::string hex = s.substr(pos, 4);
                        pos += 4;
                        unsigned long code = std::stoul(hex, nullptr, 16);
                        if (code < 0x80) {
                            result.push_back(static_cast<char>(code));
                        } else if (code < 0x800) {
                            result.push_back(static_cast<char>(0xC0 | (code >> 6)));
                            result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        } else {
                            result.push_back(static_cast<char>(0xE0 | (code >> 12)));
                            result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                            result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        }
                        break;
                    }
                    default: result.push_back(esc); break;
                }
            } else {
                result.push_back(c);
            }
        }
        throw std::runtime_error("Unterminated string");
    }

    static Value parse_string(const std::string& s, size_t& pos) {
        return Value(parse_string_raw(s, pos));
    }

    static Value parse_array(const std::string& s, size_t& pos) {
        if (s[pos] != '[') throw std::runtime_error("Expected '['");
        ++pos;
        std::vector<Value> arr;
        skip_ws(s, pos);
        if (pos < s.size() && s[pos] == ']') {
            ++pos;
            return Value(arr);
        }
        while (pos < s.size()) {
            arr.push_back(parse_value(s, pos));
            skip_ws(s, pos);
            if (pos >= s.size()) throw std::runtime_error("Unterminated array");
            if (s[pos] == ']') {
                ++pos;
                return Value(arr);
            }
            if (s[pos] == ',') {
                ++pos;
                continue;
            }
            throw std::runtime_error(std::string("Expected ',' or ']' in array, got '") + s[pos] + "'");
        }
        throw std::runtime_error("Unterminated array");
    }

    static Value parse_object(const std::string& s, size_t& pos) {
        if (s[pos] != '{') throw std::runtime_error("Expected '{'");
        ++pos;
        std::map<std::string, Value> obj;
        skip_ws(s, pos);
        if (pos < s.size() && s[pos] == '}') {
            ++pos;
            return Value(obj);
        }
        while (pos < s.size()) {
            skip_ws(s, pos);
            if (pos >= s.size() || s[pos] != '"') throw std::runtime_error("Expected object key string");
            std::string key = parse_string_raw(s, pos);
            skip_ws(s, pos);
            if (pos >= s.size() || s[pos] != ':') throw std::runtime_error("Expected ':' after object key");
            ++pos;
            Value val = parse_value(s, pos);
            obj[std::move(key)] = std::move(val);
            skip_ws(s, pos);
            if (pos >= s.size()) throw std::runtime_error("Unterminated object");
            if (s[pos] == '}') {
                ++pos;
                return Value(obj);
            }
            if (s[pos] == ',') {
                ++pos;
                continue;
            }
            throw std::runtime_error(std::string("Expected ',' or '}' in object, got '") + s[pos] + "'");
        }
        throw std::runtime_error("Unterminated object");
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
                    if (static_cast<unsigned char>(c) < 0x20) {
                        os << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)(unsigned char)c;
                    } else {
                        os << c;
                    }
                    break;
            }
        }
        os << '"';
    }

    void serialize_internal(std::ostream& os, bool canonical, int indent, int currentIndent) const {
        switch (type_) {
            case Type::Null: os << "null"; break;
            case Type::Boolean: os << (bool_val_ ? "true" : "false"); break;
            case Type::Number:
                if (is_int_) os << int_val_;
                else os << num_val_;
                break;
            case Type::String: escape_string(os, str_val_); break;
            case Type::Array: {
                os << '[';
                bool first = true;
                for (const auto& item : arr_val_) {
                    if (!first) {
                        os << ',';
                        if (indent > 0) os << ' ';
                    }
                    first = false;
                    item.serialize_internal(os, canonical, indent, currentIndent + indent);
                }
                os << ']';
                break;
            }
            case Type::Object: {
                os << '{';
                bool first = true;
                // std::map automatically iterates in alphabetical order (canonical!)
                for (const auto& pair : obj_val_) {
                    if (!first) {
                        os << ',';
                        if (indent > 0) os << "\n" << std::string(currentIndent + indent, ' ');
                    } else if (indent > 0 && !obj_val_.empty()) {
                        os << "\n" << std::string(currentIndent + indent, ' ');
                    }
                    first = false;
                    escape_string(os, pair.first);
                    os << ':';
                    if (indent > 0) os << ' ';
                    pair.second.serialize_internal(os, canonical, indent, currentIndent + indent);
                }
                if (indent > 0 && !obj_val_.empty()) {
                    os << "\n" << std::string(currentIndent, ' ');
                }
                os << '}';
                break;
            }
        }
    }
};

} // namespace kick::clap::json
