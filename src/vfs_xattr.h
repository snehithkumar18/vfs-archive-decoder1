// =============================================================================
// FenrerVFS - Virtual File System Library
// vfs_xattr.h - Extended Attributes Management Interface
//
// Extended attributes (xattrs) provide a mechanism for associating arbitrary
// name-value metadata pairs with file system paths. This module implements
// a namespace-aware xattr system modeled after POSIX extended attributes
// (see xattr(7)), supporting the 'user.', 'system.', 'security.', and
// 'trusted.' namespace prefixes.
//
// Each attribute consists of:
//   - A namespaced name (e.g., "user.mime_type", "security.selinux")
//   - A binary value stored as a vector of bytes
//   - Internally tracked namespace prefix for efficient filtering
//
// Thread safety: All public methods acquire a mutex before accessing the
// internal attribute store. It is safe to call any combination of methods
// from multiple threads concurrently, though performance may degrade under
// high contention on a single path.
//
// Limits:
//   - Maximum attribute name length: 255 bytes (matches ext4 XATTR_NAME_MAX)
//   - Maximum single value size: 65536 bytes (64 KiB)
//   - Maximum number of attributes per path: 1024
//   - Namespace prefix is validated on every set operation
//
// Copyright (c) 2026 FenrerVFS Project
// =============================================================================

#ifndef VFS_XATTR_H
#define VFS_XATTR_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <cstddef>
#include <mutex>

// ---------------------------------------------------------------------------
// Constants governing xattr storage limits.
// These mirror common Linux kernel limits for compatibility, even though
// FenrerVFS is an in-memory virtual file system.
// ---------------------------------------------------------------------------

/// Maximum length of an attribute name including the namespace prefix.
/// Example: "user.my_custom_attribute" must be <= 255 characters total.
static constexpr size_t XATTR_NAME_MAX_LEN = 255;

/// Maximum size of a single attribute value in bytes (64 KiB).
/// Attempting to store a value larger than this will be rejected.
static constexpr size_t XATTR_VALUE_MAX_SIZE = 65536;

/// Maximum number of extended attributes that can be attached to one path.
/// This prevents unbounded memory growth from pathological usage patterns.
static constexpr size_t XATTR_MAX_PER_PATH = 1024;

/// Number of recognized namespace prefixes.
static constexpr size_t XATTR_NAMESPACE_COUNT = 4;

/// The set of valid namespace prefixes. Attribute names MUST begin with
/// one of these prefixes; otherwise set_xattr will reject the operation.
/// The prefixes include the trailing dot as part of the prefix itself.
static const char* const XATTR_VALID_NAMESPACES[XATTR_NAMESPACE_COUNT] = {
    "user.",       // User-defined metadata (no special privileges required)
    "system.",     // System-level metadata (ACLs, POSIX caps, etc.)
    "security.",   // Security module metadata (SELinux labels, etc.)
    "trusted."     // Trusted metadata (requires CAP_SYS_ADMIN equivalent)
};

// ---------------------------------------------------------------------------
// XAttrEntry - a single extended attribute record.
// ---------------------------------------------------------------------------

/// Represents one extended attribute attached to a virtual file system path.
/// The namespace_prefix field is extracted and cached at insertion time so
/// that namespace-filtered queries (e.g., "list all user.* attrs") can be
/// answered without re-parsing attribute names on every call.
struct XAttrEntry {
    /// Full attribute name including namespace prefix, e.g. "user.mime_type".
    std::string name;

    /// Raw binary value. May contain arbitrary bytes including embedded NULs.
    std::vector<uint8_t> value;

    /// Cached namespace prefix extracted from `name` at insertion time.
    /// Always one of: "user.", "system.", "security.", "trusted."
    std::string namespace_prefix;

    /// Timestamp (seconds since epoch) when this attribute was last modified.
    /// Updated on set_xattr (both create and overwrite).
    uint64_t last_modified;

