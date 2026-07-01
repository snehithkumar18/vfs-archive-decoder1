#include "manifest.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <stdexcept>

namespace PixelForge {

// ─────────────────────────────────────────────────────────────────────────────
// ManifestValue implementation
// ─────────────────────────────────────────────────────────────────────────────

static const ManifestValue g_null_value{nullptr};

const ManifestValue& ManifestValue::operator[](const std::string& key) const {
    if (!is_object()) return g_null_value;
    const auto& obj = as_object();
    auto it = obj.find(key);
    if (it == obj.end()) return g_null_value;
    return it->second;
}

const ManifestValue& ManifestValue::operator[](size_t index) const {
    if (!is_array()) return g_null_value;
    const auto& arr = as_array();
    if (index >= arr.size()) return g_null_value;
    return arr[index];
}

bool ManifestValue::has(const std::string& key) const {
    if (!is_object()) return false;
    const auto& obj = as_object();
    return obj.find(key) != obj.end();
}

size_t ManifestValue::size() const {
    if (is_array())  return as_array().size();
    if (is_object()) return as_object().size();
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// ManifestParseError
// ─────────────────────────────────────────────────────────────────────────────

std::string ManifestParseError::what() const {
    std::string result = "Parse error";
    if (line > 0) {
        result += " at line " + std::to_string(line);
        result += ", column " + std::to_string(column);
    }
    result += ": " + message;
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// ManifestParser — Tokenizer
// ─────────────────────────────────────────────────────────────────────────────

ManifestParser::ManifestParser(const std::string& text)
    : source_(text) {}

ManifestValue ManifestParser::parse(const std::string& text) {
    ManifestParser parser(text);
    parser.consume(); // prime the first token
    ManifestValue result = parser.parse_value();

    // Ensure we consumed everything
    if (parser.current_token_.type != ManifestTokenType::END_OF_INPUT) {
        parser.error("unexpected content after value");
    }
    return result;
}

char ManifestParser::peek() const {
    if (pos_ >= source_.size()) return '\0';
    return source_[pos_];
}

char ManifestParser::advance() {
    if (pos_ >= source_.size()) return '\0';
    char c = source_[pos_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool ManifestParser::at_end() const {
    return pos_ >= source_.size();
}

void ManifestParser::skip_whitespace_and_comments() {
    while (!at_end()) {
        char c = peek();
        // Skip whitespace characters
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
            continue;
        }
        // C-style line comments starting with //
        if (c == '/' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '/') {
            advance(); // consume first /
            advance(); // consume second /
            while (!at_end() && peek() != '\n') {
                advance();
            }
            continue;
        }
        // C-style block comments starting with /*
        if (c == '/' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '*') {
            int start_line = line_;
            int start_col  = column_;
            advance(); // consume /
            advance(); // consume *
            bool found_end = false;
            while (!at_end()) {
                if (peek() == '*' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '/') {
                    advance(); // consume *
                    advance(); // consume /
                    found_end = true;
                    break;
                }
                advance();
            }
            if (!found_end) {
                error_at("unterminated block comment", start_line, start_col);
            }
            continue;
        }
        break;
    }
}

int ManifestParser::hex_digit(char c) const {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

ManifestToken ManifestParser::read_string() {
    ManifestToken tok;
    tok.type = ManifestTokenType::STRING;
    tok.line   = line_;
    tok.column = column_;

    // Opening quote already verified by caller
    char quote = advance(); // consume " or '

    std::string result;
    while (!at_end()) {
        char c = peek();
        if (c == quote) {
            advance(); // consume closing quote
            tok.text = result;
            return tok;
        }
        if (c == '\n') {
            error("unterminated string literal (newline in string)");
        }
        if (c == '\\') {
            advance(); // consume backslash
            if (at_end()) {
                error("unterminated escape sequence");
            }
            char esc = advance();
            switch (esc) {
                case '"':  result += '"';  break;
                case '\'': result += '\''; break;
                case '\\': result += '\\'; break;
                case '/':  result += '/';  break;
                case 'b':  result += '\b'; break;
                case 'f':  result += '\f'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                case 'u': {
                    // Parse 4-digit hex codepoint: \uXXXX
                    uint32_t codepoint = 0;
                    for (int i = 0; i < 4; i++) {
                        if (at_end()) {
                            error("incomplete \\u escape sequence");
                        }
                        char hc = advance();
                        int d = hex_digit(hc);
                        if (d < 0) {
                            error("invalid hex digit in \\u escape");
                        }
                        codepoint = (codepoint << 4) | static_cast<uint32_t>(d);
                    }
                    // Check for surrogate pair (high surrogate 0xD800..0xDBFF)
                    if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                        // Expect \uXXXX for the low surrogate
                        if (at_end() || peek() != '\\') {
                            error("missing low surrogate in \\u escape");
                        }
                        advance(); // consume '\'
                        if (at_end() || peek() != 'u') {
                            error("missing low surrogate in \\u escape");
                        }
                        advance(); // consume 'u'
                        uint32_t low = 0;
                        for (int i = 0; i < 4; i++) {
                            if (at_end()) {
                                error("incomplete low surrogate \\u escape");
                            }
                            char hc2 = advance();
                            int d2 = hex_digit(hc2);
                            if (d2 < 0) {
                                error("invalid hex digit in low surrogate");
                            }
                            low = (low << 4) | static_cast<uint32_t>(d2);
                        }
                        if (low < 0xDC00 || low > 0xDFFF) {
                            error("invalid low surrogate value");
                        }
                        codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                    }
                    // Encode codepoint as UTF-8
                    if (codepoint <= 0x7F) {
                        result += static_cast<char>(codepoint);
                    } else if (codepoint <= 0x7FF) {
                        result += static_cast<char>(0xC0 | (codepoint >> 6));
                        result += static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else if (codepoint <= 0xFFFF) {
                        result += static_cast<char>(0xE0 | (codepoint >> 12));
                        result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else if (codepoint <= 0x10FFFF) {
                        result += static_cast<char>(0xF0 | (codepoint >> 18));
                        result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                        result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else {
                        error("invalid unicode codepoint");
                    }
                    break;
                }
                default:
                    error(std::string("unknown escape sequence: \\") + esc);
            }
        } else {
            result += advance();
        }
    }
    error("unterminated string literal");
}

ManifestToken ManifestParser::read_number() {
    ManifestToken tok;
    tok.type   = ManifestTokenType::NUMBER;
    tok.line   = line_;
    tok.column = column_;

    std::string num_str;

    // Optional leading minus
    if (peek() == '-') {
        num_str += advance();
    }

    // Integer part
    if (peek() == '0') {
        num_str += advance();
        // JSON doesn't allow leading zeros except for 0 itself
    } else if (peek() >= '1' && peek() <= '9') {
        while (!at_end() && peek() >= '0' && peek() <= '9') {
            num_str += advance();
        }
    } else {
        error("expected digit in number");
    }

    // Fractional part
    if (!at_end() && peek() == '.') {
        num_str += advance(); // consume '.'
        if (at_end() || peek() < '0' || peek() > '9') {
            error("expected digit after decimal point");
        }
        while (!at_end() && peek() >= '0' && peek() <= '9') {
            num_str += advance();
        }
    }

    // Exponent part
    if (!at_end() && (peek() == 'e' || peek() == 'E')) {
        num_str += advance();
        if (!at_end() && (peek() == '+' || peek() == '-')) {
            num_str += advance();
        }
        if (at_end() || peek() < '0' || peek() > '9') {
            error("expected digit in exponent");
        }
        while (!at_end() && peek() >= '0' && peek() <= '9') {
            num_str += advance();
        }
    }

    // Parse the accumulated number string
    char* end_ptr = nullptr;
    tok.number_value = std::strtod(num_str.c_str(), &end_ptr);
    tok.text = num_str;
    return tok;
}

ManifestToken ManifestParser::read_keyword(const std::string& expected, ManifestTokenType type) {
    ManifestToken tok;
    tok.type   = type;
    tok.line   = line_;
    tok.column = column_;

    for (size_t i = 0; i < expected.size(); i++) {
        if (at_end() || peek() != expected[i]) {
            error("unexpected character while parsing keyword '" + expected + "'");
        }
        advance();
    }
    // Verify that the keyword is not followed by alphanumeric chars
    if (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
        error("unexpected character after keyword '" + expected + "'");
    }
    tok.text = expected;
    return tok;
}

ManifestToken ManifestParser::next_token() {
    skip_whitespace_and_comments();

    if (at_end()) {
        ManifestToken tok;
        tok.type   = ManifestTokenType::END_OF_INPUT;
        tok.line   = line_;
        tok.column = column_;
        return tok;
    }

    char c = peek();
    int tok_line = line_;
    int tok_col  = column_;

    // Single-character tokens
    auto make_single = [&](ManifestTokenType type) -> ManifestToken {
        ManifestToken tok;
        tok.type = type;
        tok.line = tok_line;
        tok.column = tok_col;
        tok.text = std::string(1, advance());
        return tok;
    };

    switch (c) {
        case '{': return make_single(ManifestTokenType::LBRACE);
        case '}': return make_single(ManifestTokenType::RBRACE);
        case '[': return make_single(ManifestTokenType::LBRACKET);
        case ']': return make_single(ManifestTokenType::RBRACKET);
        case ':': return make_single(ManifestTokenType::COLON);
        case ',': return make_single(ManifestTokenType::COMMA);
        case '"':
        case '\'':
            return read_string();
        case 't':
            return read_keyword("true", ManifestTokenType::TRUE_VAL);
        case 'f':
            return read_keyword("false", ManifestTokenType::FALSE_VAL);
        case 'n':
            return read_keyword("null", ManifestTokenType::NULL_VAL);
        default:
            break;
    }

    // Numbers: digits or leading minus
    if (c == '-' || (c >= '0' && c <= '9')) {
        return read_number();
    }

    error(std::string("unexpected character: '") + c + "'");
}

void ManifestParser::consume() {
    current_token_ = next_token();
}

void ManifestParser::expect(ManifestTokenType type, const std::string& context) {
    if (current_token_.type != type) {
        std::string got;
        switch (current_token_.type) {
            case ManifestTokenType::STRING:       got = "string";       break;
            case ManifestTokenType::NUMBER:       got = "number";       break;
            case ManifestTokenType::TRUE_VAL:     got = "true";         break;
            case ManifestTokenType::FALSE_VAL:    got = "false";        break;
            case ManifestTokenType::NULL_VAL:     got = "null";         break;
            case ManifestTokenType::LBRACE:       got = "'{'";          break;
            case ManifestTokenType::RBRACE:       got = "'}'";          break;
            case ManifestTokenType::LBRACKET:     got = "'['";          break;
            case ManifestTokenType::RBRACKET:     got = "']'";          break;
            case ManifestTokenType::COLON:        got = "':'";          break;
            case ManifestTokenType::COMMA:        got = "','";          break;
            case ManifestTokenType::END_OF_INPUT: got = "end of input"; break;
            case ManifestTokenType::ERROR_TOKEN:  got = "error";        break;
        }
        error("expected " + context + ", got " + got);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// ManifestParser — Recursive descent
// ─────────────────────────────────────────────────────────────────────────────

ManifestValue ManifestParser::parse_value() {
    switch (current_token_.type) {
        case ManifestTokenType::LBRACE:
            return parse_object();
        case ManifestTokenType::LBRACKET:
            return parse_array();
        case ManifestTokenType::STRING: {
            ManifestValue val(current_token_.text);
            consume();
            return val;
        }
        case ManifestTokenType::NUMBER: {
            ManifestValue val(current_token_.number_value);
            consume();
            return val;
        }
        case ManifestTokenType::TRUE_VAL: {
            ManifestValue val(true);
            consume();
            return val;
        }
        case ManifestTokenType::FALSE_VAL: {
            ManifestValue val(false);
            consume();
            return val;
        }
        case ManifestTokenType::NULL_VAL: {
            ManifestValue val(nullptr);
            consume();
            return val;
        }
        default:
            error("expected a value");
    }
}

ManifestValue ManifestParser::parse_object() {
    expect(ManifestTokenType::LBRACE, "'{'");
    consume(); // consume {

    ManifestObject obj;

    if (current_token_.type == ManifestTokenType::RBRACE) {
        consume(); // consume }
        return ManifestValue(std::move(obj));
    }

    while (true) {
        expect(ManifestTokenType::STRING, "object key (string)");
        std::string key = current_token_.text;
        consume();

        expect(ManifestTokenType::COLON, "':'");
        consume();

        ManifestValue value = parse_value();
        obj[key] = std::move(value);

        if (current_token_.type == ManifestTokenType::COMMA) {
            consume();
            // Allow trailing comma before closing brace
            if (current_token_.type == ManifestTokenType::RBRACE) {
                break;
            }
            continue;
        }
        break;
    }

    expect(ManifestTokenType::RBRACE, "'}'");
    consume();

    return ManifestValue(std::move(obj));
}

ManifestValue ManifestParser::parse_array() {
    expect(ManifestTokenType::LBRACKET, "'['");
    consume(); // consume [

    ManifestArray arr;

    if (current_token_.type == ManifestTokenType::RBRACKET) {
        consume(); // consume ]
        return ManifestValue(std::move(arr));
    }

    while (true) {
        ManifestValue value = parse_value();
        arr.push_back(std::move(value));

        if (current_token_.type == ManifestTokenType::COMMA) {
            consume();
            // Allow trailing comma before closing bracket
            if (current_token_.type == ManifestTokenType::RBRACKET) {
                break;
            }
            continue;
        }
        break;
    }

    expect(ManifestTokenType::RBRACKET, "']'");
    consume();

    return ManifestValue(std::move(arr));
}

void ManifestParser::error(const std::string& msg) const {
    ManifestParseError err;
    err.message = msg;
    err.line    = current_token_.line;
    err.column  = current_token_.column;
    throw err;
}

void ManifestParser::error_at(const std::string& msg, int line, int col) const {
    ManifestParseError err;
    err.message = msg;
    err.line    = line;
    err.column  = col;
    throw err;
}

// ─────────────────────────────────────────────────────────────────────────────
// ManifestJob
// ─────────────────────────────────────────────────────────────────────────────

bool ManifestJob::is_valid() const {
    if (input_path.empty()) return false;
    if (output_path.empty()) return false;
    if (operations.empty()) return false;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Manifest::from_value — extract manifest structure from parsed JSON tree
// ─────────────────────────────────────────────────────────────────────────────

static ManifestJob extract_job(const ManifestValue& job_val) {
    ManifestJob job;

    if (!job_val.is_object()) {
        return job;
    }

    // Required: input path
    const auto& input = job_val["input"];
    if (input.is_string()) {
        job.input_path = input.as_string();
    }

    // Required: output path
    const auto& output = job_val["output"];
    if (output.is_string()) {
        job.output_path = output.as_string();
    }

    // Optional: human-readable name
    const auto& name = job_val["name"];
    if (name.is_string()) {
        job.name = name.as_string();
    } else {
        // Derive name from input path
        job.name = job.input_path;
        size_t slash = job.name.find_last_of("/\\");
        if (slash != std::string::npos) {
            job.name = job.name.substr(slash + 1);
        }
    }

    // Required: operations list
    const auto& ops = job_val["operations"];
    if (ops.is_array()) {
        for (size_t i = 0; i < ops.size(); i++) {
            const auto& op = ops[i];
            if (op.is_string()) {
                job.operations.push_back(op.as_string());
            }
        }
    } else if (ops.is_string()) {
        // Single operation as a string
        job.operations.push_back(ops.as_string());
    }

    // Optional: priority
    const auto& prio = job_val["priority"];
    if (prio.is_number()) {
        job.priority = prio.as_int();
    }

    // Optional: parameters object
    const auto& params = job_val["params"];
    if (params.is_object()) {
        for (const auto& [k, v] : params.as_object()) {
            job.params[k] = v;
        }
    }

    // Also accept top-level operation-specific params (resize_width, etc.)
    // by treating all unknown keys as params
    if (job_val.is_object()) {
        static const std::vector<std::string> known_keys = {
            "input", "output", "name", "operations", "priority", "params"
        };
        for (const auto& [k, v] : job_val.as_object()) {
            bool is_known = false;
            for (const auto& kk : known_keys) {
                if (k == kk) { is_known = true; break; }
            }
            if (!is_known) {
                job.params[k] = v;
            }
        }
    }

    return job;
}

Manifest Manifest::from_value(const ManifestValue& root) {
    Manifest manifest;

    if (!root.is_object()) {
        return manifest;
    }

    // Global settings
    const auto& base = root["base_dir"];
    if (base.is_string()) {
        manifest.base_dir = base.as_string();
    }

    const auto& mem = root["max_memory_mb"];
    if (mem.is_number()) {
        manifest.max_memory_mb = mem.as_int();
    }

    const auto& threads = root["max_threads"];
    if (threads.is_number()) {
        manifest.max_threads = threads.as_int();
    }

    const auto& timeout = root["timeout_seconds"];
    if (timeout.is_number()) {
        manifest.timeout_seconds = timeout.as_int();
    }

    const auto& ff = root["fail_fast"];
    if (ff.is_bool()) {
        manifest.fail_fast = ff.as_bool();
    }

    const auto& fmt = root["output_format"];
    if (fmt.is_string()) {
        manifest.output_format = fmt.as_string();
    }

    // Jobs array
    const auto& jobs = root["jobs"];
    if (jobs.is_array()) {
        for (size_t i = 0; i < jobs.size(); i++) {
            ManifestJob job = extract_job(jobs[i]);
            manifest.jobs.push_back(std::move(job));
        }
    }

    // Alternative: "pipeline" key with shared operations applied to a file list
    const auto& pipeline = root["pipeline"];
    if (pipeline.is_object()) {
        const auto& shared_ops = pipeline["operations"];
        const auto& shared_params = pipeline["params"];
        const auto& files = pipeline["files"];

        std::vector<std::string> ops;
        if (shared_ops.is_array()) {
            for (size_t i = 0; i < shared_ops.size(); i++) {
                if (shared_ops[i].is_string()) {
                    ops.push_back(shared_ops[i].as_string());
                }
            }
        }

        std::map<std::string, ManifestValue> params;
        if (shared_params.is_object()) {
            for (const auto& [k, v] : shared_params.as_object()) {
                params[k] = v;
            }
        }

        if (files.is_array()) {
            for (size_t i = 0; i < files.size(); i++) {
                ManifestJob job;
                if (files[i].is_string()) {
                    job.input_path = files[i].as_string();
                    // Generate output path by appending _out before extension
                    std::string inp = job.input_path;
                    size_t dot = inp.find_last_of('.');
                    if (dot != std::string::npos) {
                        job.output_path = inp.substr(0, dot) + "_out" + inp.substr(dot);
                    } else {
                        job.output_path = inp + "_out";
                    }
                } else if (files[i].is_object()) {
                    const auto& f = files[i];
                    if (f["input"].is_string())  job.input_path  = f["input"].as_string();
                    if (f["output"].is_string()) job.output_path = f["output"].as_string();
                }
                job.operations = ops;
                job.params = params;
                job.name = job.input_path;
                size_t slash = job.name.find_last_of("/\\");
                if (slash != std::string::npos) {
                    job.name = job.name.substr(slash + 1);
                }
                manifest.jobs.push_back(std::move(job));
            }
        }
    }

    return manifest;
}

// ─────────────────────────────────────────────────────────────────────────────
// Manifest::validate
// ─────────────────────────────────────────────────────────────────────────────

std::vector<std::string> Manifest::validate() const {
    std::vector<std::string> errors;

    if (jobs.empty()) {
        errors.push_back("manifest contains no jobs");
    }

    if (max_memory_mb <= 0) {
        errors.push_back("max_memory_mb must be positive");
    }

    if (max_threads <= 0) {
        errors.push_back("max_threads must be positive");
    }

    if (timeout_seconds <= 0) {
        errors.push_back("timeout_seconds must be positive");
    }

    // Valid operation names
    static const std::vector<std::string> valid_ops = {
        "load", "save", "resize", "crop", "grayscale", "blur",
        "sharpen", "rotate", "flip", "color_convert", "composite",
        "brightness", "contrast", "gamma", "sepia", "invert",
        "threshold", "edge_detect", "emboss", "vignette"
    };

    for (size_t i = 0; i < jobs.size(); i++) {
        const auto& job = jobs[i];
        std::string prefix = "job[" + std::to_string(i) + "]";

        if (job.input_path.empty()) {
            errors.push_back(prefix + ": missing input path");
        }
        if (job.output_path.empty()) {
            errors.push_back(prefix + ": missing output path");
        }
        if (job.operations.empty()) {
            errors.push_back(prefix + ": no operations specified");
        }

        for (const auto& op : job.operations) {
            bool found = false;
            for (const auto& valid_op : valid_ops) {
                if (op == valid_op) { found = true; break; }
            }
            if (!found) {
                errors.push_back(prefix + ": unknown operation '" + op + "'");
            }
        }

        // Validate specific operation parameters
        for (const auto& op : job.operations) {
            if (op == "resize") {
                bool has_width  = job.params.count("width")  > 0 || job.params.count("resize_width")  > 0;
                bool has_height = job.params.count("height") > 0 || job.params.count("resize_height") > 0;
                if (!has_width && !has_height) {
                    errors.push_back(prefix + ": resize requires width and/or height");
                }
            }
            if (op == "crop") {
                bool has_x = job.params.count("x") > 0 || job.params.count("crop_x") > 0;
                bool has_y = job.params.count("y") > 0 || job.params.count("crop_y") > 0;
                bool has_w = job.params.count("crop_width") > 0 || job.params.count("w") > 0;
                bool has_h = job.params.count("crop_height") > 0 || job.params.count("h") > 0;
                if (!has_x || !has_y || !has_w || !has_h) {
                    errors.push_back(prefix + ": crop requires x, y, width, and height");
                }
            }
            if (op == "rotate") {
                bool has_angle = job.params.count("angle") > 0 || job.params.count("rotate_angle") > 0;
                if (!has_angle) {
                    errors.push_back(prefix + ": rotate requires angle");
                }
            }
            if (op == "blur") {
                bool has_radius = job.params.count("radius") > 0 || job.params.count("blur_radius") > 0;
                if (!has_radius) {
                    errors.push_back(prefix + ": blur requires radius");
                }
            }
        }
    }

    return errors;
}

} // namespace PixelForge
