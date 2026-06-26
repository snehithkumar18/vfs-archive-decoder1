///////////////////////////////////////////////////////////////////////////////
/// @file   vfs_symlink.cc
/// @brief  Implementation of VFSSymlinkManager — symbolic and hard link
///         management for FenrerVFS.
///
/// This translation unit contains all the logic for creating, resolving,
/// removing, and querying links inside the virtual file system.  Every
/// mutating method canonicalizes its path arguments through VFSPathUtils
/// and logs through VFSLogger.
///
/// Hard-link reference counts are maintained in a separate map so that
/// deletion of a single hard link does not orphan the underlying target
/// until the last link is removed.
///
/// Cycle detection in resolve_chain() uses a visited-path set and a
/// bounded iteration count (MAX_CHAIN_DEPTH) so that even adversarial
/// configurations terminate quickly.
///////////////////////////////////////////////////////////////////////////////

#include "vfs_symlink.h"
#include "logger.h"
#include "path_utils.h"

#include <algorithm>
#include <sstream>
#include <ctime>

// ---------------------------------------------------------------------------
//  Tag used for all VFSLogger calls originating from this module.
// ---------------------------------------------------------------------------
static const std::string LOG_TAG = "VFSSymlinkManager";

// ===========================================================================
//  Construction / Destruction
// ===========================================================================

VFSSymlinkManager::VFSSymlinkManager() {
    VFSLogger::get_instance().debug(LOG_TAG,
        "Symlink manager initialised (max chain depth = "
        + std::to_string(MAX_CHAIN_DEPTH) + ")");
}

VFSSymlinkManager::~VFSSymlinkManager() {
    VFSLogger::get_instance().debug(LOG_TAG,
        "Symlink manager destroyed — "
        + std::to_string(link_table_.size()) + " link(s) discarded");
    link_table_.clear();
    ref_counts_.clear();
}

// ===========================================================================
//  Internal helpers
// ===========================================================================

/// Validate a raw path: must not be empty, must contain only allowed
/// characters, must not exceed MAX_LINK_PATH_LENGTH, and must start with '/'.
/// On success, @p canonical receives the canonicalized form.
bool VFSSymlinkManager::validate_and_canonicalize(
        const std::string& raw, std::string& canonical) const {

    // --- empty path is never valid ---
    if (raw.empty()) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "Path validation failed: empty string");
        return false;
    }

    // --- length guard ---
    if (raw.size() > MAX_LINK_PATH_LENGTH) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "Path validation failed: length " + std::to_string(raw.size())
            + " exceeds maximum " + std::to_string(MAX_LINK_PATH_LENGTH));
        return false;
    }

    // --- character validation ---
    if (!VFSPathUtils::validate_characters(raw)) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "Path validation failed: illegal characters in '" + raw + "'");
        return false;
    }

    // --- canonicalize ---
    canonical = VFSPathUtils::canonicalize(raw);
    if (canonical.empty()) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "Path validation failed: canonicalize returned empty for '"
            + raw + "'");
        return false;
    }

    // --- must be absolute after canonicalization ---
    if (!VFSPathUtils::is_absolute(canonical)) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "Path validation failed: '" + canonical + "' is not absolute");
        return false;
    }

    return true;
}

/// Walk the existing symlink graph starting from @p target and check
/// whether we would eventually reach @p link, which would mean that
/// adding the edge  link → target  creates a cycle.
bool VFSSymlinkManager::would_create_cycle(
        const std::string& target, const std::string& link) const {

    std::set<std::string> visited;
    std::string current = target;
    int depth = 0;

    while (depth < MAX_CHAIN_DEPTH) {
        // If we arrived back at the proposed link path, it's a cycle.
        if (current == link) {
            VFSLogger::get_instance().warn(LOG_TAG,
                "Cycle detected: link '" + link
                + "' → target '" + target
                + "' would create a loop at depth "
                + std::to_string(depth));
            return true;
        }

        // Have we already visited this node?  If so, cycle within
        // existing graph (doesn't involve @p link, but still bad).
        if (visited.count(current)) {
            break;  // existing cycle — not our problem to report here
        }
        visited.insert(current);

        // Follow the next indirection if one exists.
        auto it = link_table_.find(current);
        if (it == link_table_.end() || it->second.is_hard) {
            break;  // end of chain
        }

        current = it->second.target_path;
        ++depth;
    }

    return false;
}

