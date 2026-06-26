/**
 * @file string_utils.h
 * @brief String utility functions for the FenrerVFS library.
 *
 * Provides a comprehensive set of static string manipulation helpers used
 * throughout the VFS codebase.  Every method is stateless and thread-safe
 * (no shared mutable state).
 *
 * Categories of functionality:
 *   - Whitespace trimming (trim, ltrim, rtrim)
 *   - Splitting / joining
 *   - Case conversion
 *   - Prefix / suffix tests
 *   - Search-and-replace
 *   - Encoding: URL percent-encoding, Base64 (RFC 4648), hex
 *   - Padding helpers
 *   - Character-class predicates
 *   - Human-readable byte formatting
 *   - Levenshtein edit distance
 *   - Glob-style pattern matching (* and ?)
 *
 * Copyright (c) 2026 Project Fenrer contributors.
 */

#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class VFSStringUtils {
public:
    // -----------------------------------------------------------------------
    //  Whitespace trimming
    // -----------------------------------------------------------------------

    /** Remove leading and trailing whitespace (space, tab, CR, LF). */
    static std::string trim(const std::string& s);

    /** Remove leading whitespace only. */
    static std::string ltrim(const std::string& s);

    /** Remove trailing whitespace only. */
    static std::string rtrim(const std::string& s);

    // -----------------------------------------------------------------------
    //  Splitting and joining
    // -----------------------------------------------------------------------

    /**
     * Split @p s around every occurrence of the single-character @p delimiter.
     * Two consecutive delimiters produce an empty element in the result.
     * An empty input string returns a single-element vector containing "".
     */
    static std::vector<std::string> split(const std::string& s, char delimiter);

    /**
     * Join all elements of @p parts with @p delimiter inserted between them.
     * An empty vector returns "".
     */
    static std::string join(const std::vector<std::string>& parts,
                            const std::string& delimiter);

    // -----------------------------------------------------------------------
    //  Case conversion
    // -----------------------------------------------------------------------

    /** Convert every ASCII letter to lowercase; non-ASCII bytes are untouched. */
    static std::string to_lower(const std::string& s);

    /** Convert every ASCII letter to uppercase; non-ASCII bytes are untouched. */
    static std::string to_upper(const std::string& s);

    // -----------------------------------------------------------------------
    //  Prefix / suffix tests
    // -----------------------------------------------------------------------

    /** True if @p s begins with @p prefix (byte-exact comparison). */
    static bool starts_with(const std::string& s, const std::string& prefix);

    /** True if @p s ends with @p suffix (byte-exact comparison). */
    static bool ends_with(const std::string& s, const std::string& suffix);

    // -----------------------------------------------------------------------
    //  Search and replace
    // -----------------------------------------------------------------------

    /**
     * Return a copy of @p s with every non-overlapping occurrence of @p from
     * replaced by @p to.  An empty @p from causes the original string to be
     * returned unchanged.
     */
    static std::string replace_all(const std::string& s,
                                   const std::string& from,
                                   const std::string& to);

    // -----------------------------------------------------------------------
    //  URL percent-encoding  (RFC 3986)
    // -----------------------------------------------------------------------

    /**
     * Percent-encode every byte that is *not* an unreserved character
     * (A-Z, a-z, 0-9, '-', '.', '_', '~').
     */
    static std::string url_encode(const std::string& s);

    /** Decode percent-encoded sequences (%XX).  Invalid sequences are kept
     *  literally. */
    static std::string url_decode(const std::string& s);

    // -----------------------------------------------------------------------
    //  Base64  (RFC 4648, standard alphabet, '=' padding)
    // -----------------------------------------------------------------------

    /** Encode raw bytes to a Base64 string using the standard alphabet. */
    static std::string base64_encode(const std::string& input);

    /** Decode a Base64 string back to raw bytes.
     *  Returns an empty string on malformed input. */
    static std::string base64_decode(const std::string& input);

    // -----------------------------------------------------------------------
    //  Hexadecimal encoding
    // -----------------------------------------------------------------------

    /** Encode each byte of @p input as two lowercase hex digits. */
    static std::string hex_encode(const std::string& input);

    /**
     * Decode pairs of hex digits back to bytes.
     * Returns "" if the input length is odd or contains non-hex characters.
     */
    static std::string hex_decode(const std::string& input);

    // -----------------------------------------------------------------------
    //  Padding
    // -----------------------------------------------------------------------

    /** Pad @p s on the left with @p pad_char until it reaches @p width.
     *  If s.size() >= width the string is returned unmodified. */
    static std::string pad_left(const std::string& s, size_t width,
                                char pad_char = ' ');

    /** Pad @p s on the right with @p pad_char until it reaches @p width. */
    static std::string pad_right(const std::string& s, size_t width,
                                 char pad_char = ' ');

    // -----------------------------------------------------------------------
    //  Character-class predicates  (ASCII only)
    // -----------------------------------------------------------------------

    /** True if every character is an ASCII digit ('0'-'9').
     *  Empty string → false. */
    static bool is_numeric(const std::string& s);

    /** True if every character is an ASCII letter ('A'-'Z','a'-'z').
     *  Empty string → false. */
    static bool is_alpha(const std::string& s);

    /** True if every character is a digit or ASCII letter.
     *  Empty string → false. */
    static bool is_alphanumeric(const std::string& s);

    // -----------------------------------------------------------------------
    //  Human-readable byte sizes
    // -----------------------------------------------------------------------

    /**
     * Format a byte count as a human-readable string.
     * Uses binary prefixes (1 KB = 1024 B).  Two decimal places are shown
     * for KB and above; plain bytes have no decimals.
     * Examples: "0 B", "512 B", "1.50 KB", "3.27 MB", "2.00 TB".
     */
    static std::string format_bytes(size_t bytes);

    // -----------------------------------------------------------------------
    //  Levenshtein edit distance
    // -----------------------------------------------------------------------

    /**
     * Compute the Levenshtein (edit) distance between two strings using a
     * full dynamic-programming matrix.  Cost model: insert = delete =
     * substitute = 1.
     */
    static size_t levenshtein_distance(const std::string& a,
                                       const std::string& b);

    // -----------------------------------------------------------------------
    //  Glob-style pattern matching
    // -----------------------------------------------------------------------

    /**
     * Match @p text against a glob @p pattern that supports:
     *   - '*' matches any sequence of characters (including empty).
     *   - '?' matches exactly one character.
     *   - All other characters are matched literally (case-sensitive).
     *
     * This is a recursive implementation with memoisation for acceptable
     * worst-case performance on pathological patterns.
     */
    static bool glob_match(const std::string& pattern, const std::string& text);

private:
    // Internal recursive helper for glob_match with memoisation index.
    static bool glob_match_impl(const std::string& pattern, size_t pi,
                                const std::string& text, size_t ti,
                                std::vector<int8_t>& memo, size_t text_len);

    VFSStringUtils() = delete;                           // purely static class
    VFSStringUtils(const VFSStringUtils&) = delete;
    VFSStringUtils& operator=(const VFSStringUtils&) = delete;
};

#endif // STRING_UTILS_H
