/**
 * @file string_utils.cc
 * @brief Implementation of VFSStringUtils – string helper functions for
 *        FenrerVFS.
 *
 * Every function is a self-contained, stateless utility.  Encoding routines
 * follow the relevant RFCs (Base64: RFC 4648, URL: RFC 3986).  The
 * Levenshtein distance uses a full (m+1)×(n+1) DP matrix, and glob matching
 * uses recursion with memoisation so that pathological patterns like
 * "a]***...***b" run in O(P·T) time rather than exponential.
 *
 * Copyright (c) 2026 Project Fenrer contributors.
 */

#include "string_utils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <stdexcept>

// ===================================================================
//  Whitespace trimming
// ===================================================================

static inline bool is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string VFSStringUtils::ltrim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && is_ws(s[start])) {
        ++start;
    }
    return s.substr(start);
}

std::string VFSStringUtils::rtrim(const std::string& s) {
    if (s.empty()) return s;
    size_t end = s.size();
    while (end > 0 && is_ws(s[end - 1])) {
        --end;
    }
    return s.substr(0, end);
}

std::string VFSStringUtils::trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && is_ws(s[start])) {
        ++start;
    }
    if (start == s.size()) return "";

    size_t end = s.size();
    while (end > start && is_ws(s[end - 1])) {
        --end;
    }
    return s.substr(start, end - start);
}

// ===================================================================
//  Splitting and joining
// ===================================================================

std::vector<std::string> VFSStringUtils::split(const std::string& s,
                                                char delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;

    // Walk through the string, finding each delimiter.  Two consecutive
    // delimiters yield an empty token – this is intentional so that the
    // caller can detect "missing" fields.
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == delimiter) {
            tokens.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return tokens;
}

std::string VFSStringUtils::join(const std::vector<std::string>& parts,
                                  const std::string& delimiter) {
    if (parts.empty()) return "";

    // Pre-calculate total length to avoid repeated reallocations.
    size_t total = 0;
    for (size_t i = 0; i < parts.size(); ++i) {
        total += parts[i].size();
        if (i + 1 < parts.size()) total += delimiter.size();
    }

    std::string result;
    result.reserve(total);
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += delimiter;
        result += parts[i];
    }
    return result;
}

// ===================================================================
//  Case conversion
// ===================================================================

std::string VFSStringUtils::to_lower(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (unsigned char c : s) {
        // Only touch ASCII letters; leave everything else alone.
        if (c >= 'A' && c <= 'Z') {
            result += static_cast<char>(c + ('a' - 'A'));
        } else {
            result += static_cast<char>(c);
        }
    }
    return result;
}

std::string VFSStringUtils::to_upper(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (unsigned char c : s) {
        if (c >= 'a' && c <= 'z') {
            result += static_cast<char>(c - ('a' - 'A'));
        } else {
            result += static_cast<char>(c);
        }
    }
    return result;
}

// ===================================================================
//  Prefix / suffix tests
// ===================================================================

bool VFSStringUtils::starts_with(const std::string& s,
                                  const std::string& prefix) {
    if (prefix.size() > s.size()) return false;
    return s.compare(0, prefix.size(), prefix) == 0;
}

bool VFSStringUtils::ends_with(const std::string& s,
                                const std::string& suffix) {
    if (suffix.size() > s.size()) return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// ===================================================================
//  Search and replace
// ===================================================================

std::string VFSStringUtils::replace_all(const std::string& s,
                                         const std::string& from,
                                         const std::string& to) {
    if (from.empty()) return s;   // avoid infinite loop

    std::string result;
    result.reserve(s.size());     // optimistic – usually close enough

    size_t pos = 0;
    size_t found;
    while ((found = s.find(from, pos)) != std::string::npos) {
        result.append(s, pos, found - pos);
        result += to;
        pos = found + from.size();
    }
    result.append(s, pos, s.size() - pos);
    return result;
}

// ===================================================================
//  URL percent-encoding  (RFC 3986)
// ===================================================================

// Characters that do NOT need to be percent-encoded (unreserved set).
static inline bool is_unreserved(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '.' || c == '_' || c == '~';
}

// Convert a single hex digit character to its integer value (0-15).
// Returns -1 on invalid input.
static inline int hex_digit_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

std::string VFSStringUtils::url_encode(const std::string& s) {
    static const char hex_chars[] = "0123456789ABCDEF";

    std::string result;
    result.reserve(s.size() * 3);  // worst case: every byte encoded

    for (unsigned char c : s) {
        if (is_unreserved(c)) {
            result += static_cast<char>(c);
        } else {
            result += '%';
            result += hex_chars[(c >> 4) & 0x0F];
            result += hex_chars[c & 0x0F];
        }
    }
    return result;
}

std::string VFSStringUtils::url_decode(const std::string& s) {
    std::string result;
    result.reserve(s.size());

    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int hi = hex_digit_value(s[i + 1]);
            int lo = hex_digit_value(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                result += static_cast<char>((hi << 4) | lo);
                i += 2;  // skip the two hex digits
                continue;
            }
        }
        // '+' is sometimes used as a space in query strings (HTML form
        // encoding), but RFC 3986 does not mandate that.  We keep '+' as
        // '+' for strict compliance; callers can replace_all("+", " ")
        // beforehand if needed.
        result += s[i];
    }
    return result;
}

