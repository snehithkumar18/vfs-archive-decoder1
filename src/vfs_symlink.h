///////////////////////////////////////////////////////////////////////////////
/// @file   vfs_symlink.h
/// @brief  Symbolic link and hard link management for FenrerVFS.
///
/// Provides a VFSSymlinkManager class that maintains a registry of symbolic
/// and hard links within the virtual file system.  Symbolic links store a
/// target path string and are resolved lazily at access time.  Hard links
/// share an underlying reference-counted identity so that a file remains
/// accessible as long as at least one hard link points to it.
///
/// Key design decisions:
///   - Symlink chains are resolved iteratively up to MAX_CHAIN_DEPTH (10)
///     to prevent infinite loops caused by circular references.
///   - Cycle detection uses a visited-set algorithm during chain resolution.
///   - Hard link reference counts are tracked in a separate map keyed by
///     the canonical target path, allowing O(1) ref-count lookup.
///   - All mutating operations validate paths through VFSPathUtils and log
///     their actions through VFSLogger.
///////////////////////////////////////////////////////////////////////////////

#ifndef VFS_SYMLINK_H
#define VFS_SYMLINK_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstdint>
#include <ctime>

#include "errors.h"

/// Maximum number of symlink indirections that resolve_chain will follow
/// before declaring a cycle or excessive-depth error.
static constexpr int MAX_CHAIN_DEPTH = 10;

/// Maximum allowed length for a link path or target path.  Prevents
/// degenerate memory consumption from extremely long path strings.
static constexpr size_t MAX_LINK_PATH_LENGTH = 4096;

///////////////////////////////////////////////////////////////////////////////
/// @struct SymlinkEntry
/// @brief  One link record inside the VFS link table.
///
/// Both symbolic and hard links are stored through this struct.  The
/// `is_hard` flag distinguishes the two kinds.  For symbolic links the
/// target may itself be another link (forming a chain); for hard links the
/// target must resolve to an existing non-link node.
///////////////////////////////////////////////////////////////////////////////
struct SymlinkEntry {
    /// Canonical path of the entity that this link points to.
    std::string target_path;

    /// Canonical path of the link itself (the name the user sees).
    std::string link_path;

    /// True if this is a hard link; false for a symbolic link.
    bool is_hard;

    /// UNIX timestamp of creation.  Used for sorting in list_links().
    std::time_t creation_time;

    /// Human-readable tag for debugging (e.g. "user-created", "auto").
    std::string tag;

    SymlinkEntry()
        : is_hard(false), creation_time(0) {}

    SymlinkEntry(const std::string& target,
                 const std::string& link,
                 bool hard,
                 std::time_t ctime,
                 const std::string& t = "")
        : target_path(target),
          link_path(link),
          is_hard(hard),
          creation_time(ctime),
          tag(t) {}
};

///////////////////////////////////////////////////////////////////////////////
/// @struct HardlinkRefCount
/// @brief  Tracks how many hard links point to a given canonical target.
///
/// When the count drops to zero the target is eligible for deletion by the
/// upper VFS layer (this manager only tracks counts, it does not delete
/// VFS nodes directly).
///////////////////////////////////////////////////////////////////////////////
struct HardlinkRefCount {
    /// Canonical path of the target.
    std::string canonical_path;

    /// Number of hard links currently pointing at this target.
    uint32_t ref_count;

    HardlinkRefCount()
        : ref_count(0) {}

    explicit HardlinkRefCount(const std::string& path, uint32_t count = 1)
        : canonical_path(path), ref_count(count) {}
};

///////////////////////////////////////////////////////////////////////////////
/// @class VFSSymlinkManager
/// @brief Manages all symbolic and hard links in a FenrerVFS instance.
///
/// Thread-safety: none.  The caller (typically the VFS class) is responsible
/// for any required synchronisation.
///////////////////////////////////////////////////////////////////////////////
class VFSSymlinkManager {
public:
    VFSSymlinkManager();
    ~VFSSymlinkManager();

    // ---------------------------------------------------------------
    //  Link creation
    // ---------------------------------------------------------------

