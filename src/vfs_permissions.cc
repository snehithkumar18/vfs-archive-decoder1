///////////////////////////////////////////////////////////////////////////////
/// @file   vfs_permissions.cc
/// @brief  Implementation of VFSPermissions — UNIX-style permission
///         management for FenrerVFS.
///
/// This translation unit implements:
///   - Permission storage and retrieval with defaults
///   - Numeric chmod (e.g. 0755)
///   - Symbolic chmod with full parsing  (u+rwx, g=rx, o-w, a+r, etc.)
///   - Access checking against owner/group/other with special root bypass
///   - Conversions between PermissionEntry, octal uint16_t, and string
///
/// All mutating operations canonicalize paths via VFSPathUtils and log
/// through VFSLogger.
///////////////////////////////////////////////////////////////////////////////

#include "vfs_permissions.h"
#include "logger.h"
#include "path_utils.h"

#include <algorithm>
#include <sstream>
#include <cctype>

// ---------------------------------------------------------------------------
//  Tag used for all VFSLogger calls from this module.
// ---------------------------------------------------------------------------
static const std::string LOG_TAG = "VFSPermissions";

// ===========================================================================
//  Construction / Destruction
// ===========================================================================

VFSPermissions::VFSPermissions() {
    VFSLogger::get_instance().debug(LOG_TAG,
        "Permission manager initialised");
}

VFSPermissions::~VFSPermissions() {
    VFSLogger::get_instance().debug(LOG_TAG,
        "Permission manager destroyed — "
        + std::to_string(perm_table_.size()) + " entry(ies) discarded");
    perm_table_.clear();
}

// ===========================================================================
//  Internal helpers
// ===========================================================================

bool VFSPermissions::validate_path(
        const std::string& raw, std::string& canon) const {

    if (raw.empty()) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "validate_path: empty path string");
        return false;
    }

    if (!VFSPathUtils::validate_characters(raw)) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "validate_path: illegal characters in '" + raw + "'");
        return false;
    }

    canon = VFSPathUtils::canonicalize(raw);
    if (canon.empty()) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "validate_path: canonicalize returned empty for '" + raw + "'");
        return false;
    }

    if (!VFSPathUtils::is_absolute(canon)) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "validate_path: '" + canon + "' is not absolute");
        return false;
    }

    return true;
}

