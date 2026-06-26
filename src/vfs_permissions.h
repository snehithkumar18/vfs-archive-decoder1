///////////////////////////////////////////////////////////////////////////////
/// @file   vfs_permissions.h
/// @brief  UNIX-style file permission management for FenrerVFS.
///
/// Models the classic owner / group / other × read / write / execute
/// permission matrix.  Each path in the VFS may have an associated
/// PermissionEntry that records nine permission bits plus numeric owner
/// and group identifiers.
///
/// The interface mirrors the POSIX chmod(2) / stat(2) semantics:
///   - Numeric modes   (e.g. 0755)
///   - Symbolic modes   (e.g. u+rwx,g=rx,o-w)
///   - Access checks    (read / write / execute for a given uid/gid)
///   - String rendering (e.g. "rwxr-xr-x")
///
/// All operations validate their inputs and log through VFSLogger.
///////////////////////////////////////////////////////////////////////////////

#ifndef VFS_PERMISSIONS_H
#define VFS_PERMISSIONS_H

#include <string>
#include <map>
#include <cstdint>
#include <vector>

#include "errors.h"

// -----------------------------------------------------------------------
//  POSIX-style permission bit constants
// -----------------------------------------------------------------------

/// Owner read.
static constexpr uint16_t VFS_S_IRUSR = 0400;
/// Owner write.
static constexpr uint16_t VFS_S_IWUSR = 0200;
/// Owner execute.
static constexpr uint16_t VFS_S_IXUSR = 0100;

/// Group read.
static constexpr uint16_t VFS_S_IRGRP = 0040;
/// Group write.
static constexpr uint16_t VFS_S_IWGRP = 0020;
/// Group execute.
static constexpr uint16_t VFS_S_IXGRP = 0010;

/// Other read.
static constexpr uint16_t VFS_S_IROTH = 0004;
/// Other write.
static constexpr uint16_t VFS_S_IWOTH = 0002;
/// Other execute.
static constexpr uint16_t VFS_S_IXOTH = 0001;

/// Convenience: all owner bits.
static constexpr uint16_t VFS_S_IRWXU = VFS_S_IRUSR | VFS_S_IWUSR | VFS_S_IXUSR;
/// Convenience: all group bits.
static constexpr uint16_t VFS_S_IRWXG = VFS_S_IRGRP | VFS_S_IWGRP | VFS_S_IXGRP;
/// Convenience: all other bits.
static constexpr uint16_t VFS_S_IRWXO = VFS_S_IROTH | VFS_S_IWOTH | VFS_S_IXOTH;

/// All nine permission bits combined.
static constexpr uint16_t VFS_S_ALL   = VFS_S_IRWXU | VFS_S_IRWXG | VFS_S_IRWXO;

// -----------------------------------------------------------------------
//  Access-check mode flags  (passed to check_access)
// -----------------------------------------------------------------------

/// Check for read permission.
static constexpr uint8_t VFS_R_OK = 0x04;
/// Check for write permission.
static constexpr uint8_t VFS_W_OK = 0x02;
/// Check for execute permission.
static constexpr uint8_t VFS_X_OK = 0x01;
/// Check for existence only (always succeeds if path has an entry).
static constexpr uint8_t VFS_F_OK = 0x00;

/// Superuser UID — always passes access checks.
static constexpr uint32_t VFS_ROOT_UID = 0;

/// Default owner UID assigned to new entries when none is specified.
static constexpr uint32_t VFS_DEFAULT_UID = 1000;
/// Default group GID assigned to new entries when none is specified.
static constexpr uint32_t VFS_DEFAULT_GID = 1000;

/// Maximum valid octal mode value (0777 == 511 decimal).
static constexpr uint16_t VFS_MAX_MODE = 0777;

///////////////////////////////////////////////////////////////////////////////
/// @struct PermissionEntry
/// @brief  Stores the full permission state for one VFS path.
///////////////////////////////////////////////////////////////////////////////
struct PermissionEntry {
    // Owner permission bits
    bool owner_read;
    bool owner_write;
    bool owner_exec;

    // Group permission bits
    bool group_read;
    bool group_write;
    bool group_exec;

    // Other (world) permission bits
    bool other_read;
    bool other_write;
    bool other_exec;

    // Ownership identifiers
    uint32_t owner_uid;
    uint32_t group_gid;

    /// True if this entry represents a directory (affects to_string prefix).
    bool is_directory;

    /// Construct with default permissions 0644, uid=1000, gid=1000.
    PermissionEntry()
        : owner_read(true),  owner_write(true),  owner_exec(false),
          group_read(true),  group_write(false),  group_exec(false),
          other_read(true),  other_write(false),  other_exec(false),
          owner_uid(VFS_DEFAULT_UID),
          group_gid(VFS_DEFAULT_GID),
          is_directory(false) {}