    /// Default constructor for container compatibility.
    XAttrEntry() : last_modified(0) {}

    /// Constructs a fully-initialized xattr entry.
    XAttrEntry(std::string attr_name,
               std::vector<uint8_t> attr_value,
               std::string ns_prefix,
               uint64_t mod_time)
        : name(std::move(attr_name)),
          value(std::move(attr_value)),
          namespace_prefix(std::move(ns_prefix)),
          last_modified(mod_time) {}
};

// ---------------------------------------------------------------------------
// Result codes specific to xattr operations.
// These extend VFSErrorCode but are kept separate to avoid modifying the
// core errors.h header, maintaining backward compatibility.
// ---------------------------------------------------------------------------
enum class XAttrResult {
    OK = 0,                    // Operation completed successfully
    ERR_NOT_FOUND,             // Path or attribute name does not exist
    ERR_NAME_TOO_LONG,         // Attribute name exceeds XATTR_NAME_MAX_LEN
    ERR_VALUE_TOO_LARGE,       // Value exceeds XATTR_VALUE_MAX_SIZE
    ERR_INVALID_NAMESPACE,     // Name does not start with a valid ns prefix
    ERR_LIMIT_EXCEEDED,        // Path already has XATTR_MAX_PER_PATH attrs
    ERR_ALREADY_EXISTS,        // Attribute exists and overwrite was not set
    ERR_EMPTY_NAME,            // Attribute name (after prefix) is empty
    ERR_INVALID_PATH,          // Path string is empty or malformed
    ERR_INTERNAL               // Unexpected internal error
};

/// Convert an XAttrResult code to a human-readable string for logging.
const char* xattr_result_to_string(XAttrResult result);

// ---------------------------------------------------------------------------
// VFSExtendedAttributes - the main xattr management class.
// ---------------------------------------------------------------------------

/// Manages extended attributes for all paths in a FenrerVFS instance.
///
/// Usage example:
///   VFSExtendedAttributes xattrs;
///   std::vector<uint8_t> val = {'p','n','g'};
///   xattrs.set_xattr("/images/logo.png", "user.format", val, false);
///   auto retrieved = xattrs.get_xattr("/images/logo.png", "user.format");
///
/// The class stores attributes in a nested map structure:
///   outer map: canonical path -> vector of XAttrEntry
///   Each XAttrEntry within the vector has a unique name for that path.
///
/// All public methods are mutex-protected for thread safety.
class VFSExtendedAttributes {
public:
    VFSExtendedAttributes();
    ~VFSExtendedAttributes();

    // -- Core CRUD operations -----------------------------------------------

    /// Set (create or update) an extended attribute on a path.
    /// @param path       Canonical VFS path (e.g., "/data/config.json")
    /// @param attr_name  Full attribute name with namespace (e.g., "user.key")
    /// @param value      Binary value to store
    /// @param overwrite  If false and attr already exists, returns ERR_ALREADY_EXISTS
    /// @return XAttrResult indicating success or specific failure reason
    XAttrResult set_xattr(const std::string& path,
                          const std::string& attr_name,
                          const std::vector<uint8_t>& value,
                          bool overwrite = true);

    /// Retrieve the value of an extended attribute.
    /// @param path       Canonical VFS path
    /// @param attr_name  Full attribute name with namespace
    /// @param out_value  [out] Populated with the attribute value on success
    /// @return XAttrResult::OK on success, ERR_NOT_FOUND if path/attr missing
    XAttrResult get_xattr(const std::string& path,
                          const std::string& attr_name,
                          std::vector<uint8_t>& out_value) const;

    /// Remove an extended attribute from a path.
    /// @param path       Canonical VFS path
    /// @param attr_name  Full attribute name with namespace
    /// @return XAttrResult::OK on success, ERR_NOT_FOUND if not present
    XAttrResult remove_xattr(const std::string& path,
                             const std::string& attr_name);

