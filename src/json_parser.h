#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <string>
#include <vector>
#include <map>
#include <memory>

enum class JSONType {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
};

struct JSONValue {
    JSONType type;
    bool bool_val;
    double num_val;
    std::string str_val;
    std::vector<std::shared_ptr<JSONValue>> arr_val;
    std::map<std::string, std::shared_ptr<JSONValue>> obj_val;

    JSONValue() : type(JSONType::JSON_NULL), bool_val(false), num_val(0.0) {}
};

class VFSJsonParser {
private:
    std::string source;
    size_t index;

    void skip_whitespace();
    char peek_char();
    char get_char();
    
    std::string parse_string_raw();
    std::shared_ptr<JSONValue> parse_value();
    std::shared_ptr<JSONValue> parse_number();
    std::shared_ptr<JSONValue> parse_string();
    std::shared_ptr<JSONValue> parse_array();
    std::shared_ptr<JSONValue> parse_object();
    std::shared_ptr<JSONValue> parse_literal();

public:
    VFSJsonParser() = default;
    
    std::shared_ptr<JSONValue> parse(const std::string& json_str);
};

#endif // JSON_PARSER_H