    /// Construct from an explicit octal mode.
    explicit PermissionEntry(uint16_t mode,
                             uint32_t uid = VFS_DEFAULT_UID,
                             uint32_t gid = VFS_DEFAULT_GID,
                             bool dir = false)
        : owner_read  ((mode & VFS_S_IRUSR) != 0),
          owner_write ((mode & VFS_S_IWUSR) != 0),
          owner_exec  ((mode & VFS_S_IXUSR) != 0),
          group_read  ((mode & VFS_S_IRGRP) != 0),
          group_write ((mode & VFS_S_IWGRP) != 0),
          group_exec  ((mode & VFS_S_IXGRP) != 0),
          other_read  ((mode & VFS_S_IROTH) != 0),
          other_write ((mode & VFS_S_IWOTH) != 0),
          other_exec  ((mode & VFS_S_IXOTH) != 0),
          owner_uid(uid),
          group_gid(gid),
          is_directory(dir) {}
};

///////////////////////////////////////////////////////////////////////////////
/// @class VFSPermissions
/// @brief Manages UNIX-style permissions for every path in a VFS instance.
///
/// Thread-safety: none.  The caller must synchronise externally.
///////////////////////////////////////////////////////////////////////////////
class VFSPermissions {
public:
    VFSPermissions();
    ~VFSPermissions();

    // ---------------------------------------------------------------
    //  Permission storage
    // ---------------------------------------------------------------

    /// Stores (or overwrites) the permission entry for @p path.
    /// @return SUCCESS or ERROR_INVALID_PATH.
    VFSErrorCode set_permissions(const std::string& path,
                                 const PermissionEntry& entry);

    /// Retrieves the permission entry for @p path.
    /// If no entry exists a default (0644, uid 1000, gid 1000) is returned.
    /// @param[out] entry  Populated on return.
    /// @return SUCCESS (always — defaults are provided for missing paths).
    VFSErrorCode get_permissions(const std::string& path,
                                 PermissionEntry& entry) const;

    /// Set ownership (uid + gid) for a path.
    VFSErrorCode chown(const std::string& path,
                       uint32_t uid, uint32_t gid);

    // ---------------------------------------------------------------
    //  Access checking
    // ---------------------------------------------------------------

    /// Check whether user @p uid in group @p gid may perform the
    /// operations described by @p mode (VFS_R_OK | VFS_W_OK | VFS_X_OK).
    /// @return true if access is permitted.
    bool check_access(const std::string& path,
                      uint32_t uid, uint32_t gid,
                      uint8_t mode) const;

    // ---------------------------------------------------------------
    //  chmod — numeric
    // ---------------------------------------------------------------

    /// Set permissions from a numeric octal mode (e.g. 0755).
    /// Only the low 9 bits are used.
    /// @return SUCCESS or ERROR_INVALID_PATH.
    VFSErrorCode chmod_numeric(const std::string& path, uint16_t mode);

    // ---------------------------------------------------------------
    //  chmod — symbolic
    // ---------------------------------------------------------------

    /// Parse and apply a symbolic mode string such as "u+rwx,g=rx,o-w".
    /// Multiple clauses may be comma-separated.
    /// @return SUCCESS, ERROR_INVALID_PATH, or ERROR_GENERIC on parse error.
    VFSErrorCode chmod_symbolic(const std::string& path,
                                const std::string& symbolic);

    // ---------------------------------------------------------------
    //  Conversions
    // ---------------------------------------------------------------

    /// Convert a PermissionEntry to the familiar "drwxr-xr-x" string.
    static std::string to_string(const PermissionEntry& entry);

    /// Build a PermissionEntry from a numeric octal mode.
    static PermissionEntry from_octal(uint16_t mode);

    /// Convert a PermissionEntry back to its numeric octal mode.
    static uint16_t to_octal(const PermissionEntry& entry);

    // ---------------------------------------------------------------
    //  Queries
    // ---------------------------------------------------------------

    /// Number of paths that have explicit permission entries.
    size_t total_entries() const;

    /// Remove the permission entry for @p path.
    /// @return SUCCESS or ERROR_NOT_FOUND.
    VFSErrorCode remove_entry(const std::string& path);

    /// Remove all stored permission entries.
    void clear();

    /// Return all paths that have explicit entries (sorted).
    std::vector<std::string> list_paths() const;

private:
    // ---------------------------------------------------------------
    //  Internal helpers
    // ---------------------------------------------------------------

    /// Validate and canonicalize a path before use.
    bool validate_path(const std::string& raw, std::string& canon) const;

    /// Apply one symbolic clause (e.g. "u+rw") to @p entry.
    /// @return true on success.
    bool apply_symbolic_clause(const std::string& clause,
                               PermissionEntry& entry) const;

    // ---------------------------------------------------------------
    //  Data
    // ---------------------------------------------------------------

    /// Permission table.  Key = canonical path.
    std::map<std::string, PermissionEntry> perm_table_;
};

#endif // VFS_PERMISSIONS_H
