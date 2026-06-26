#include "json_parser.h"
#include "logger.h"
#include <stdexcept>
#include <cctype>

void VFSJsonParser::skip_whitespace() {
    while (index < source.size() && (source[index] == ' ' || source[index] == '\t' || source[index] == '\n' || source[index] == '\r')) {
        index++;
    }
}

char VFSJsonParser::peek_char() {
    skip_whitespace();
    if (index >= source.size()) return '\0';
    return source[index];
}

char VFSJsonParser::get_char() {
    skip_whitespace();
    if (index >= source.size()) return '\0';
    return source[index++];
}

std::string VFSJsonParser::parse_string_raw() {
    if (get_char() != '"') {
        throw std::runtime_error("Expected opening double quote");
    }
    
    std::string result;
    while (index < source.size()) {
        char c = source[index++];
        if (c == '"') {
            return result;
        } else if (c == '\\') {
            if (index >= source.size()) throw std::runtime_error("Unexpected EOF inside escape sequence");
            char escape = source[index++];
            switch (escape) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                default: result += escape; break;
            }
        } else {
            result += c;
        }
    }
    throw std::runtime_error("Unterminated JSON string");
}

std::shared_ptr<JSONValue> VFSJsonParser::parse_value() {
    char c = peek_char();
    if (c == '\0') {
        return nullptr;
    }
    
    if (c == '{') return parse_object();
    if (c == '[') return parse_array();
    if (c == '"') return parse_string();
    if (c == '-' || std::isdigit(c)) return parse_number();
    if (c == 't' || c == 'f' || c == 'n') return parse_literal();
    
    throw std::runtime_error(std::string("Unexpected token: ") + c);
}

std::shared_ptr<JSONValue> VFSJsonParser::parse_literal() {
    char c = peek_char();
    auto val = std::make_shared<JSONValue>();
    
    if (c == 't') {
        if (source.substr(index, 4) == "true") {
            index += 4;
            val->type = JSONType::JSON_BOOL;
            val->bool_val = true;
            return val;
        }
    } else if (c == 'f') {
        if (source.substr(index, 5) == "false") {
            index += 5;
            val->type = JSONType::JSON_BOOL;
            val->bool_val = false;
            return val;
        }
    } else if (c == 'n') {
        if (source.substr(index, 4) == "null") {
            index += 4;
            val->type = JSONType::JSON_NULL;
            return val;
        }
    }
    
    throw std::runtime_error("Failed to parse boolean or null literal");
}

std::shared_ptr<JSONValue> VFSJsonParser::parse_number() {
    skip_whitespace();
    size_t start = index;
    if (source[index] == '-') index++;
    
    while (index < source.size() && (std::isdigit(source[index]) || source[index] == '.' || source[index] == 'e' || source[index] == 'E' || source[index] == '+' || source[index] == '-')) {
        index++;
    }
    
    std::string num_str = source.substr(start, index - start);
    auto val = std::make_shared<JSONValue>();
    val->type = JSONType::JSON_NUMBER;
    val->num_val = std::stod(num_str);
    return val;
}

std::shared_ptr<JSONValue> VFSJsonParser::parse_string() {
    auto val = std::make_shared<JSONValue>();
    val->type = JSONType::JSON_STRING;
    val->str_val = parse_string_raw();
    return val;
}

std::shared_ptr<JSONValue> VFSJsonParser::parse_array() {
    if (get_char() != '[') {
        throw std::runtime_error("Expected array start");
    }
    
    auto val = std::make_shared<JSONValue>();
    val->type = JSONType::JSON_ARRAY;
    
    if (peek_char() == ']') {
        get_char();
        return val;
    }
    
    while (true) {
        val->arr_val.push_back(parse_value());
        char next = peek_char();
        if (next == ']') {
            get_char();
            break;
        } else if (next == ',') {
            get_char();
        } else {
            throw std::runtime_error("Expected comma or array end symbol");
        }
    }
    return val;
}

std::shared_ptr<JSONValue> VFSJsonParser::parse_object() {
    if (get_char() != '{') {
        throw std::runtime_error("Expected object start token");
    }
    
    auto val = std::make_shared<JSONValue>();
    val->type = JSONType::JSON_OBJECT;
    
    if (peek_char() == '}') {
        get_char();
        return val;
    }
    
    while (true) {
        std::string key = parse_string_raw();
        if (get_char() != ':') {
            throw std::runtime_error("Expected colon after object key mapping");
        }
        val->obj_val[key] = parse_value();
        
        char next = peek_char();
        if (next == '}') {
            get_char();
            break;
        } else if (next == ',') {
            get_char();
        } else {
            throw std::runtime_error("Expected comma or object end token");
        }
    }
    return val;
}

std::shared_ptr<JSONValue> VFSJsonParser::parse(const std::string& json_str) {
    source = json_str;
    index = 0;
    
    try {
        return parse_value();
    } catch (const std::exception& e) {
        VFSLogger::get_instance().error("VFSJsonParser", std::string("Parser error: ") + e.what());
        return nullptr;
    }
}