/// Parse a single symbolic chmod clause.  A clause has the form:
///     [who...][op][perms...]
///
/// where:
///   who  = one or more of {u, g, o, a}
///   op   = one of {+, -, =}
///   perms = one or more of {r, w, x}
///
/// Example clauses: "u+rw", "go-x", "a=rx", "o+w".
///
/// The '=' operator clears the targeted bits first, then sets the
/// specified ones.  '+' only sets; '-' only clears.
bool VFSPermissions::apply_symbolic_clause(
        const std::string& clause, PermissionEntry& entry) const {

    if (clause.empty()) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "apply_symbolic_clause: empty clause");
        return false;
    }

    // ---- 1.  Parse the "who" characters --------------------------------
    size_t pos = 0;
    bool affect_owner = false;
    bool affect_group = false;
    bool affect_other = false;

    while (pos < clause.size()) {
        char ch = clause[pos];
        if      (ch == 'u') { affect_owner = true; ++pos; }
        else if (ch == 'g') { affect_group = true; ++pos; }
        else if (ch == 'o') { affect_other = true; ++pos; }
        else if (ch == 'a') {
            affect_owner = true;
            affect_group = true;
            affect_other = true;
            ++pos;
        }
        else {
            break;  // end of who-specifiers
        }
    }

    // If no who-specifier was given, default to 'a' (all).
    if (!affect_owner && !affect_group && !affect_other) {
        affect_owner = true;
        affect_group = true;
        affect_other = true;
        VFSLogger::get_instance().debug(LOG_TAG,
            "apply_symbolic_clause: no who-specifier — defaulting to 'a'");
    }

    // ---- 2.  Parse the operator ----------------------------------------
    if (pos >= clause.size()) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "apply_symbolic_clause: missing operator in '" + clause + "'");
        return false;
    }

    char op = clause[pos];
    if (op != '+' && op != '-' && op != '=') {
        VFSLogger::get_instance().warn(LOG_TAG,
            "apply_symbolic_clause: invalid operator '"
            + std::string(1, op) + "' in '" + clause + "'");
        return false;
    }
    ++pos;

    // ---- 3.  Parse the permission characters ---------------------------
    bool perm_r = false;
    bool perm_w = false;
    bool perm_x = false;

    while (pos < clause.size()) {
        char ch = clause[pos];
        if      (ch == 'r') { perm_r = true; }
        else if (ch == 'w') { perm_w = true; }
        else if (ch == 'x') { perm_x = true; }
        else {
            VFSLogger::get_instance().warn(LOG_TAG,
                "apply_symbolic_clause: unexpected character '"
                + std::string(1, ch) + "' in '" + clause + "'");
            return false;
        }
        ++pos;
    }

    VFSLogger::get_instance().debug(LOG_TAG,
        "apply_symbolic_clause: who=[" +
        std::string(affect_owner ? "u" : "") +
        std::string(affect_group ? "g" : "") +
        std::string(affect_other ? "o" : "") +
        "] op=" + std::string(1, op) +
        " perms=[" +
        std::string(perm_r ? "r" : "") +
        std::string(perm_w ? "w" : "") +
        std::string(perm_x ? "x" : "") + "]");

    // ---- 4.  Apply the operation to the entry --------------------------

    // Helper lambda: for a single who-category, apply the operator to the
    // three bits (read, write, execute).
    auto apply = [&](bool& bit_r, bool& bit_w, bool& bit_x) {
        switch (op) {
            case '+':
                if (perm_r) bit_r = true;
                if (perm_w) bit_w = true;
                if (perm_x) bit_x = true;
                break;
            case '-':
                if (perm_r) bit_r = false;
                if (perm_w) bit_w = false;
                if (perm_x) bit_x = false;
                break;
            case '=':
                // '=' clears all three, then sets the specified ones.
                bit_r = perm_r;
                bit_w = perm_w;
                bit_x = perm_x;
                break;
        }
    };

    if (affect_owner) apply(entry.owner_read, entry.owner_write, entry.owner_exec);
    if (affect_group) apply(entry.group_read, entry.group_write, entry.group_exec);
    if (affect_other) apply(entry.other_read, entry.other_write, entry.other_exec);

    return true;
}

// ===========================================================================
//  Permission storage
// ===========================================================================

VFSErrorCode VFSPermissions::set_permissions(
        const std::string& path, const PermissionEntry& entry) {

    std::string canon;
    if (!validate_path(path, canon)) {
        VFSLogger::get_instance().error(LOG_TAG,
            "set_permissions: invalid path '" + path + "'");
        return VFSErrorCode::ERROR_INVALID_PATH;
    }

    perm_table_[canon] = entry;

    VFSLogger::get_instance().info(LOG_TAG,
        "set_permissions: '" + canon + "' → "
        + to_string(entry) + " (octal "
        + std::to_string(to_octal(entry)) + ")"
        + " uid=" + std::to_string(entry.owner_uid)
        + " gid=" + std::to_string(entry.group_gid));
    return VFSErrorCode::SUCCESS;
}

VFSErrorCode VFSPermissions::get_permissions(
        const std::string& path, PermissionEntry& entry) const {

    std::string canon;
    if (!validate_path(path, canon)) {
        // Return defaults for invalid paths as well — the API doc says
        // this always returns SUCCESS.
        entry = PermissionEntry();
        VFSLogger::get_instance().debug(LOG_TAG,
            "get_permissions: invalid path '" + path
            + "' — returning defaults");
        return VFSErrorCode::SUCCESS;
    }

    auto it = perm_table_.find(canon);
    if (it != perm_table_.end()) {
        entry = it->second;
        VFSLogger::get_instance().debug(LOG_TAG,
            "get_permissions: '" + canon + "' = " + to_string(entry));
    } else {
        entry = PermissionEntry();   // default 0644
        VFSLogger::get_instance().debug(LOG_TAG,
            "get_permissions: '" + canon
            + "' not found — returning defaults "
            + to_string(entry));
    }

    return VFSErrorCode::SUCCESS;
}