void VFSSymlinkManager::increment_ref(const std::string& target) {
    auto it = ref_counts_.find(target);
    if (it == ref_counts_.end()) {
        ref_counts_[target] = HardlinkRefCount(target, 1);
        VFSLogger::get_instance().debug(LOG_TAG,
            "New ref-count entry for '" + target + "' = 1");
    } else {
        it->second.ref_count++;
        VFSLogger::get_instance().debug(LOG_TAG,
            "Incremented ref-count for '" + target + "' to "
            + std::to_string(it->second.ref_count));
    }
}

void VFSSymlinkManager::decrement_ref(const std::string& target) {
    auto it = ref_counts_.find(target);
    if (it == ref_counts_.end()) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "decrement_ref called for '" + target
            + "' which has no ref-count entry (ignored)");
        return;
    }

    if (it->second.ref_count == 0) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "Ref-count for '" + target
            + "' is already zero — clamping");
        return;
    }

    it->second.ref_count--;
    VFSLogger::get_instance().debug(LOG_TAG,
        "Decremented ref-count for '" + target + "' to "
        + std::to_string(it->second.ref_count));

    // If it reaches zero, clean up the entry itself.
    if (it->second.ref_count == 0) {
        VFSLogger::get_instance().info(LOG_TAG,
            "Ref-count for '" + target
            + "' reached zero — entry removed from tracking");
        ref_counts_.erase(it);
    }
}

// ===========================================================================
//  Link creation
// ===========================================================================

VFSErrorCode VFSSymlinkManager::create_symlink(
        const std::string& target_path,
        const std::string& link_path,
        const std::string& tag) {

    VFSLogger::get_instance().info(LOG_TAG,
        "create_symlink: '" + link_path + "' → '" + target_path + "'");

    // --- validate both paths ---
    std::string canon_target, canon_link;
    if (!validate_and_canonicalize(target_path, canon_target)) {
        VFSLogger::get_instance().error(LOG_TAG,
            "create_symlink: invalid target path '" + target_path + "'");
        return VFSErrorCode::ERROR_INVALID_PATH;
    }
    if (!validate_and_canonicalize(link_path, canon_link)) {
        VFSLogger::get_instance().error(LOG_TAG,
            "create_symlink: invalid link path '" + link_path + "'");
        return VFSErrorCode::ERROR_INVALID_PATH;
    }

    // --- cannot create a link whose name already exists ---
    if (link_table_.count(canon_link)) {
        VFSLogger::get_instance().error(LOG_TAG,
            "create_symlink: link '" + canon_link + "' already exists");
        return VFSErrorCode::ERROR_EXISTS;
    }

    // --- cannot link to yourself ---
    if (canon_target == canon_link) {
        VFSLogger::get_instance().error(LOG_TAG,
            "create_symlink: target and link are the same path '"
            + canon_link + "'");
        return VFSErrorCode::ERROR_GENERIC;
    }

    // --- cycle detection ---
    if (would_create_cycle(canon_target, canon_link)) {
        VFSLogger::get_instance().error(LOG_TAG,
            "create_symlink: would create a cycle — aborted");
        return VFSErrorCode::ERROR_GENERIC;
    }

    // --- store ---
    std::time_t now = std::time(nullptr);
    SymlinkEntry entry(canon_target, canon_link, /*hard=*/false, now, tag);
    link_table_[canon_link] = entry;

    VFSLogger::get_instance().info(LOG_TAG,
        "Symlink created: '" + canon_link + "' → '" + canon_target
        + "' (tag='" + tag + "', time=" + std::to_string(now) + ")");
    return VFSErrorCode::SUCCESS;
}

