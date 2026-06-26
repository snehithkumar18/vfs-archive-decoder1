///////////////////////////////////////////////////////////////////////////////
/// @file config_parser.cc
/// @brief Implementation of VFSConfigParser — INI-style configuration parser
///
/// Handles line-by-line parsing of INI text with full support for:
///   - [section] headers
///   - key = value pairs (with = or : as delimiters)
///   - Comments (# and ;), including inline comments
///   - Quoted string values with escape sequences
///   - Backslash line continuation for multiline values
///   - Duplicate keys (last definition wins, with warning)
///   - Type-safe accessors with default fallbacks
///   - Round-trip serialization back to INI format
///////////////////////////////////////////////////////////////////////////////

#include "config_parser.h"
#include "logger.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>

static const char* LOG_TAG = "ConfigParser";

///////////////////////////////////////////////////////////////////////////////
// ConfigSection member functions
///////////////////////////////////////////////////////////////////////////////

bool ConfigSection::has_key(const std::string& key) const
{
    return entries.find(key) != entries.end();
}

const std::string& ConfigSection::get_value(const std::string& key) const
{
    static const std::string empty_string;
    auto it = entries.find(key);
    if (it != entries.end()) {
        return it->second.value;
    }
    return empty_string;
}

///////////////////////////////////////////////////////////////////////////////
// Construction / Destruction
///////////////////////////////////////////////////////////////////////////////

VFSConfigParser::VFSConfigParser()
{
    VFSLogger::get_instance().debug(LOG_TAG, "Config parser instance created");
}

VFSConfigParser::~VFSConfigParser()
{
    VFSLogger::get_instance().debug(LOG_TAG,
        "Config parser destroyed. Sections=" + std::to_string(sections_.size()) +
        " total_keys=" + std::to_string(total_key_count()));
}

///////////////////////////////////////////////////////////////////////////////
// Static helpers
///////////////////////////////////////////////////////////////////////////////

std::string VFSConfigParser::trim(const std::string& str)
{
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }

    if (start == str.size()) return "";

    size_t end = str.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(str[end]))) {
        --end;
    }

    return str.substr(start, end - start + 1);
}

std::string VFSConfigParser::to_lower(const std::string& str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool VFSConfigParser::is_comment_char(char c)
{
    return c == '#' || c == ';';
}

/// @brief Process escape sequences in a parsed value string.
///
/// Supported sequences:
///   \\  -> backslash
///   \n  -> newline
///   \t  -> tab
///   \r  -> carriage return
///   \"  -> double quote
///   \'  -> single quote
///   \#  -> literal # (not a comment)
///   \;  -> literal ; (not a comment)
///   \0  -> null character
///   \xHH -> hex byte
///
/// Unknown escape sequences are kept as-is (e.g. \q -> \q) with a warning
/// emitted during parsing. Here we just do the translation silently.
std::string VFSConfigParser::process_escapes(const std::string& str)
{
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            char next = str[i + 1];
            switch (next) {
                case '\\': result += '\\'; ++i; break;
                case 'n':  result += '\n'; ++i; break;
                case 't':  result += '\t'; ++i; break;
                case 'r':  result += '\r'; ++i; break;
                case '"':  result += '"';  ++i; break;
                case '\'': result += '\''; ++i; break;
                case '#':  result += '#';  ++i; break;
                case ';':  result += ';';  ++i; break;
                case '0':  result += '\0'; ++i; break;
                case 'x': {
                    // Hex escape: \xHH — need exactly two hex digits.
                    if (i + 3 < str.size()
                        && std::isxdigit(static_cast<unsigned char>(str[i + 2]))
                        && std::isxdigit(static_cast<unsigned char>(str[i + 3])))
                    {
                        char hex_str[3] = { str[i + 2], str[i + 3], '\0' };
                        unsigned long val = std::strtoul(hex_str, nullptr, 16);
                        result += static_cast<char>(val);
                        i += 3;
                    } else {
                        // Malformed hex escape — keep as-is.
                        result += '\\';
                    }
                    break;
                }
                default:
                    // Unknown escape — keep both characters.
                    result += '\\';
                    result += next;
                    ++i;
                    break;
            }
        } else {
            result += str[i];
        }
    }

    return result;
}

