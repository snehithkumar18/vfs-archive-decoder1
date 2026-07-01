#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace PixelForge {

// Forward declaration for recursive variant
struct ManifestValue;

// Type aliases for the variant members
using ManifestNull   = std::nullptr_t;
using ManifestBool   = bool;
using ManifestNumber = double;
using ManifestString = std::string;
using ManifestArray  = std::vector<ManifestValue>;
using ManifestObject = std::map<std::string, ManifestValue>;

// A variant type that can represent any JSON-like value:
// null, boolean, number (double), string, array, or object.
struct ManifestValue {
    std::variant<ManifestNull,
                 ManifestBool,
                 ManifestNumber,
                 ManifestString,
                 ManifestArray,
                 ManifestObject> data;

    ManifestValue() : data(nullptr) {}
    ManifestValue(std::nullptr_t) : data(nullptr) {}
    ManifestValue(bool v) : data(v) {}
    ManifestValue(double v) : data(v) {}
    ManifestValue(int v) : data(static_cast<double>(v)) {}
    ManifestValue(const std::string& v) : data(v) {}
    ManifestValue(std::string&& v) : data(std::move(v)) {}
    ManifestValue(const char* v) : data(std::string(v)) {}
    ManifestValue(const ManifestArray& v) : data(v) {}
    ManifestValue(ManifestArray&& v) : data(std::move(v)) {}
    ManifestValue(const ManifestObject& v) : data(v) {}
    ManifestValue(ManifestObject&& v) : data(std::move(v)) {}

    bool is_null()   const { return std::holds_alternative<ManifestNull>(data); }
    bool is_bool()   const { return std::holds_alternative<ManifestBool>(data); }
    bool is_number() const { return std::holds_alternative<ManifestNumber>(data); }
    bool is_string() const { return std::holds_alternative<ManifestString>(data); }
    bool is_array()  const { return std::holds_alternative<ManifestArray>(data); }
    bool is_object() const { return std::holds_alternative<ManifestObject>(data); }

    bool                as_bool()   const { return std::get<ManifestBool>(data); }
    double              as_number() const { return std::get<ManifestNumber>(data); }
    int                 as_int()    const { return static_cast<int>(std::get<ManifestNumber>(data)); }
    const std::string&  as_string() const { return std::get<ManifestString>(data); }
    std::string&        as_string()       { return std::get<ManifestString>(data); }
    const ManifestArray&  as_array()  const { return std::get<ManifestArray>(data); }
    ManifestArray&        as_array()        { return std::get<ManifestArray>(data); }
    const ManifestObject& as_object() const { return std::get<ManifestObject>(data); }
    ManifestObject&       as_object()       { return std::get<ManifestObject>(data); }

    // Convenience: access object member by key, returns null if missing or not object
    const ManifestValue& operator[](const std::string& key) const;
    // Convenience: access array element by index, returns null if out of bounds
    const ManifestValue& operator[](size_t index) const;

    // Check if object has a given key
    bool has(const std::string& key) const;

    // Returns the number of children (array length or object key count), 0 otherwise
    size_t size() const;
};

// Token types used by the parser
enum class ManifestTokenType {
    STRING,
    NUMBER,
    TRUE_VAL,
    FALSE_VAL,
    NULL_VAL,
    LBRACE,     // {
    RBRACE,     // }
    LBRACKET,   // [
    RBRACKET,   // ]
    COLON,      // :
    COMMA,      // ,
    END_OF_INPUT,
    ERROR_TOKEN
};

struct ManifestToken {
    ManifestTokenType type = ManifestTokenType::END_OF_INPUT;
    std::string       text;
    double            number_value = 0.0;
    int               line   = 1;
    int               column = 1;
};

// Exception type for parse errors
struct ManifestParseError {
    std::string message;
    int line   = 0;
    int column = 0;

    std::string what() const;
};

// Parses a JSON-like manifest format into a ManifestValue tree.
class ManifestParser {
public:
    // Parse from a complete text string.
    // Throws ManifestParseError on syntax errors.
    static ManifestValue parse(const std::string& text);

private:
    explicit ManifestParser(const std::string& text);

    // Tokenizer
    ManifestToken next_token();
    void skip_whitespace_and_comments();
    ManifestToken read_string();
    ManifestToken read_number();
    ManifestToken read_keyword(const std::string& expected, ManifestTokenType type);
    char peek() const;
    char advance();
    bool at_end() const;
    int hex_digit(char c) const;

    // Recursive descent parser
    ManifestValue parse_value();
    ManifestValue parse_object();
    ManifestValue parse_array();

    ManifestToken current_token_;
    void consume();
    void expect(ManifestTokenType type, const std::string& context);

    [[noreturn]] void error(const std::string& msg) const;
    [[noreturn]] void error_at(const std::string& msg, int line, int col) const;

    const std::string& source_;
    size_t pos_    = 0;
    int    line_   = 1;
    int    column_ = 1;
};

// Represents a single batch job from the manifest
struct ManifestJob {
    std::string input_path;
    std::string output_path;
    std::vector<std::string> operations;
    std::map<std::string, ManifestValue> params;
    int priority = 0;
    std::string name;   // optional human-readable job name

    bool is_valid() const;
};

// Represents a fully parsed batch manifest with global settings and a list of jobs.
class Manifest {
public:
    std::vector<ManifestJob> jobs;
    std::string base_dir;
    int max_memory_mb   = 512;
    int max_threads     = 4;
    int timeout_seconds = 300;
    bool fail_fast      = false;
    std::string output_format;

    // Build a Manifest from a parsed ManifestValue tree (the root object).
    static Manifest from_value(const ManifestValue& root);

    // Validate the manifest. Returns list of error strings (empty = valid).
    std::vector<std::string> validate() const;

    // Total number of jobs
    size_t job_count() const { return jobs.size(); }
};

} // namespace PixelForge