VFSErrorCode VFSPermissions::chown(
        const std::string& path, uint32_t uid, uint32_t gid) {

    std::string canon;
    if (!validate_path(path, canon)) {
        VFSLogger::get_instance().error(LOG_TAG,
            "chown: invalid path '" + path + "'");
        return VFSErrorCode::ERROR_INVALID_PATH;
    }

    // If there is no entry yet, create one with defaults and then update.
    auto it = perm_table_.find(canon);
    if (it == perm_table_.end()) {
        perm_table_[canon] = PermissionEntry();
        it = perm_table_.find(canon);
    }

    uint32_t old_uid = it->second.owner_uid;
    uint32_t old_gid = it->second.group_gid;
    it->second.owner_uid = uid;
    it->second.group_gid = gid;

    VFSLogger::get_instance().info(LOG_TAG,
        "chown: '" + canon + "' uid "
        + std::to_string(old_uid) + " → " + std::to_string(uid)
        + ", gid "
        + std::to_string(old_gid) + " → " + std::to_string(gid));
    return VFSErrorCode::SUCCESS;
}

// ===========================================================================
//  Access checking
// ===========================================================================

bool VFSPermissions::check_access(
        const std::string& path,
        uint32_t uid, uint32_t gid,
        uint8_t mode) const {

    VFSLogger::get_instance().debug(LOG_TAG,
        "check_access: path='" + path
        + "' uid=" + std::to_string(uid)
        + " gid=" + std::to_string(gid)
        + " mode=" + std::to_string(static_cast<int>(mode)));

    // Root (uid 0) always passes.
    if (uid == VFS_ROOT_UID) {
        VFSLogger::get_instance().debug(LOG_TAG,
            "check_access: uid is root — access granted unconditionally");
        return true;
    }

    // F_OK (existence check) — succeeds if the path has an entry or if we
    // are willing to provide defaults.
    if (mode == VFS_F_OK) {
        VFSLogger::get_instance().debug(LOG_TAG,
            "check_access: F_OK — existence check passes");
        return true;
    }

    // Retrieve the permission entry (defaults are returned for unknown paths).
    PermissionEntry entry;
    get_permissions(path, entry);

    // Determine which category applies: owner → group → other.
    //   - If uid matches owner_uid → use owner bits.
    //   - Else if gid matches group_gid → use group bits.
    //   - Else → use other bits.
    bool cat_r = false;
    bool cat_w = false;
    bool cat_x = false;
    std::string cat_name;

    if (uid == entry.owner_uid) {
        cat_r = entry.owner_read;
        cat_w = entry.owner_write;
        cat_x = entry.owner_exec;
        cat_name = "owner";
    } else if (gid == entry.group_gid) {
        cat_r = entry.group_read;
        cat_w = entry.group_write;
        cat_x = entry.group_exec;
        cat_name = "group";
    } else {
        cat_r = entry.other_read;
        cat_w = entry.other_write;
        cat_x = entry.other_exec;
        cat_name = "other";
    }

    // Check each requested mode bit.
    bool granted = true;

    if ((mode & VFS_R_OK) && !cat_r) {
        VFSLogger::get_instance().debug(LOG_TAG,
            "check_access: read denied for " + cat_name);
        granted = false;
    }
    if ((mode & VFS_W_OK) && !cat_w) {
        VFSLogger::get_instance().debug(LOG_TAG,
            "check_access: write denied for " + cat_name);
        granted = false;
    }
    if ((mode & VFS_X_OK) && !cat_x) {
        VFSLogger::get_instance().debug(LOG_TAG,
            "check_access: execute denied for " + cat_name);
        granted = false;
    }

    VFSLogger::get_instance().info(LOG_TAG,
        "check_access: path='" + path + "' category=" + cat_name
        + " result=" + (granted ? "GRANTED" : "DENIED"));
    return granted;
}

// ===========================================================================
//  chmod — numeric
// ===========================================================================