/// @brief Extract a quoted string value starting at str[0] which must be '"'.
///
/// Handles escape sequences within the quoted region. The closing quote
/// terminates extraction. Returns the number of characters consumed from str
/// (including the opening and closing quotes), or 0 if the string is
/// unterminated (no closing quote found).
///
/// @param str     Input starting with the opening quote character.
/// @param result  [out] The extracted and unescaped string content.
/// @return Number of characters consumed, or 0 on error.
size_t VFSConfigParser::extract_quoted_string(const std::string& str,
                                               std::string& result)
{
    if (str.empty() || (str[0] != '"' && str[0] != '\'')) return 0;

    char quote_char = str[0];
    result.clear();

    size_t i = 1;  // Skip opening quote.
    while (i < str.size()) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            // Escape sequence inside quotes.
            char next = str[i + 1];
            switch (next) {
                case '\\': result += '\\'; break;
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case 'r':  result += '\r'; break;
                case '"':  result += '"';  break;
                case '\'': result += '\''; break;
                case '#':  result += '#';  break;
                case ';':  result += ';';  break;
                default:
                    // Unknown escape in quotes — keep both chars.
                    result += '\\';
                    result += next;
                    break;
            }
            i += 2;
        } else if (str[i] == quote_char) {
            // Closing quote found.
            return i + 1;  // Consumed characters including both quotes.
        } else {
            result += str[i];
            ++i;
        }
    }

    // Unterminated string — no closing quote found.
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Clearing state
///////////////////////////////////////////////////////////////////////////////

void VFSConfigParser::clear()
{
    sections_.clear();
    section_order_.clear();

    VFSLogger::get_instance().debug(LOG_TAG, "Cleared all parsed configuration data");
}

///////////////////////////////////////////////////////////////////////////////
// Parsing from byte buffer
///////////////////////////////////////////////////////////////////////////////

ConfigParseResult VFSConfigParser::parse_file(const uint8_t* buffer, size_t size)
{
    if (!buffer || size == 0) {
        VFSLogger::get_instance().warn(LOG_TAG, "parse_file called with null/empty buffer");
        ConfigParseResult r;
        r.success = false;
        r.lines_parsed = 0;
        r.entries_parsed = 0;
        r.sections_parsed = 0;
        r.warnings = 0;
        return r;
    }

    // Convert buffer to string and delegate to parse_string.
    std::string data(reinterpret_cast<const char*>(buffer), size);

    VFSLogger::get_instance().info(LOG_TAG,
        "parse_file: parsing " + std::to_string(size) + " bytes of config data");

    return parse_string(data);
}

///////////////////////////////////////////////////////////////////////////////
// Parsing from string
///////////////////////////////////////////////////////////////////////////////

