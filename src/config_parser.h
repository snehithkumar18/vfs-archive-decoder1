///////////////////////////////////////////////////////////////////////////////
/// @file config_parser.h
/// @brief VFSConfigParser — INI-style configuration file parser for FenrerVFS
///
/// Parses configuration data in INI format:
///   [section]
///   key = value
///   # comment
///   ; another comment
///
/// Features:
///   - Hierarchical sections with key-value pairs
///   - Comment lines (# and ;) and inline comments
///   - Quoted string values (preserves internal whitespace)
///   - Backslash line continuation for multiline values
///   - Escape sequences (\n, \t, \\, \", \#, \;)
///   - Type-safe accessors: string, int, bool, double
///   - Duplicate keys: last definition wins (with warning)
///   - Round-trip serialization back to INI text
///
/// Thread safety: NOT thread-safe. External synchronization required.
///////////////////////////////////////////////////////////////////////////////

#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

///////////////////////////////////////////////////////////////////////////////
/// @struct ConfigEntry
/// @brief A single key-value pair within a section, with optional metadata.
///////////////////////////////////////////////////////////////////////////////
struct ConfigEntry {
    std::string key;             ///< The key name (case-preserved)
    std::string value;           ///< The raw string value (after parsing)
    std::string inline_comment;  ///< Inline comment that followed the value
    size_t source_line;          ///< 1-based line number where key was defined
};

///////////////////////////////////////////////////////////////////////////////
/// @struct ConfigSection
/// @brief A named section containing ordered key-value pairs.
///////////////////////////////////////////////////////////////////////////////
struct ConfigSection {
    std::string name;                           ///< Section name (without brackets)
    std::map<std::string, ConfigEntry> entries;  ///< Key -> entry mapping
    std::vector<std::string> key_order;          ///< Insertion order for serialization
    std::string header_comment;                  ///< Comment line(s) above the [section]
    size_t source_line;                          ///< Line where [section] was defined

    /// @brief Check if this section contains a key.
    bool has_key(const std::string& key) const;

    /// @brief Get the value for a key, or empty string if missing.
    const std::string& get_value(const std::string& key) const;
};

///////////////////////////////////////////////////////////////////////////////
/// @struct ConfigParseResult
/// @brief Result of a parse operation with diagnostics.
///////////////////////////////////////////////////////////////////////////////
struct ConfigParseResult {
    bool success;                ///< True if parsing completed without fatal errors
    size_t lines_parsed;        ///< Total lines processed
    size_t entries_parsed;      ///< Total key-value pairs found
    size_t sections_parsed;     ///< Total [section] headers found
    size_t warnings;            ///< Non-fatal issues (duplicate keys, etc.)
    std::vector<std::string> warning_messages;  ///< Human-readable warning details
};

///////////////////////////////////////////////////////////////////////////////
/// @class VFSConfigParser
/// @brief Parses and manages INI-style configuration data.
///
/// Typical usage:
///   VFSConfigParser config;
///   auto result = config.parse_string(ini_text);
///   if (result.success) {
///       int port = config.get_int("server", "port", 8080);
///       std::string host = config.get_string("server", "host", "localhost");
///   }
///   std::string output = config.serialize_to_string();
///////////////////////////////////////////////////////////////////////////////
class VFSConfigParser {
public:
    //=========================================================================
    // Construction
    //=========================================================================

    VFSConfigParser();
    ~VFSConfigParser();

    //=========================================================================
    // Parsing
    //=========================================================================

    /// @brief Parse INI-formatted text from a string.
    /// Clears any previously parsed data before parsing.
    /// @param data  The INI text to parse.
    /// @return Parse result with diagnostics.
    ConfigParseResult parse_string(const std::string& data);

    /// @brief Parse INI-formatted text from a raw byte buffer.
    /// The buffer is interpreted as UTF-8 text.
    /// @param buffer  Pointer to the data.
    /// @param size    Size of the data in bytes.
    /// @return Parse result with diagnostics.
    ConfigParseResult parse_file(const uint8_t* buffer, size_t size);

    /// @brief Clear all parsed data, resetting to empty state.
    void clear();