// ===================================================================
//  Base64  (RFC 4648, standard alphabet, '=' padding)
// ===================================================================

// The standard Base64 alphabet (indices 0-63).
static const char BASE64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

// Reverse lookup table: maps a byte value to its Base64 index, or -1 if
// the byte is not part of the alphabet.  Built once on first use.
static const int8_t* base64_decode_table() {
    static int8_t table[256];
    static bool initialised = false;
    if (!initialised) {
        for (int i = 0; i < 256; ++i) table[i] = -1;
        for (int i = 0; i < 64; ++i) {
            table[static_cast<unsigned char>(BASE64_CHARS[i])] =
                static_cast<int8_t>(i);
        }
        initialised = true;
    }
    return table;
}

std::string VFSStringUtils::base64_encode(const std::string& input) {
    std::string output;
    // Every 3 input bytes → 4 output characters.
    size_t encoded_len = 4 * ((input.size() + 2) / 3);
    output.reserve(encoded_len);

    size_t i = 0;
    while (i + 2 < input.size()) {
        // Full 3-byte group → 4 base64 characters.
        uint32_t triplet =
            (static_cast<uint8_t>(input[i])     << 16) |
            (static_cast<uint8_t>(input[i + 1]) <<  8) |
            (static_cast<uint8_t>(input[i + 2]));
        output += BASE64_CHARS[(triplet >> 18) & 0x3F];
        output += BASE64_CHARS[(triplet >> 12) & 0x3F];
        output += BASE64_CHARS[(triplet >>  6) & 0x3F];
        output += BASE64_CHARS[(triplet)       & 0x3F];
        i += 3;
    }

    // Handle the 1- or 2-byte tail with '=' padding.
    size_t remaining = input.size() - i;
    if (remaining == 1) {
        uint32_t b0 = static_cast<uint8_t>(input[i]);
        output += BASE64_CHARS[(b0 >> 2) & 0x3F];
        output += BASE64_CHARS[(b0 << 4) & 0x3F];
        output += '=';
        output += '=';
    } else if (remaining == 2) {
        uint32_t b0 = static_cast<uint8_t>(input[i]);
        uint32_t b1 = static_cast<uint8_t>(input[i + 1]);
        output += BASE64_CHARS[(b0 >> 2) & 0x3F];
        output += BASE64_CHARS[((b0 << 4) | (b1 >> 4)) & 0x3F];
        output += BASE64_CHARS[(b1 << 2) & 0x3F];
        output += '=';
    }

    return output;
}

std::string VFSStringUtils::base64_decode(const std::string& input) {
    const int8_t* table = base64_decode_table();
    std::string output;

    // Strip trailing '=' padding and whitespace for length calculation.
    size_t len = input.size();
    while (len > 0 && (input[len - 1] == '=' || input[len - 1] == '\n' ||
                        input[len - 1] == '\r')) {
        --len;
    }

    output.reserve((len * 3) / 4);

    // Accumulate 4 base64 digits at a time into a 24-bit group, then emit
    // the corresponding 1-3 bytes.
    uint32_t accum = 0;
    int bits_collected = 0;

    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);

        // Skip whitespace and padding characters.
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;

        int8_t val = table[c];
        if (val < 0) {
            // Invalid character – return empty to signal error.
            return "";
        }

        accum = (accum << 6) | static_cast<uint32_t>(val);
        bits_collected += 6;

        if (bits_collected >= 8) {
            bits_collected -= 8;
            output += static_cast<char>((accum >> bits_collected) & 0xFF);
        }
    }

    return output;
}

// ===================================================================
//  Hexadecimal encoding
// ===================================================================

std::string VFSStringUtils::hex_encode(const std::string& input) {
    static const char hex_chars[] = "0123456789abcdef";
    std::string result;
    result.reserve(input.size() * 2);

    for (unsigned char c : input) {
        result += hex_chars[(c >> 4) & 0x0F];
        result += hex_chars[c & 0x0F];
    }
    return result;
}

std::string VFSStringUtils::hex_decode(const std::string& input) {
    if (input.size() % 2 != 0) return "";   // odd length → error

    std::string result;
    result.reserve(input.size() / 2);

    for (size_t i = 0; i < input.size(); i += 2) {
        int hi = hex_digit_value(input[i]);
        int lo = hex_digit_value(input[i + 1]);
        if (hi < 0 || lo < 0) return "";    // invalid hex digit
        result += static_cast<char>((hi << 4) | lo);
    }
    return result;
}

// ===================================================================
//  Padding
// ===================================================================

std::string VFSStringUtils::pad_left(const std::string& s, size_t width,
                                      char pad_char) {
    if (s.size() >= width) return s;
    return std::string(width - s.size(), pad_char) + s;
}

std::string VFSStringUtils::pad_right(const std::string& s, size_t width,
                                       char pad_char) {
    if (s.size() >= width) return s;
    return s + std::string(width - s.size(), pad_char);
}