ConfigParseResult VFSConfigParser::parse_string(const std::string& data)
{
    // Clear previous state.
    clear();

    ConfigParseResult result;
    result.success = true;
    result.lines_parsed = 0;
    result.entries_parsed = 0;
    result.sections_parsed = 0;
    result.warnings = 0;

    VFSLogger::get_instance().info(LOG_TAG,
        "parse_string: beginning parse of " + std::to_string(data.size()) +
        " character(s) of INI data");

    // Ensure the global section exists (for keys defined before any [section]).
    {
        ConfigSection global_sec;
        global_sec.name = GLOBAL_SECTION;
        global_sec.source_line = 0;
        sections_[GLOBAL_SECTION] = std::move(global_sec);
        section_order_.push_back(GLOBAL_SECTION);
    }

    std::string current_section_name = GLOBAL_SECTION;
    std::string accumulated_comment;

    // Split input into lines. We handle \r\n, \n, and \r line endings.
    std::vector<std::string> lines;
    {
        std::istringstream stream(data);
        std::string line;
        while (std::getline(stream, line)) {
            // Remove trailing \r if present (from \r\n endings).
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(std::move(line));
        }
    }

    VFSLogger::get_instance().debug(LOG_TAG,
        "Input split into " + std::to_string(lines.size()) + " line(s)");

    for (size_t line_idx = 0; line_idx < lines.size(); ++line_idx) {
        result.lines_parsed++;
        size_t line_num = line_idx + 1;  // 1-based for human-readable logs.

        std::string raw_line = lines[line_idx];

        //---------------------------------------------------------------------
        // Handle backslash line continuation.
        // If a line ends with '\' (after trimming), concatenate the next line.
        //---------------------------------------------------------------------
        while (!raw_line.empty() && raw_line.back() == '\\') {
            // Remove the trailing backslash.
            raw_line.pop_back();

            // Append the next line, if any.
            if (line_idx + 1 < lines.size()) {
                line_idx++;
                result.lines_parsed++;
                std::string next_line = lines[line_idx];
                // Remove trailing \r if present.
                if (!next_line.empty() && next_line.back() == '\r') {
                    next_line.pop_back();
                }
                // Trim leading whitespace from continuation line for clean join.
                size_t ws = 0;
                while (ws < next_line.size() &&
                       std::isspace(static_cast<unsigned char>(next_line[ws]))) {
                    ++ws;
                }
                raw_line += next_line.substr(ws);

                VFSLogger::get_instance().debug(LOG_TAG,
                    "Line " + std::to_string(line_num) +
                    ": backslash continuation joined with line " +
                    std::to_string(line_idx + 1));
            } else {
                // Trailing backslash at end of file — treat as literal.
                VFSLogger::get_instance().warn(LOG_TAG,
                    "Line " + std::to_string(line_num) +
                    ": trailing backslash at end of input (no continuation line)");
                result.warnings++;
                result.warning_messages.push_back(
                    "Line " + std::to_string(line_num) +
                    ": trailing backslash at end of input");
                break;
            }
        }

        std::string line = trim(raw_line);

        //---------------------------------------------------------------------
        // Skip empty lines.
        //---------------------------------------------------------------------
        if (line.empty()) {
            continue;
        }

        //---------------------------------------------------------------------
        // Comment lines (# or ; as first non-whitespace character).
        //---------------------------------------------------------------------
        if (is_comment_char(line[0])) {
            // Accumulate comments to attach to the next section header.
            if (!accumulated_comment.empty()) {
                accumulated_comment += "\n";
            }
            accumulated_comment += line;
            continue;
        }

        //---------------------------------------------------------------------
        // Section header: [section_name]
        //---------------------------------------------------------------------
        if (line[0] == '[') {
            size_t close = line.find(']');
            if (close == std::string::npos) {
                // Malformed section header — missing closing bracket.
                VFSLogger::get_instance().warn(LOG_TAG,
                    "Line " + std::to_string(line_num) +
                    ": malformed section header (missing ']'): " + line);
                result.warnings++;
                result.warning_messages.push_back(
                    "Line " + std::to_string(line_num) +
                    ": malformed section header");
                continue;
            }

            std::string section_name = trim(line.substr(1, close - 1));

            if (section_name.empty()) {
                VFSLogger::get_instance().warn(LOG_TAG,
                    "Line " + std::to_string(line_num) +
                    ": empty section name in header");
                result.warnings++;
                result.warning_messages.push_back(
                    "Line " + std::to_string(line_num) +
                    ": empty section name");
                continue;
            }

            current_section_name = section_name;
            result.sections_parsed++;

            // Create section if it doesn't exist.
            if (sections_.find(section_name) == sections_.end()) {
                ConfigSection sec;
                sec.name = section_name;
                sec.source_line = line_num;
                sec.header_comment = accumulated_comment;
                sections_[section_name] = std::move(sec);
                section_order_.push_back(section_name);

                VFSLogger::get_instance().debug(LOG_TAG,
                    "Line " + std::to_string(line_num) +
                    ": new section [" + section_name + "]");
            } else {
                // Section already exists — this is a continuation.
                VFSLogger::get_instance().debug(LOG_TAG,
                    "Line " + std::to_string(line_num) +
                    ": continuing existing section [" + section_name + "]");
            }

            accumulated_comment.clear();
            continue;
        }

        //---------------------------------------------------------------------
        // Key-value pair: key = value  or  key : value
        //---------------------------------------------------------------------
        // Find the first unquoted '=' or ':' delimiter.
        size_t delim_pos = std::string::npos;
        bool in_quotes = false;
        char quote_char = '\0';

        for (size_t i = 0; i < line.size(); ++i) {
            if (in_quotes) {
                if (line[i] == '\\' && i + 1 < line.size()) {
                    ++i;  // Skip escaped character inside quotes.
                } else if (line[i] == quote_char) {
                    in_quotes = false;
                }
            } else {
                if (line[i] == '"' || line[i] == '\'') {
                    in_quotes = true;
                    quote_char = line[i];
                } else if (line[i] == '=' || line[i] == ':') {
                    delim_pos = i;
                    break;
                }
            }
        }

        if (delim_pos == std::string::npos) {
            // No delimiter found — malformed line.
            VFSLogger::get_instance().warn(LOG_TAG,
                "Line " + std::to_string(line_num) +
                ": no key-value delimiter found: " + line);
            result.warnings++;
            result.warning_messages.push_back(
                "Line " + std::to_string(line_num) +
                ": no key-value delimiter found");
            continue;
        }

        std::string key = trim(line.substr(0, delim_pos));
        std::string value_part = (delim_pos + 1 < line.size())
            ? line.substr(delim_pos + 1)
            : "";

        if (key.empty()) {
            VFSLogger::get_instance().warn(LOG_TAG,
                "Line " + std::to_string(line_num) + ": empty key name");
            result.warnings++;
            result.warning_messages.push_back(
                "Line " + std::to_string(line_num) + ": empty key name");
            continue;
        }

        //-- Parse the value portion ----------------------------------------
        std::string value;
        std::string inline_comment;

        std::string trimmed_value = trim(value_part);

        if (!trimmed_value.empty() && (trimmed_value[0] == '"' || trimmed_value[0] == '\'')) {
            // Quoted value — extract with escape handling.
            std::string extracted;
            size_t consumed = extract_quoted_string(trimmed_value, extracted);

            if (consumed == 0) {
                // Unterminated quote — use the raw value minus the opening quote.
                VFSLogger::get_instance().warn(LOG_TAG,
                    "Line " + std::to_string(line_num) +
                    ": unterminated quoted string for key '" + key + "'");
                result.warnings++;
                result.warning_messages.push_back(
                    "Line " + std::to_string(line_num) +
                    ": unterminated quoted string");
                value = trimmed_value.substr(1);
            } else {
                value = extracted;

                // Check for inline comment after the closing quote.
                std::string after_quote = trim(trimmed_value.substr(consumed));
                if (!after_quote.empty() && is_comment_char(after_quote[0])) {
                    inline_comment = after_quote;
                }
            }
        } else {
            // Unquoted value — strip inline comments.
            // Scan for unescaped # or ; outside quotes.
            size_t comment_pos = std::string::npos;
            for (size_t i = 0; i < trimmed_value.size(); ++i) {
                if (trimmed_value[i] == '\\' && i + 1 < trimmed_value.size()) {
                    ++i;  // Skip escaped char.
                } else if (is_comment_char(trimmed_value[i])) {
                    comment_pos = i;
                    break;
                }
            }

            if (comment_pos != std::string::npos) {
                inline_comment = trimmed_value.substr(comment_pos);
                value = trim(trimmed_value.substr(0, comment_pos));
            } else {
                value = trimmed_value;
            }

            // Process escape sequences in unquoted values.
            value = process_escapes(value);
        }

        //-- Store the entry -------------------------------------------------
        ConfigEntry entry;
        entry.key = key;
        entry.value = value;
        entry.inline_comment = inline_comment;
        entry.source_line = line_num;

        auto& sec = sections_[current_section_name];

        // Check for duplicate key.
        if (sec.entries.find(key) != sec.entries.end()) {
            VFSLogger::get_instance().warn(LOG_TAG,
                "Line " + std::to_string(line_num) +
                ": duplicate key '" + key + "' in section [" +
                current_section_name + "] — overwriting previous value '" +
                sec.entries[key].value + "' with '" + value + "'");
            result.warnings++;
            result.warning_messages.push_back(
                "Line " + std::to_string(line_num) +
                ": duplicate key '" + key + "' in [" + current_section_name + "]");
        } else {
            // Track insertion order only for new keys.
            sec.key_order.push_back(key);
        }

        sec.entries[key] = std::move(entry);
        result.entries_parsed++;

        VFSLogger::get_instance().debug(LOG_TAG,
            "Line " + std::to_string(line_num) + ": [" +
            current_section_name + "] " + key + " = \"" + value + "\"");
    }

    // Remove the global section if it has no entries (cleaner output).
    auto git = sections_.find(GLOBAL_SECTION);
    if (git != sections_.end() && git->second.entries.empty()) {
        sections_.erase(git);
        section_order_.erase(
            std::remove(section_order_.begin(), section_order_.end(),
                        std::string(GLOBAL_SECTION)),
            section_order_.end());
    }

    VFSLogger::get_instance().info(LOG_TAG,
        "parse_string complete: " + std::to_string(result.lines_parsed) + " lines, " +
        std::to_string(result.sections_parsed) + " sections, " +
        std::to_string(result.entries_parsed) + " entries, " +
        std::to_string(result.warnings) + " warnings");

    return result;
}