VFSErrorCode VFSSymlinkManager::create_hardlink(
        const std::string& target_path,
        const std::string& link_path,
        const std::string& tag) {

    VFSLogger::get_instance().info(LOG_TAG,
        "create_hardlink: '" + link_path + "' → '" + target_path + "'");

    // --- validate both paths ---
    std::string canon_target, canon_link;
    if (!validate_and_canonicalize(target_path, canon_target)) {
        VFSLogger::get_instance().error(LOG_TAG,
            "create_hardlink: invalid target path '" + target_path + "'");
        return VFSErrorCode::ERROR_INVALID_PATH;
    }
    if (!validate_and_canonicalize(link_path, canon_link)) {
        VFSLogger::get_instance().error(LOG_TAG,
            "create_hardlink: invalid link path '" + link_path + "'");
        return VFSErrorCode::ERROR_INVALID_PATH;
    }

    // --- link name must not already exist ---
    if (link_table_.count(canon_link)) {
        VFSLogger::get_instance().error(LOG_TAG,
            "create_hardlink: link '" + canon_link + "' already exists");
        return VFSErrorCode::ERROR_EXISTS;
    }

    // --- cannot hardlink to yourself ---
    if (canon_target == canon_link) {
        VFSLogger::get_instance().error(LOG_TAG,
            "create_hardlink: target and link are the same path '"
            + canon_link + "'");
        return VFSErrorCode::ERROR_GENERIC;
    }

    // --- target must not itself be a symlink (hard links point to real nodes) ---
    auto target_it = link_table_.find(canon_target);
    if (target_it != link_table_.end() && !target_it->second.is_hard) {
        VFSLogger::get_instance().error(LOG_TAG,
            "create_hardlink: target '" + canon_target
            + "' is a symlink — hard links must point to real nodes");
        return VFSErrorCode::ERROR_GENERIC;
    }

    // --- store ---
    std::time_t now = std::time(nullptr);
    SymlinkEntry entry(canon_target, canon_link, /*hard=*/true, now, tag);
    link_table_[canon_link] = entry;

    // --- update reference count ---
    increment_ref(canon_target);

    VFSLogger::get_instance().info(LOG_TAG,
        "Hardlink created: '" + canon_link + "' → '" + canon_target
        + "' (tag='" + tag + "', ref_count="
        + std::to_string(get_ref_count(canon_target)) + ")");
    return VFSErrorCode::SUCCESS;
}

// ===========================================================================
//  Link resolution
// ===========================================================================

VFSErrorCode VFSSymlinkManager::resolve_symlink(
        const std::string& path, std::string& resolved) const {

    VFSLogger::get_instance().debug(LOG_TAG,
        "resolve_symlink: '" + path + "'");

    // Canonicalize the input.
    std::string canon = VFSPathUtils::canonicalize(path);
    if (canon.empty()) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "resolve_symlink: failed to canonicalize '" + path + "'");
        resolved = path;    // best-effort: return the raw input
        return VFSErrorCode::ERROR_INVALID_PATH;
    }

    // Look up in the link table.
    auto it = link_table_.find(canon);
    if (it == link_table_.end()) {
        // Not a link — the path resolves to itself.
        resolved = canon;
        VFSLogger::get_instance().debug(LOG_TAG,
            "resolve_symlink: '" + canon + "' is not a link — identity");
        return VFSErrorCode::SUCCESS;
    }

    // Found — return the immediate target (one level).
    resolved = it->second.target_path;
    VFSLogger::get_instance().debug(LOG_TAG,
        "resolve_symlink: '" + canon + "' → '" + resolved + "' ("
        + (it->second.is_hard ? "hard" : "sym") + ")");
    return VFSErrorCode::SUCCESS;
}