    //=========================================================================
    // Type-safe value accessors
    //=========================================================================

    /// @brief Get a string value.
    /// @param section   Section name.
    /// @param key       Key name.
    /// @param default_val  Value returned if section/key not found.
    std::string get_string(const std::string& section,
                           const std::string& key,
                           const std::string& default_val = "") const;

    /// @brief Get an integer value (supports decimal, hex 0x, octal 0o, binary 0b).
    /// @param section   Section name.
    /// @param key       Key name.
    /// @param default_val  Value returned if section/key not found or not parseable.
    int64_t get_int(const std::string& section,
                    const std::string& key,
                    int64_t default_val = 0) const;

    /// @brief Get a boolean value.
    /// True values: "true", "yes", "on", "1"
    /// False values: "false", "no", "off", "0"
    /// @param section   Section name.
    /// @param key       Key name.
    /// @param default_val  Value returned if section/key not found or ambiguous.
    bool get_bool(const std::string& section,
                  const std::string& key,
                  bool default_val = false) const;

    /// @brief Get a floating-point value.
    /// @param section   Section name.
    /// @param key       Key name.
    /// @param default_val  Value returned if section/key not found or not parseable.
    double get_double(const std::string& section,
                      const std::string& key,
                      double default_val = 0.0) const;

    //=========================================================================
    // Existence checks
    //=========================================================================

    /// @brief Check if a section exists.
    bool has_section(const std::string& section) const;

    /// @brief Check if a key exists within a section.
    bool has_key(const std::string& section, const std::string& key) const;

    //=========================================================================
    // Enumeration
    //=========================================================================

    /// @brief Get list of all section names in parse order.
    std::vector<std::string> get_sections() const;

    /// @brief Get list of all keys in a section, in parse order.
    /// Returns empty vector if section does not exist.
    std::vector<std::string> get_keys(const std::string& section) const;

    /// @brief Get number of sections.
    size_t section_count() const;

    /// @brief Get number of keys across all sections.
    size_t total_key_count() const;

    //=========================================================================
    // Mutation
    //=========================================================================

    /// @brief Set (add or update) a key-value pair in a section.
    /// Creates the section if it does not exist.
    /// @param section  Section name.
    /// @param key      Key name.
    /// @param value    String value to set.
    void set_value(const std::string& section,
                   const std::string& key,
                   const std::string& value);

    /// @brief Remove a key from a section.
    /// @return true if the key existed and was removed.
    bool remove_key(const std::string& section, const std::string& key);

    /// @brief Remove an entire section and all its keys.
    /// @return true if the section existed and was removed.
    bool remove_section(const std::string& section);

    //=========================================================================
    // Serialization
    //=========================================================================

    /// @brief Serialize the current configuration back to INI format.
    /// Sections and keys are written in their original parse order,
    /// with newly added items appended at the end.
    /// @return The INI-formatted string.
    std::string serialize_to_string() const;

private:
    //=========================================================================
    // Internal parsing helpers
    //=========================================================================

    /// @brief Trim leading and trailing whitespace from a string.
    static std::string trim(const std::string& str);

    /// @brief Process escape sequences in a value string (\n, \t, \\, etc.).
    static std::string process_escapes(const std::string& str);

    /// @brief Extract a quoted string value, handling escape sequences.
    /// @param str    Input string starting at the opening quote.
    /// @param[out] result  The extracted, unescaped string.
    /// @return Number of characters consumed (including quotes), or 0 on error.
    static size_t extract_quoted_string(const std::string& str, std::string& result);

    /// @brief Check if a character starts a comment (# or ;) outside quotes.
    static bool is_comment_char(char c);

    /// @brief Convert a string to lowercase for case-insensitive comparisons.
    static std::string to_lower(const std::string& str);

    //=========================================================================
    // State
    //=========================================================================

    /// Ordered map of section name -> ConfigSection.
    std::map<std::string, ConfigSection> sections_;

    /// Section names in parse/insertion order (for serialization).
    std::vector<std::string> section_order_;

    /// Name of the "global" section for keys defined before any [section] header.
    static constexpr const char* GLOBAL_SECTION = "";
};

#endif // CONFIG_PARSER_H