///////////////////////////////////////////////////////////////////////////////
// Type-safe accessors
///////////////////////////////////////////////////////////////////////////////

std::string VFSConfigParser::get_string(const std::string& section,
                                         const std::string& key,
                                         const std::string& default_val) const
{
    auto sec_it = sections_.find(section);
    if (sec_it == sections_.end()) {
        VFSLogger::get_instance().debug(LOG_TAG,
            "get_string: section [" + section + "] not found, returning default");
        return default_val;
    }

    auto& entries = sec_it->second.entries;
    auto key_it = entries.find(key);
    if (key_it == entries.end()) {
        VFSLogger::get_instance().debug(LOG_TAG,
            "get_string: key '" + key + "' not found in [" + section +
            "], returning default");
        return default_val;
    }

    return key_it->second.value;
}

int64_t VFSConfigParser::get_int(const std::string& section,
                                  const std::string& key,
                                  int64_t default_val) const
{
    std::string str = get_string(section, key, "");
    if (str.empty() && !has_key(section, key)) {
        return default_val;
    }

    // Handle empty value for existing key.
    if (str.empty()) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "get_int: key '" + key + "' in [" + section +
            "] has empty value, returning default");
        return default_val;
    }

    try {
        // Detect base from prefix.
        size_t pos = 0;
        int base = 10;
        std::string parse_str = str;

        // Handle negative sign.
        bool negative = false;
        if (!parse_str.empty() && parse_str[0] == '-') {
            negative = true;
            parse_str = parse_str.substr(1);
        } else if (!parse_str.empty() && parse_str[0] == '+') {
            parse_str = parse_str.substr(1);
        }

        if (parse_str.size() > 2 && parse_str[0] == '0') {
            char prefix = std::tolower(static_cast<unsigned char>(parse_str[1]));
            if (prefix == 'x') {
                base = 16;
                parse_str = parse_str.substr(2);
            } else if (prefix == 'o') {
                base = 8;
                parse_str = parse_str.substr(2);
            } else if (prefix == 'b') {
                base = 2;
                parse_str = parse_str.substr(2);
            }
        }

        int64_t value = static_cast<int64_t>(std::stoull(parse_str, &pos, base));
        if (negative) value = -value;

        // Check that we consumed the entire string.
        if (pos != parse_str.size()) {
            VFSLogger::get_instance().warn(LOG_TAG,
                "get_int: key '" + key + "' in [" + section +
                "] has trailing characters after integer: '" + str + "'");
            return default_val;
        }

        return value;
    }
    catch (const std::exception& ex) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "get_int: failed to parse '" + str + "' for key '" + key +
            "' in [" + section + "]: " + ex.what());
        return default_val;
    }
}

