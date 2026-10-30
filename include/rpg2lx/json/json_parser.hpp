#pragma once

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <cmath>
#include <limits>
#include <locale>

namespace rpg2lx {

enum class JsonType {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

class JsonValue {
public:
    JsonType type = JsonType::Null;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    std::vector<JsonValue> arr_val;
    std::map<std::string, JsonValue> obj_val;

    bool is_null() const { return type == JsonType::Null; }
    bool is_bool() const { return type == JsonType::Boolean; }
    bool is_number() const { return type == JsonType::Number; }
    bool is_string() const { return type == JsonType::String; }
    bool is_array() const { return type == JsonType::Array; }
    bool is_object() const { return type == JsonType::Object; }

    bool get_bool(bool default_val = false) const {
        return (type == JsonType::Boolean) ? bool_val : default_val;
    }

    int get_int(int default_val = 0) const {
        if (type != JsonType::Number || !std::isfinite(num_val) ||
            std::trunc(num_val) != num_val || num_val < std::numeric_limits<int>::min() ||
            num_val > std::numeric_limits<int>::max()) return default_val;
        return static_cast<int>(num_val);
    }

    double get_double(double default_val = 0.0) const {
        return (type == JsonType::Number) ? num_val : default_val;
    }

    std::string get_string(const std::string& default_val = "") const {
        return (type == JsonType::String) ? str_val : default_val;
    }

    const JsonValue* find(const std::string& key) const {
        if (type != JsonType::Object) return nullptr;
        auto it = obj_val.find(key);
        if (it != obj_val.end()) return &it->second;
        return nullptr;
    }

    bool has_key(const std::string& key) const {
        return find(key) != nullptr;
    }
};

class JsonParser {
public:
    static JsonValue parse(const std::string& source, std::string& error_msg) {
        error_msg.clear();
        // Reject pathological nesting before recursion. Strings do not add depth.
        unsigned depth = 0;
        bool quoted = false, escaped = false;
        for (unsigned char c : source) {
            if (quoted) {
                if (escaped) escaped = false;
                else if (c == '\\') escaped = true;
                else if (c == '"') quoted = false;
            } else if (c == '"') quoted = true;
            else if (c == '{' || c == '[') {
                if (++depth > 128) { error_msg = "JSON nesting exceeds 128 levels"; return {}; }
            } else if ((c == '}' || c == ']') && depth) --depth;
        }
        size_t idx = 0;
        if (source.compare(0, 3, "\xEF\xBB\xBF") == 0) idx = 3;
        skip_whitespace(source, idx);
        if (idx >= source.size()) {
            error_msg = "Empty JSON input";
            return JsonValue();
        }

        try {
            JsonValue val = parse_value(source, idx);
            skip_whitespace(source, idx);
            if (idx < source.size()) {
                error_msg = "Unexpected trailing characters after JSON root at offset " + std::to_string(idx);
                return JsonValue();
            }
            return val;
        } catch (const std::exception& e) {
            error_msg = e.what();
            return JsonValue();
        }
    }

private:
    static void skip_whitespace(const std::string& s, size_t& idx) {
        while (idx < s.size() && (s[idx] == ' ' || s[idx] == '\t' || s[idx] == '\r' || s[idx] == '\n')) {
            idx++;
        }
    }

    static JsonValue parse_value(const std::string& s, size_t& idx) {
        skip_whitespace(s, idx);
        if (idx >= s.size()) {
            throw std::runtime_error("Unexpected end of input while parsing value");
        }

        char c = s[idx];
        if (c == '{') return parse_object(s, idx);
        if (c == '[') return parse_array(s, idx);
        if (c == '"') return parse_string(s, idx);
        if (c == 't' || c == 'f') return parse_boolean(s, idx);
        if (c == 'n') return parse_null(s, idx);
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number(s, idx);

        throw std::runtime_error("Unexpected character '" + std::string(1, c) + "' at offset " + std::to_string(idx));
    }