VFSErrorCode VFSPermissions::chmod_numeric(
        const std::string& path, uint16_t mode) {

    VFSLogger::get_instance().info(LOG_TAG,
        "chmod_numeric: path='" + path + "' mode="
        + std::to_string(mode));

    std::string canon;
    if (!validate_path(path, canon)) {
        VFSLogger::get_instance().error(LOG_TAG,
            "chmod_numeric: invalid path '" + path + "'");
        return VFSErrorCode::ERROR_INVALID_PATH;
    }

    // Mask to the valid 9 permission bits.
    uint16_t masked = mode & VFS_S_ALL;
    if (masked != mode) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "chmod_numeric: mode " + std::to_string(mode)
            + " had bits outside 0777 — masked to "
            + std::to_string(masked));
    }

    // Retrieve existing entry (or create defaults) so we preserve
    // uid/gid and is_directory.
    auto it = perm_table_.find(canon);
    PermissionEntry entry;
    if (it != perm_table_.end()) {
        entry = it->second;
    }
    // else entry starts with defaults (uid=1000, gid=1000, dir=false)

    // Apply the mode bits.
    entry.owner_read  = (masked & VFS_S_IRUSR) != 0;
    entry.owner_write = (masked & VFS_S_IWUSR) != 0;
    entry.owner_exec  = (masked & VFS_S_IXUSR) != 0;

    entry.group_read  = (masked & VFS_S_IRGRP) != 0;
    entry.group_write = (masked & VFS_S_IWGRP) != 0;
    entry.group_exec  = (masked & VFS_S_IXGRP) != 0;

    entry.other_read  = (masked & VFS_S_IROTH) != 0;
    entry.other_write = (masked & VFS_S_IWOTH) != 0;
    entry.other_exec  = (masked & VFS_S_IXOTH) != 0;

    perm_table_[canon] = entry;

    VFSLogger::get_instance().info(LOG_TAG,
        "chmod_numeric: '" + canon + "' → " + to_string(entry));
    return VFSErrorCode::SUCCESS;
}

// ===========================================================================
//  chmod — symbolic
// ===========================================================================

VFSErrorCode VFSPermissions::chmod_symbolic(
        const std::string& path, const std::string& symbolic) {

    VFSLogger::get_instance().info(LOG_TAG,
        "chmod_symbolic: path='" + path + "' symbolic='" + symbolic + "'");

    std::string canon;
    if (!validate_path(path, canon)) {
        VFSLogger::get_instance().error(LOG_TAG,
            "chmod_symbolic: invalid path '" + path + "'");
        return VFSErrorCode::ERROR_INVALID_PATH;
    }

    if (symbolic.empty()) {
        VFSLogger::get_instance().error(LOG_TAG,
            "chmod_symbolic: empty symbolic string");
        return VFSErrorCode::ERROR_GENERIC;
    }

    // Retrieve existing entry (or defaults).
    auto it = perm_table_.find(canon);
    PermissionEntry entry;
    if (it != perm_table_.end()) {
        entry = it->second;
    }

    // ---- Split by commas and apply each clause -------------------------
    //
    // The input may look like "u+rw,g=rx,o-w".  We tokenize on ','
    // and apply each clause in left-to-right order.

    std::vector<std::string> clauses;
    {
        std::string current_clause;
        for (char ch : symbolic) {
            if (ch == ',') {
                if (!current_clause.empty()) {
                    clauses.push_back(current_clause);
                    current_clause.clear();
                }
            } else if (!std::isspace(static_cast<unsigned char>(ch))) {
                // Skip whitespace silently.
                current_clause += ch;
            }
        }
        if (!current_clause.empty()) {
            clauses.push_back(current_clause);
        }
    }

    if (clauses.empty()) {
        VFSLogger::get_instance().error(LOG_TAG,
            "chmod_symbolic: no valid clauses parsed from '"
            + symbolic + "'");
        return VFSErrorCode::ERROR_GENERIC;
    }

    VFSLogger::get_instance().debug(LOG_TAG,
        "chmod_symbolic: " + std::to_string(clauses.size())
        + " clause(s) to apply");

    for (size_t i = 0; i < clauses.size(); ++i) {
        VFSLogger::get_instance().debug(LOG_TAG,
            "chmod_symbolic: applying clause [" + std::to_string(i)
            + "] '" + clauses[i] + "'");

        if (!apply_symbolic_clause(clauses[i], entry)) {
            VFSLogger::get_instance().error(LOG_TAG,
                "chmod_symbolic: failed to apply clause '"
                + clauses[i] + "'");
            return VFSErrorCode::ERROR_GENERIC;
        }
    }

    perm_table_[canon] = entry;

    VFSLogger::get_instance().info(LOG_TAG,
        "chmod_symbolic: '" + canon + "' → " + to_string(entry)
        + " (octal " + std::to_string(to_octal(entry)) + ")");
    return VFSErrorCode::SUCCESS;
}