bool VFSConfigParser::get_bool(const std::string& section,
                                const std::string& key,
                                bool default_val) const
{
    std::string str = get_string(section, key, "");
    if (str.empty() && !has_key(section, key)) {
        return default_val;
    }

    std::string lower = to_lower(trim(str));

    // True values.
    if (lower == "true" || lower == "yes" || lower == "on" || lower == "1" ||
        lower == "enabled" || lower == "enable")
    {
        return true;
    }

    // False values.
    if (lower == "false" || lower == "no" || lower == "off" || lower == "0" ||
        lower == "disabled" || lower == "disable")
    {
        return false;
    }

    VFSLogger::get_instance().warn(LOG_TAG,
        "get_bool: ambiguous boolean value '" + str + "' for key '" + key +
        "' in [" + section + "], returning default=" +
        std::string(default_val ? "true" : "false"));
    return default_val;
}

double VFSConfigParser::get_double(const std::string& section,
                                    const std::string& key,
                                    double default_val) const
{
    std::string str = get_string(section, key, "");
    if (str.empty() && !has_key(section, key)) {
        return default_val;
    }

    if (str.empty()) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "get_double: key '" + key + "' in [" + section +
            "] has empty value, returning default");
        return default_val;
    }

    try {
        size_t pos = 0;
        double value = std::stod(str, &pos);

        if (pos != str.size()) {
            VFSLogger::get_instance().warn(LOG_TAG,
                "get_double: trailing characters after number in '" + str +
                "' for key '" + key + "' in [" + section + "]");
            return default_val;
        }

        return value;
    }
    catch (const std::exception& ex) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "get_double: failed to parse '" + str + "' for key '" + key +
            "' in [" + section + "]: " + ex.what());
        return default_val;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Existence checks