VFSErrorCode VFSSymlinkManager::resolve_chain(
        const std::string& path, std::string& resolved) const {

    VFSLogger::get_instance().debug(LOG_TAG,
        "resolve_chain: starting at '" + path + "'");

    // Canonicalize the starting path.
    std::string current = VFSPathUtils::canonicalize(path);
    if (current.empty()) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "resolve_chain: failed to canonicalize '" + path + "'");
        resolved = path;
        return VFSErrorCode::ERROR_INVALID_PATH;
    }

    // Visited set for cycle detection.
    std::set<std::string> visited;
    int depth = 0;

    while (depth < MAX_CHAIN_DEPTH) {
        // Check for cycle.
        if (visited.count(current)) {
            VFSLogger::get_instance().error(LOG_TAG,
                "resolve_chain: cycle detected at '" + current
                + "' after " + std::to_string(depth) + " step(s)");
            resolved = current;
            return VFSErrorCode::ERROR_GENERIC;
        }
        visited.insert(current);

        // Look up the current path in the link table.
        auto it = link_table_.find(current);
        if (it == link_table_.end()) {
            // Not a link — resolution complete.
            resolved = current;
            VFSLogger::get_instance().info(LOG_TAG,
                "resolve_chain: '" + path + "' resolved to '"
                + resolved + "' in " + std::to_string(depth) + " step(s)");
            return VFSErrorCode::SUCCESS;
        }

        // Hard links resolve in one step (they share identity directly).
        if (it->second.is_hard) {
            resolved = it->second.target_path;
            VFSLogger::get_instance().info(LOG_TAG,
                "resolve_chain: hard link '" + current + "' → '"
                + resolved + "' (terminal)");
            return VFSErrorCode::SUCCESS;
        }

        // Follow the symlink.
        std::string next = it->second.target_path;
        VFSLogger::get_instance().debug(LOG_TAG,
            "resolve_chain: step " + std::to_string(depth)
            + ": '" + current + "' → '" + next + "'");
        current = next;
        ++depth;
    }

    // Exceeded maximum depth without finding a non-link terminal.
    VFSLogger::get_instance().error(LOG_TAG,
        "resolve_chain: max depth (" + std::to_string(MAX_CHAIN_DEPTH)
        + ") exceeded starting from '" + path
        + "' — possible infinite chain");
    resolved = current;
    return VFSErrorCode::ERROR_GENERIC;
}

// ===========================================================================
//  Link removal
// ===========================================================================

VFSErrorCode VFSSymlinkManager::remove_link(const std::string& link_path) {
    VFSLogger::get_instance().info(LOG_TAG,
        "remove_link: '" + link_path + "'");

    // Canonicalize.
    std::string canon = VFSPathUtils::canonicalize(link_path);
    if (canon.empty()) {
        VFSLogger::get_instance().error(LOG_TAG,
            "remove_link: failed to canonicalize '" + link_path + "'");
        return VFSErrorCode::ERROR_INVALID_PATH;
    }

    auto it = link_table_.find(canon);
    if (it == link_table_.end()) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "remove_link: '" + canon + "' not found in link table");
        return VFSErrorCode::ERROR_NOT_FOUND;
    }

    // If this was a hard link, decrement the reference count of its target.
    if (it->second.is_hard) {
        VFSLogger::get_instance().debug(LOG_TAG,
            "remove_link: '" + canon
            + "' is a hard link — decrementing ref-count for '"
            + it->second.target_path + "'");
        decrement_ref(it->second.target_path);
    }

    // Erase the link entry.
    std::string target_copy = it->second.target_path;
    bool was_hard = it->second.is_hard;
    link_table_.erase(it);

    VFSLogger::get_instance().info(LOG_TAG,
        "Link removed: '" + canon + "' → '" + target_copy
        + "' (" + (was_hard ? "hard" : "sym") + ")");
    return VFSErrorCode::SUCCESS;
}

// ===========================================================================
//  Queries
// ===========================================================================

std::vector<SymlinkEntry> VFSSymlinkManager::list_links() const {
    std::vector<SymlinkEntry> result;
    result.reserve(link_table_.size());

    for (auto& kv : link_table_) {
        result.push_back(kv.second);
    }

    // Sort by creation time, oldest first.  Ties broken by link path.
    std::sort(result.begin(), result.end(),
        [](const SymlinkEntry& a, const SymlinkEntry& b) {
            if (a.creation_time != b.creation_time)
                return a.creation_time < b.creation_time;
            return a.link_path < b.link_path;
        });

    VFSLogger::get_instance().debug(LOG_TAG,
        "list_links: returning " + std::to_string(result.size()) + " entries");
    return result;
}