// ===========================================================================
//  Conversions
// ===========================================================================

std::string VFSPermissions::to_string(const PermissionEntry& entry) {
    // Build a 10-character string: type + 9 permission chars.
    //   d|l|-  rwx  rwx  rwx
    //   [0]    [1-3][4-6][7-9]

    std::string result;
    result.reserve(10);

    // Position 0: type indicator.
    result += entry.is_directory ? 'd' : '-';

    // Owner bits.
    result += entry.owner_read  ? 'r' : '-';
    result += entry.owner_write ? 'w' : '-';
    result += entry.owner_exec  ? 'x' : '-';

    // Group bits.
    result += entry.group_read  ? 'r' : '-';
    result += entry.group_write ? 'w' : '-';
    result += entry.group_exec  ? 'x' : '-';

    // Other bits.
    result += entry.other_read  ? 'r' : '-';
    result += entry.other_write ? 'w' : '-';
    result += entry.other_exec  ? 'x' : '-';

    return result;
}

PermissionEntry VFSPermissions::from_octal(uint16_t mode) {
    // Mask to valid range.
    uint16_t masked = mode & VFS_S_ALL;

    PermissionEntry entry;
    entry.owner_read  = (masked & VFS_S_IRUSR) != 0;
    entry.owner_write = (masked & VFS_S_IWUSR) != 0;
    entry.owner_exec  = (masked & VFS_S_IXUSR) != 0;

    entry.group_read  = (masked & VFS_S_IRGRP) != 0;
    entry.group_write = (masked & VFS_S_IWGRP) != 0;
    entry.group_exec  = (masked & VFS_S_IXGRP) != 0;

    entry.other_read  = (masked & VFS_S_IROTH) != 0;
    entry.other_write = (masked & VFS_S_IWOTH) != 0;
    entry.other_exec  = (masked & VFS_S_IXOTH) != 0;

    return entry;
}

uint16_t VFSPermissions::to_octal(const PermissionEntry& entry) {
    uint16_t mode = 0;

    if (entry.owner_read)  mode |= VFS_S_IRUSR;
    if (entry.owner_write) mode |= VFS_S_IWUSR;
    if (entry.owner_exec)  mode |= VFS_S_IXUSR;

    if (entry.group_read)  mode |= VFS_S_IRGRP;
    if (entry.group_write) mode |= VFS_S_IWGRP;
    if (entry.group_exec)  mode |= VFS_S_IXGRP;

    if (entry.other_read)  mode |= VFS_S_IROTH;
    if (entry.other_write) mode |= VFS_S_IWOTH;
    if (entry.other_exec)  mode |= VFS_S_IXOTH;

    return mode;
}

// ===========================================================================
//  Queries
// ===========================================================================

size_t VFSPermissions::total_entries() const {
    return perm_table_.size();
}

VFSErrorCode VFSPermissions::remove_entry(const std::string& path) {
    std::string canon;
    if (!validate_path(path, canon)) {
        VFSLogger::get_instance().error(LOG_TAG,
            "remove_entry: invalid path '" + path + "'");
        return VFSErrorCode::ERROR_INVALID_PATH;
    }

    auto it = perm_table_.find(canon);
    if (it == perm_table_.end()) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "remove_entry: no entry for '" + canon + "'");
        return VFSErrorCode::ERROR_NOT_FOUND;
    }

    perm_table_.erase(it);
    VFSLogger::get_instance().info(LOG_TAG,
        "remove_entry: removed permissions for '" + canon + "'");
    return VFSErrorCode::SUCCESS;
}

void VFSPermissions::clear() {
    size_t count = perm_table_.size();
    perm_table_.clear();
    VFSLogger::get_instance().info(LOG_TAG,
        "clear: removed " + std::to_string(count) + " permission entry(ies)");
}

std::vector<std::string> VFSPermissions::list_paths() const {
    std::vector<std::string> paths;
    paths.reserve(perm_table_.size());

    for (auto& kv : perm_table_) {
        paths.push_back(kv.first);
    }

    // Already sorted because std::map keys are ordered, but let's be
    // explicit for documentation clarity.
    std::sort(paths.begin(), paths.end());

    VFSLogger::get_instance().debug(LOG_TAG,
        "list_paths: returning " + std::to_string(paths.size()) + " path(s)");
    return paths;
}