///////////////////////////////////////////////////////////////////////////////

bool VFSConfigParser::has_section(const std::string& section) const
{
    return sections_.find(section) != sections_.end();
}

bool VFSConfigParser::has_key(const std::string& section,
                               const std::string& key) const
{
    auto sec_it = sections_.find(section);
    if (sec_it == sections_.end()) return false;
    return sec_it->second.entries.find(key) != sec_it->second.entries.end();
}

///////////////////////////////////////////////////////////////////////////////
// Enumeration
///////////////////////////////////////////////////////////////////////////////

std::vector<std::string> VFSConfigParser::get_sections() const
{
    // Return sections in parse/insertion order.
    return section_order_;
}

std::vector<std::string> VFSConfigParser::get_keys(const std::string& section) const
{
    auto sec_it = sections_.find(section);
    if (sec_it == sections_.end()) {
        VFSLogger::get_instance().debug(LOG_TAG,
            "get_keys: section [" + section + "] not found");
        return {};
    }

    return sec_it->second.key_order;
}

size_t VFSConfigParser::section_count() const
{
    return sections_.size();
}

size_t VFSConfigParser::total_key_count() const
{
    size_t count = 0;
    for (auto& pair : sections_) {
        count += pair.second.entries.size();
    }
    return count;
}

///////////////////////////////////////////////////////////////////////////////
// Mutation
///////////////////////////////////////////////////////////////////////////////

void VFSConfigParser::set_value(const std::string& section,
                                 const std::string& key,
                                 const std::string& value)
{
    // Create section if it doesn't exist.
    auto sec_it = sections_.find(section);
    if (sec_it == sections_.end()) {
        ConfigSection new_sec;
        new_sec.name = section;
        new_sec.source_line = 0;
        sections_[section] = std::move(new_sec);
        section_order_.push_back(section);

        sec_it = sections_.find(section);

        VFSLogger::get_instance().info(LOG_TAG,
            "set_value: created new section [" + section + "]");
    }

    auto& sec = sec_it->second;

    // Check if key already exists.
    bool is_new = (sec.entries.find(key) == sec.entries.end());

    ConfigEntry entry;
    entry.key = key;
    entry.value = value;
    entry.source_line = 0;  // Programmatically set, no source line.

    sec.entries[key] = std::move(entry);

    if (is_new) {
        sec.key_order.push_back(key);
        VFSLogger::get_instance().debug(LOG_TAG,
            "set_value: added [" + section + "] " + key + " = \"" + value + "\"");
    } else {
        VFSLogger::get_instance().debug(LOG_TAG,
            "set_value: updated [" + section + "] " + key + " = \"" + value + "\"");
    }
}