    /// Create a symbolic link at @p link_path pointing to @p target_path.
    /// @return SUCCESS, ERROR_EXISTS, ERROR_INVALID_PATH, or ERROR_GENERIC.
    VFSErrorCode create_symlink(const std::string& target_path,
                                const std::string& link_path,
                                const std::string& tag = "");

    /// Create a hard link at @p link_path pointing to @p target_path.
    /// The caller must ensure that target_path refers to an existing
    /// non-directory node.
    /// @return SUCCESS, ERROR_EXISTS, ERROR_INVALID_PATH, ERROR_NOT_FOUND.
    VFSErrorCode create_hardlink(const std::string& target_path,
                                 const std::string& link_path,
                                 const std::string& tag = "");

    // ---------------------------------------------------------------
    //  Link resolution
    // ---------------------------------------------------------------

    /// Resolve a single level of indirection.  If @p path is not a
    /// symlink the output is the path itself.
    /// @param[out] resolved  The target of the link (one level).
    /// @return SUCCESS or ERROR_NOT_FOUND.
    VFSErrorCode resolve_symlink(const std::string& path,
                                 std::string& resolved) const;

    /// Iteratively resolve a chain of symlinks up to MAX_CHAIN_DEPTH.
    /// Detects cycles via a visited-path set.
    /// @param[out] resolved  The final resolved path.
    /// @return SUCCESS, ERROR_NOT_FOUND, or ERROR_GENERIC (cycle).
    VFSErrorCode resolve_chain(const std::string& path,
                               std::string& resolved) const;

    // ---------------------------------------------------------------
    //  Link removal
    // ---------------------------------------------------------------

    /// Remove a link entry.  For hard links the reference count of the
    /// underlying target is decremented.
    /// @return SUCCESS or ERROR_NOT_FOUND.
    VFSErrorCode remove_link(const std::string& link_path);

    // ---------------------------------------------------------------
    //  Queries
    // ---------------------------------------------------------------

    /// Return all link entries sorted by creation time (oldest first).
    std::vector<SymlinkEntry> list_links() const;

    /// Return only the symlinks (not hard links) sorted by creation time.
    std::vector<SymlinkEntry> list_symlinks() const;

    /// Return only the hard links sorted by creation time.
    std::vector<SymlinkEntry> list_hardlinks() const;

    /// Check whether @p path is registered as any kind of link.
    bool is_symlink(const std::string& path) const;

    /// Check whether @p path is specifically a hard link.
    bool is_hardlink(const std::string& path) const;

    /// Retrieve the immediate target of a link (one-level, no chaining).
    /// @param[out] target  Populated on success.
    /// @return SUCCESS or ERROR_NOT_FOUND.
    VFSErrorCode get_link_target(const std::string& path,
                                 std::string& target) const;

    /// Return the current hard-link reference count for @p target_path.
    /// Returns 0 if no hard links point to that target.
    uint32_t get_ref_count(const std::string& target_path) const;

    /// Total number of link entries (symlinks + hardlinks).
    size_t total_links() const;

    /// Remove every link entry and reset all reference counts.
    void clear();

private:
    // ---------------------------------------------------------------
    //  Internal helpers
    // ---------------------------------------------------------------

    /// Validate and canonicalize a raw path string.
    /// @param[out] canonical  The cleaned path.
    /// @return true if the path is usable.
    bool validate_and_canonicalize(const std::string& raw,
                                   std::string& canonical) const;

    /// Quick cycle check: would adding a link from @p link to @p target
    /// create a cycle in the existing symlink graph?
    bool would_create_cycle(const std::string& target,
                            const std::string& link) const;

    /// Increment the hard-link reference count for @p target.
    void increment_ref(const std::string& target);

    /// Decrement the hard-link reference count for @p target.
    /// Clamps at zero.
    void decrement_ref(const std::string& target);

    // ---------------------------------------------------------------
    //  Data
    // ---------------------------------------------------------------

    /// Primary link table.  Key = canonical link_path.
    std::map<std::string, SymlinkEntry> link_table_;

    /// Hard-link reference counts.  Key = canonical target path.
    std::map<std::string, HardlinkRefCount> ref_counts_;
};

#endif // VFS_SYMLINK_H