    static JsonValue parse_object(const std::string& s, size_t& idx) {
        JsonValue val;
        val.type = JsonType::Object;
        idx++; // skip '{'
        skip_whitespace(s, idx);

        if (idx < s.size() && s[idx] == '}') {
            idx++;
            return val;
        }

        while (idx < s.size()) {
            skip_whitespace(s, idx);
            if (idx >= s.size() || s[idx] != '"') {
                throw std::runtime_error("Expected string key in object at offset " + std::to_string(idx));
            }
            JsonValue key_val = parse_string(s, idx);
            std::string key = key_val.str_val;

            skip_whitespace(s, idx);
            if (idx >= s.size() || s[idx] != ':') {
                throw std::runtime_error("Expected ':' after key in object at offset " + std::to_string(idx));
            }
            idx++; // skip ':'

            JsonValue child = parse_value(s, idx);
            if (!val.obj_val.emplace(key, std::move(child)).second)
                throw std::runtime_error("Duplicate object key: " + key);

            skip_whitespace(s, idx);
            if (idx >= s.size()) {
                throw std::runtime_error("Unterminated object at offset " + std::to_string(idx));
            }
            if (s[idx] == '}') {
                idx++;
                return val;
            }
            if (s[idx] == ',') {
                idx++;
                continue;
            }
            throw std::runtime_error("Expected ',' or '}' in object at offset " + std::to_string(idx));
        }

        throw std::runtime_error("Unterminated object at end of input");
    }

    static JsonValue parse_array(const std::string& s, size_t& idx) {
        JsonValue val;
        val.type = JsonType::Array;
        idx++; // skip '['
        skip_whitespace(s, idx);

        if (idx < s.size() && s[idx] == ']') {
            idx++;
            return val;
        }

        while (idx < s.size()) {
            val.arr_val.push_back(parse_value(s, idx));
            skip_whitespace(s, idx);
            if (idx >= s.size()) {
                throw std::runtime_error("Unterminated array at offset " + std::to_string(idx));
            }
            if (s[idx] == ']') {
                idx++;
                return val;
            }
            if (s[idx] == ',') {
                idx++;
                continue;
            }
            throw std::runtime_error("Expected ',' or ']' in array at offset " + std::to_string(idx));
        }

        throw std::runtime_error("Unterminated array at end of input");
    }

    static JsonValue parse_string(const std::string& s, size_t& idx) {
        JsonValue val;
        val.type = JsonType::String;
        idx++; // skip opening '"'

        std::string result;
        while (idx < s.size()) {
            char c = s[idx++];
            if (c == '"') {
                val.str_val = result;
                return val;
            }
            if (c == '\\') {
                if (idx >= s.size()) {
                    throw std::runtime_error("Unterminated escape sequence in string");
                }
                char esc = s[idx++];
                switch (esc) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        if (idx + 4 > s.size()) {
                            throw std::runtime_error("Invalid unicode escape in string");
                        }
                        unsigned cp = hex4(s, idx);
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            if (s.compare(idx, 2, "\\u") != 0)
                                throw std::runtime_error("Missing low surrogate");
                            idx += 2;
                            unsigned low = hex4(s, idx);
                            if (low < 0xDC00 || low > 0xDFFF)
                                throw std::runtime_error("Invalid low surrogate");
                            cp = 0x10000 + ((cp - 0xD800) << 10) + low - 0xDC00;
                        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                            throw std::runtime_error("Unpaired low surrogate");
                        }
                        utf8(result, cp);
                        break;
                    }
                    default:
                        throw std::runtime_error("Invalid string escape");
                }
            } else {
                const auto byte = static_cast<unsigned char>(c);
                if (byte < 0x20) throw std::runtime_error("Unescaped control character");
                if (byte < 0x80) result += c;
                else {
                    unsigned cp = 0, minimum = 0, remaining = 0;
                    if (byte >= 0xC2 && byte <= 0xDF) { cp = byte & 31; minimum = 0x80; remaining = 1; }
                    else if (byte >= 0xE0 && byte <= 0xEF) { cp = byte & 15; minimum = 0x800; remaining = 2; }
                    else if (byte >= 0xF0 && byte <= 0xF4) { cp = byte & 7; minimum = 0x10000; remaining = 3; }
                    else throw std::runtime_error("Invalid UTF-8 leading byte");
                    for (unsigned n = 0; n < remaining; ++n) {
                        if (idx == s.size()) throw std::runtime_error("Truncated UTF-8");
                        auto next = static_cast<unsigned char>(s[idx++]);
                        if ((next & 0xC0) != 0x80) throw std::runtime_error("Invalid UTF-8 continuation");
                        cp = (cp << 6) | (next & 63);
                    }
                    if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
                        throw std::runtime_error("Invalid UTF-8 code point");
                    utf8(result, cp);
                }
            }
        }