bool VFSConfigParser::remove_key(const std::string& section,
                                  const std::string& key)
{
    auto sec_it = sections_.find(section);
    if (sec_it == sections_.end()) return false;

    auto& sec = sec_it->second;
    auto key_it = sec.entries.find(key);
    if (key_it == sec.entries.end()) return false;

    sec.entries.erase(key_it);
    sec.key_order.erase(
        std::remove(sec.key_order.begin(), sec.key_order.end(), key),
        sec.key_order.end());

    VFSLogger::get_instance().info(LOG_TAG,
        "remove_key: removed '" + key + "' from [" + section + "]");
    return true;
}

bool VFSConfigParser::remove_section(const std::string& section)
{
    auto sec_it = sections_.find(section);
    if (sec_it == sections_.end()) return false;

    size_t key_count = sec_it->second.entries.size();
    sections_.erase(sec_it);

    section_order_.erase(
        std::remove(section_order_.begin(), section_order_.end(), section),
        section_order_.end());

    VFSLogger::get_instance().info(LOG_TAG,
        "remove_section: removed [" + section + "] with " +
        std::to_string(key_count) + " key(s)");
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// Serialization
///////////////////////////////////////////////////////////////////////////////

/// @brief Serialize the configuration back to INI format.
///
/// Output format:
///   - Sections are written in parse/insertion order.
///   - Within each section, keys are written in insertion order.
///   - The global section (keys before any [section]) are written first
///     without a header.
///   - Header comments above sections are preserved.
///   - Inline comments are preserved.
///   - Values containing special characters are quoted.
///   - Blank lines separate sections for readability.
std::string VFSConfigParser::serialize_to_string() const
{
    std::ostringstream oss;
    bool first_section = true;

    VFSLogger::get_instance().debug(LOG_TAG,
        "serialize_to_string: serializing " + std::to_string(sections_.size()) +
        " section(s)");

    for (const auto& sec_name : section_order_) {
        auto sec_it = sections_.find(sec_name);
        if (sec_it == sections_.end()) continue;

        const ConfigSection& sec = sec_it->second;

        // Skip empty sections to keep output clean.
        if (sec.entries.empty() && sec.name.empty()) continue;

        // Blank line between sections (except before the first).
        if (!first_section) {
            oss << "\n";
        }
        first_section = false;

        // Write header comment if present.
        if (!sec.header_comment.empty()) {
            oss << sec.header_comment << "\n";
        }

        // Write section header (skip for global/unnamed section).
        if (!sec.name.empty()) {
            oss << "[" << sec.name << "]\n";
        }

        // Write key-value pairs in insertion order.
        for (const auto& key_name : sec.key_order) {
            auto entry_it = sec.entries.find(key_name);
            if (entry_it == sec.entries.end()) continue;

            const ConfigEntry& entry = entry_it->second;

            // Determine if the value needs quoting. Quote if it contains:
            // - leading/trailing whitespace
            // - comment characters (# or ;)
            // - newlines or tabs
            // - the delimiter characters (= or :)
            bool needs_quoting = false;
            const std::string& val = entry.value;

            if (!val.empty()) {
                if (std::isspace(static_cast<unsigned char>(val.front())) ||
                    std::isspace(static_cast<unsigned char>(val.back())))
                {
                    needs_quoting = true;
                }

                for (char c : val) {
                    if (c == '#' || c == ';' || c == '\n' || c == '\t' ||
                        c == '"' || c == '\\')
                    {
                        needs_quoting = true;
                        break;
                    }
                }
            }

            oss << entry.key << " = ";

            if (needs_quoting) {
                oss << "\"";
                // Write value with escape sequences.
                for (char c : val) {
                    switch (c) {
                        case '\\': oss << "\\\\"; break;
                        case '"':  oss << "\\\""; break;
                        case '\n': oss << "\\n";  break;
                        case '\t': oss << "\\t";  break;
                        case '\r': oss << "\\r";  break;
                        default:   oss << c;      break;
                    }
                }
                oss << "\"";
            } else {
                oss << val;
            }

            // Write inline comment if present.
            if (!entry.inline_comment.empty()) {
                oss << " " << entry.inline_comment;
            }

            oss << "\n";
        }
    }

    std::string output = oss.str();

    VFSLogger::get_instance().info(LOG_TAG,
        "serialize_to_string: output is " + std::to_string(output.size()) +
        " character(s)");

    return output;
}