// ===================================================================
//  Character-class predicates
// ===================================================================

bool VFSStringUtils::is_numeric(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

bool VFSStringUtils::is_alpha(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) return false;
    }
    return true;
}

bool VFSStringUtils::is_alphanumeric(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s) {
        bool digit = (c >= '0' && c <= '9');
        bool alpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        if (!digit && !alpha) return false;
    }
    return true;
}

// ===================================================================
//  Human-readable byte sizes
// ===================================================================

std::string VFSStringUtils::format_bytes(size_t bytes) {
    // Binary prefixes: 1 KB = 1024 B, 1 MB = 1024 KB, …
    static const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    static const size_t num_units = sizeof(units) / sizeof(units[0]);

    if (bytes == 0) return "0 B";

    // Find the largest unit where the value is >= 1.0.
    double value = static_cast<double>(bytes);
    size_t unit_index = 0;
    while (unit_index + 1 < num_units && value >= 1024.0) {
        value /= 1024.0;
        ++unit_index;
    }

    // Plain bytes → no decimals; everything else → 2 decimal places.
    char buf[64];
    if (unit_index == 0) {
        std::snprintf(buf, sizeof(buf), "%zu B", bytes);
    } else {
        std::snprintf(buf, sizeof(buf), "%.2f %s", value, units[unit_index]);
    }
    return std::string(buf);
}

// ===================================================================
//  Levenshtein edit distance (full DP matrix)
// ===================================================================

size_t VFSStringUtils::levenshtein_distance(const std::string& a,
                                             const std::string& b) {
    const size_t m = a.size();
    const size_t n = b.size();

    // Trivial cases.
    if (m == 0) return n;
    if (n == 0) return m;

    // Allocate a (m+1) × (n+1) matrix stored in row-major order.
    // dp[i][j] = edit distance between a[0..i-1] and b[0..j-1].
    std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1, 0));

    // Base cases: transforming an empty string into a prefix costs its length.
    for (size_t i = 0; i <= m; ++i) dp[i][0] = i;
    for (size_t j = 0; j <= n; ++j) dp[0][j] = j;

    // Fill the matrix row by row.
    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            // Cost of substitution: 0 if the characters already match.
            size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;

            // Standard recurrence: minimum of deletion, insertion,
            // substitution.
            size_t del_cost = dp[i - 1][j] + 1;
            size_t ins_cost = dp[i][j - 1] + 1;
            size_t sub_cost = dp[i - 1][j - 1] + cost;

            dp[i][j] = std::min({del_cost, ins_cost, sub_cost});
        }
    }

    return dp[m][n];
}

// ===================================================================
//  Glob-style pattern matching
// ===================================================================

bool VFSStringUtils::glob_match(const std::string& pattern,
                                 const std::string& text) {
    // Memo table: -1 = unvisited, 0 = no match, 1 = match.
    // Indexed as memo[pi * (text_len+1) + ti].
    size_t text_len = text.size();
    size_t pat_len  = pattern.size();
    std::vector<int8_t> memo((pat_len + 1) * (text_len + 1), -1);

    return glob_match_impl(pattern, 0, text, 0, memo, text_len);
}

bool VFSStringUtils::glob_match_impl(const std::string& pattern, size_t pi,
                                      const std::string& text, size_t ti,
                                      std::vector<int8_t>& memo,
                                      size_t text_len) {
    // Compute the flat index into the memo table.
    size_t idx = pi * (text_len + 1) + ti;
    if (memo[idx] != -1) {
        return memo[idx] == 1;
    }

    bool result = false;
    size_t pat_len = pattern.size();

    if (pi == pat_len) {
        // Pattern exhausted – match only if text is also exhausted.
        result = (ti == text_len);
    } else if (pattern[pi] == '*') {
        // '*' matches zero or more characters.
        // Optimisation: collapse consecutive '*'s – they are equivalent to
        // a single '*'.
        size_t next_pi = pi;
        while (next_pi < pat_len && pattern[next_pi] == '*') {
            ++next_pi;
        }

        // Try matching zero characters (skip '*' entirely) …
        if (glob_match_impl(pattern, next_pi, text, ti, memo, text_len)) {
            result = true;
        } else {
            // … or consume one character from text and keep the '*' active.
            // We iterate rather than recurse for the "consume-one" step to
            // limit stack depth on long texts.
            for (size_t k = ti + 1; k <= text_len && !result; ++k) {
                if (glob_match_impl(pattern, next_pi, text, k, memo,
                                    text_len)) {
                    result = true;
                }
            }
        }
    } else if (ti < text_len) {
        if (pattern[pi] == '?') {
            // '?' matches exactly one character.
            result = glob_match_impl(pattern, pi + 1, text, ti + 1, memo,
                                     text_len);
        } else if (pattern[pi] == text[ti]) {
            // Literal match.
            result = glob_match_impl(pattern, pi + 1, text, ti + 1, memo,
                                     text_len);
        }
        // else: mismatch → result stays false.
    }
    // else: pattern not exhausted but text is, and pattern[pi] != '*'
    //       → no match, result stays false.

    memo[idx] = result ? 1 : 0;
    return result;
}