        throw std::runtime_error("Unterminated string literal");
    }

    static JsonValue parse_number(const std::string& s, size_t& idx) {
        JsonValue val;
        val.type = JsonType::Number;
        size_t start = idx;

        if (s[idx] == '-') idx++;
        auto digit = [&] { return idx < s.size() && s[idx] >= '0' && s[idx] <= '9'; };
        if (!digit()) throw std::runtime_error("Expected number digit");
        if (s[idx] == '0') ++idx;
        else while (digit()) ++idx;
        if (idx < s.size() && s[idx] == '.') {
            idx++;
            if (!digit()) throw std::runtime_error("Expected fraction digit");
            while (digit()) ++idx;
        }
        if (idx < s.size() && (s[idx] == 'e' || s[idx] == 'E')) {
            idx++;
            if (idx < s.size() && (s[idx] == '+' || s[idx] == '-')) idx++;
            if (!digit()) throw std::runtime_error("Expected exponent digit");
            while (digit()) ++idx;
        }

        std::string num_str = s.substr(start, idx - start);
        try {
            std::istringstream stream(num_str);
            stream.imbue(std::locale::classic());
            stream >> val.num_val;
            if (stream.fail() || !std::isfinite(val.num_val))
                throw std::runtime_error("Number outside finite range");
        } catch (...) {
            throw std::runtime_error("Invalid number literal: " + num_str);
        }
        return val;
    }

    static unsigned hex4(const std::string& s, size_t& idx) {
        unsigned value = 0;
        for (unsigned n = 0; n < 4; ++n) {
            if (idx == s.size()) throw std::runtime_error("Truncated unicode escape");
            unsigned char c = s[idx++];
            unsigned digit;
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else throw std::runtime_error("Invalid unicode hex digit");
            value = value * 16 + digit;
        }
        return value;
    }

    static void utf8(std::string& out, unsigned cp) {
        if (cp <= 0x7F) out += static_cast<char>(cp);
        else if (cp <= 0x7FF) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 63));
        } else if (cp <= 0xFFFF) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 63));
            out += static_cast<char>(0x80 | (cp & 63));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 63));
            out += static_cast<char>(0x80 | ((cp >> 6) & 63));
            out += static_cast<char>(0x80 | (cp & 63));
        }
    }

    static JsonValue parse_boolean(const std::string& s, size_t& idx) {
        JsonValue val;
        val.type = JsonType::Boolean;
        if (s.compare(idx, 4, "true") == 0) {
            val.bool_val = true;
            idx += 4;
            return val;
        }
        if (s.compare(idx, 5, "false") == 0) {
            val.bool_val = false;
            idx += 5;
            return val;
        }
        throw std::runtime_error("Invalid boolean token at offset " + std::to_string(idx));
    }

    static JsonValue parse_null(const std::string& s, size_t& idx) {
        JsonValue val;
        val.type = JsonType::Null;
        if (s.compare(idx, 4, "null") == 0) {
            idx += 4;
            return val;
        }
        throw std::runtime_error("Invalid null token at offset " + std::to_string(idx));
    }
};

} // namespace rpg2lx