    // -- Query operations ---------------------------------------------------

    /// List all attribute names attached to a path.
    /// @param path             Canonical VFS path
    /// @param out_names        [out] Populated with attribute names
    /// @param namespace_filter Optional namespace prefix to filter by (e.g., "user.")
    ///                         Pass empty string to list all namespaces.
    /// @return XAttrResult::OK (out_names may be empty if no attrs match)
    XAttrResult list_xattrs(const std::string& path,
                            std::vector<std::string>& out_names,
                            const std::string& namespace_filter = "") const;

    /// Check whether a specific attribute exists on a path.
    /// @param path       Canonical VFS path
    /// @param attr_name  Full attribute name with namespace
    /// @return true if the attribute exists, false otherwise
    bool has_xattr(const std::string& path,
                   const std::string& attr_name) const;

    /// Get the size (in bytes) of an attribute's value without copying it.
    /// Useful for pre-allocating buffers before a get_xattr call.
    /// @param path       Canonical VFS path
    /// @param attr_name  Full attribute name with namespace
    /// @param out_size   [out] Set to the value size on success
    /// @return XAttrResult::OK on success, ERR_NOT_FOUND if not present
    XAttrResult get_xattr_size(const std::string& path,
                               const std::string& attr_name,
                               size_t& out_size) const;

    // -- Bulk operations ----------------------------------------------------

    /// Copy all extended attributes from one path to another.
    /// Pre-existing attributes on the destination that share a name with a
    /// source attribute will be overwritten. Destination attributes with
    /// names not present in the source are left untouched.
    /// @param from_path  Source path
    /// @param to_path    Destination path
    /// @return XAttrResult::OK on success, ERR_NOT_FOUND if source has no attrs
    XAttrResult copy_xattrs(const std::string& from_path,
                            const std::string& to_path);

    /// Remove all extended attributes from a path.
    /// @param path  Canonical VFS path
    /// @return Number of attributes that were removed (0 if path had none)
    size_t clear_xattrs(const std::string& path);

    // -- Statistics ---------------------------------------------------------

    /// Returns the total number of attributes stored across all paths.
    size_t total_attribute_count() const;

    /// Returns the number of distinct paths that have at least one attribute.
    size_t path_count() const;

    /// Returns the total memory (approximate) consumed by all stored values.
    size_t total_value_bytes() const;

private:
    // -- Internal helpers (must be called with mutex held) -------------------

    /// Validate that attr_name starts with a recognized namespace prefix.
    /// On success, populates out_prefix with the matched prefix.
    bool validate_namespace(const std::string& attr_name,
                            std::string& out_prefix) const;

    /// Validate that a path string is non-empty and reasonably well-formed.
    bool validate_path(const std::string& path) const;

    /// Find an XAttrEntry by name within a path's attribute vector.
    /// Returns pointer to the entry or nullptr if not found.
    /// Caller must hold the mutex.
    const XAttrEntry* find_entry(const std::string& path,
                                 const std::string& attr_name) const;

    /// Mutable version of find_entry for internal updates.
    XAttrEntry* find_entry_mut(const std::string& path,
                               const std::string& attr_name);

    /// Get current timestamp as seconds since epoch (UTC).
    uint64_t current_timestamp() const;

    // -- Data members -------------------------------------------------------

    /// Primary storage: maps canonical path -> list of xattr entries.
    /// Using a vector per path (rather than an inner map) keeps memory
    /// layout cache-friendly for the common case of few attrs per path.
    /// Linear scan is acceptable given XATTR_MAX_PER_PATH = 1024.
    std::map<std::string, std::vector<XAttrEntry>> attr_store_;

    /// Protects all access to attr_store_. Mutable to allow const methods
    /// to acquire the lock (logical constness vs. bitwise constness).
    mutable std::mutex mutex_;
};

#endif // VFS_XATTR_H