std::vector<SymlinkEntry> VFSSymlinkManager::list_symlinks() const {
    std::vector<SymlinkEntry> result;
    for (auto& kv : link_table_) {
        if (!kv.second.is_hard) {
            result.push_back(kv.second);
        }
    }

    std::sort(result.begin(), result.end(),
        [](const SymlinkEntry& a, const SymlinkEntry& b) {
            if (a.creation_time != b.creation_time)
                return a.creation_time < b.creation_time;
            return a.link_path < b.link_path;
        });

    VFSLogger::get_instance().debug(LOG_TAG,
        "list_symlinks: returning " + std::to_string(result.size())
        + " symlink(s)");
    return result;
}

std::vector<SymlinkEntry> VFSSymlinkManager::list_hardlinks() const {
    std::vector<SymlinkEntry> result;
    for (auto& kv : link_table_) {
        if (kv.second.is_hard) {
            result.push_back(kv.second);
        }
    }

    std::sort(result.begin(), result.end(),
        [](const SymlinkEntry& a, const SymlinkEntry& b) {
            if (a.creation_time != b.creation_time)
                return a.creation_time < b.creation_time;
            return a.link_path < b.link_path;
        });

    VFSLogger::get_instance().debug(LOG_TAG,
        "list_hardlinks: returning " + std::to_string(result.size())
        + " hard link(s)");
    return result;
}

bool VFSSymlinkManager::is_symlink(const std::string& path) const {
    std::string canon = VFSPathUtils::canonicalize(path);
    if (canon.empty()) {
        return false;
    }

    auto it = link_table_.find(canon);
    if (it == link_table_.end()) {
        return false;
    }

    // Return true for either kind of link.  Use is_hardlink() for
    // hard-link-specific checks.
    VFSLogger::get_instance().debug(LOG_TAG,
        "is_symlink: '" + canon + "' = true ("
        + (it->second.is_hard ? "hard" : "sym") + ")");
    return true;
}

bool VFSSymlinkManager::is_hardlink(const std::string& path) const {
    std::string canon = VFSPathUtils::canonicalize(path);
    if (canon.empty()) {
        return false;
    }

    auto it = link_table_.find(canon);
    if (it == link_table_.end()) {
        return false;
    }

    return it->second.is_hard;
}

VFSErrorCode VFSSymlinkManager::get_link_target(
        const std::string& path, std::string& target) const {

    VFSLogger::get_instance().debug(LOG_TAG,
        "get_link_target: '" + path + "'");

    std::string canon = VFSPathUtils::canonicalize(path);
    if (canon.empty()) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "get_link_target: failed to canonicalize '" + path + "'");
        return VFSErrorCode::ERROR_INVALID_PATH;
    }

    auto it = link_table_.find(canon);
    if (it == link_table_.end()) {
        VFSLogger::get_instance().warn(LOG_TAG,
            "get_link_target: '" + canon + "' is not a link");
        return VFSErrorCode::ERROR_NOT_FOUND;
    }

    target = it->second.target_path;
    VFSLogger::get_instance().debug(LOG_TAG,
        "get_link_target: '" + canon + "' → '" + target + "'");
    return VFSErrorCode::SUCCESS;
}

uint32_t VFSSymlinkManager::get_ref_count(const std::string& target_path) const {
    std::string canon = VFSPathUtils::canonicalize(target_path);
    if (canon.empty()) {
        return 0;
    }

    auto it = ref_counts_.find(canon);
    if (it == ref_counts_.end()) {
        return 0;
    }
    return it->second.ref_count;
}

size_t VFSSymlinkManager::total_links() const {
    return link_table_.size();
}

void VFSSymlinkManager::clear() {
    size_t count = link_table_.size();
    link_table_.clear();
    ref_counts_.clear();
    VFSLogger::get_instance().info(LOG_TAG,
        "clear: removed " + std::to_string(count) + " link(s)");
}
