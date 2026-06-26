// =============================================================================
// FenrerVFS - Virtual File System Library
// vfs_xattr.cc - Extended Attributes Management Implementation
//
// Implements the VFSExtendedAttributes class declared in vfs_xattr.h.
// All methods acquire the internal mutex before accessing the attribute store,
// ensuring thread-safe operation. Namespace validation is strict: attribute
// names MUST begin with one of the four recognized prefixes.
//
// Copyright (c) 2026 FenrerVFS Project
// =============================================================================

#include "vfs_xattr.h"
#include "logger.h"
#include "path_utils.h"

#include <algorithm>
#include <chrono>
#include <sstream>

// ---------------------------------------------------------------------------
// Free function: xattr_result_to_string
// ---------------------------------------------------------------------------

const char* xattr_result_to_string(XAttrResult result) {
    switch (result) {
        case XAttrResult::OK:                 return "OK";
        case XAttrResult::ERR_NOT_FOUND:      return "Not Found";
        case XAttrResult::ERR_NAME_TOO_LONG:  return "Name Too Long";
        case XAttrResult::ERR_VALUE_TOO_LARGE: return "Value Too Large";
        case XAttrResult::ERR_INVALID_NAMESPACE: return "Invalid Namespace";
        case XAttrResult::ERR_LIMIT_EXCEEDED: return "Attribute Limit Exceeded";
        case XAttrResult::ERR_ALREADY_EXISTS: return "Already Exists";
        case XAttrResult::ERR_EMPTY_NAME:     return "Empty Name";
        case XAttrResult::ERR_INVALID_PATH:   return "Invalid Path";
        case XAttrResult::ERR_INTERNAL:       return "Internal Error";
        default:                              return "Unknown XAttr Error";
    }
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

VFSExtendedAttributes::VFSExtendedAttributes() {
    VFSLogger::get_instance().info("VFSXAttr",
        "Extended attributes subsystem initialized"
        " (name_max=" + std::to_string(XATTR_NAME_MAX_LEN) +
        ", value_max=" + std::to_string(XATTR_VALUE_MAX_SIZE) +
        ", per_path_max=" + std::to_string(XATTR_MAX_PER_PATH) + ")");
}

VFSExtendedAttributes::~VFSExtendedAttributes() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t total_paths = attr_store_.size();
    size_t total_attrs = 0;
    for (const auto& pair : attr_store_) {
        total_attrs += pair.second.size();
    }
    VFSLogger::get_instance().info("VFSXAttr",
        "Shutting down xattr subsystem. Releasing " +
        std::to_string(total_attrs) + " attribute(s) across " +
        std::to_string(total_paths) + " path(s).");
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

bool VFSExtendedAttributes::validate_namespace(const std::string& attr_name,
                                                std::string& out_prefix) const {
    // Check each recognized namespace prefix in order.
    // Prefixes include the trailing dot (e.g., "user."), so we use
    // starts-with comparison. The first match wins; since no prefix
    // is a prefix of another, order does not matter.
    for (size_t i = 0; i < XATTR_NAMESPACE_COUNT; ++i) {
        const std::string prefix(XATTR_VALID_NAMESPACES[i]);
        if (attr_name.size() >= prefix.size() &&
            attr_name.compare(0, prefix.size(), prefix) == 0) {
            out_prefix = prefix;
            return true;
        }
    }
    return false;
}

bool VFSExtendedAttributes::validate_path(const std::string& path) const {
    // A valid path must be non-empty. We also require it to start with '/'
    // to enforce canonical absolute paths within the VFS.
    if (path.empty()) {
        return false;
    }
    if (path[0] != '/') {
        return false;
    }
    // Reject paths with double slashes or trailing slashes (except root "/")
    if (path.size() > 1 && path.back() == '/') {
        return false;
    }
    for (size_t i = 1; i < path.size(); ++i) {
        if (path[i] == '/' && path[i - 1] == '/') {
            return false;
        }
    }
    return true;
}

const XAttrEntry* VFSExtendedAttributes::find_entry(
        const std::string& path, const std::string& attr_name) const {
    auto path_it = attr_store_.find(path);
    if (path_it == attr_store_.end()) {
        return nullptr;
    }
    const auto& entries = path_it->second;
    for (const auto& entry : entries) {
        if (entry.name == attr_name) {
            return &entry;
        }
    }
    return nullptr;
}

XAttrEntry* VFSExtendedAttributes::find_entry_mut(
        const std::string& path, const std::string& attr_name) {
    auto path_it = attr_store_.find(path);
    if (path_it == attr_store_.end()) {
        return nullptr;
    }
    auto& entries = path_it->second;
    for (auto& entry : entries) {
        if (entry.name == attr_name) {
            return &entry;
        }
    }
    return nullptr;
}

uint64_t VFSExtendedAttributes::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(epoch).count());
}

// ---------------------------------------------------------------------------
// set_xattr
// ---------------------------------------------------------------------------

XAttrResult VFSExtendedAttributes::set_xattr(
        const std::string& path,
        const std::string& attr_name,
        const std::vector<uint8_t>& value,
        bool overwrite) {

    VFSLogger& log = VFSLogger::get_instance();

    // Validate path format before acquiring the lock (pure validation,
    // no shared state access).
    if (!validate_path(path)) {
        log.warn("VFSXAttr", "set_xattr rejected: invalid path '" + path + "'");
        return XAttrResult::ERR_INVALID_PATH;
    }

    // Validate attribute name length.
    if (attr_name.empty()) {
        log.warn("VFSXAttr", "set_xattr rejected: empty attribute name on path '"
                 + path + "'");
        return XAttrResult::ERR_EMPTY_NAME;
    }
    if (attr_name.size() > XATTR_NAME_MAX_LEN) {
        log.warn("VFSXAttr", "set_xattr rejected: name length " +
                 std::to_string(attr_name.size()) + " exceeds limit " +
                 std::to_string(XATTR_NAME_MAX_LEN) +
                 " for attr '" + attr_name.substr(0, 40) + "...'");
        return XAttrResult::ERR_NAME_TOO_LONG;
    }

    // Validate namespace prefix.
    std::string ns_prefix;
    if (!validate_namespace(attr_name, ns_prefix)) {
        log.warn("VFSXAttr", "set_xattr rejected: attribute '" + attr_name +
                 "' does not start with a valid namespace prefix "
                 "(user., system., security., trusted.)");
        return XAttrResult::ERR_INVALID_NAMESPACE;
    }

    // Validate that the name has content after the namespace prefix.
    // e.g., "user." alone is not a valid attribute name.
    if (attr_name.size() <= ns_prefix.size()) {
        log.warn("VFSXAttr", "set_xattr rejected: attribute name '" + attr_name +
                 "' has no key after namespace prefix '" + ns_prefix + "'");
        return XAttrResult::ERR_EMPTY_NAME;
    }

    // Validate value size.
    if (value.size() > XATTR_VALUE_MAX_SIZE) {
        log.warn("VFSXAttr", "set_xattr rejected: value size " +
                 std::to_string(value.size()) + " bytes exceeds limit " +
                 std::to_string(XATTR_VALUE_MAX_SIZE) +
                 " for attr '" + attr_name + "' on path '" + path + "'");
        return XAttrResult::ERR_VALUE_TOO_LARGE;
    }

    // --- Critical section begins ---
    std::lock_guard<std::mutex> lock(mutex_);

    auto& entries = attr_store_[path];  // Creates entry if path is new

    // Check if the attribute already exists on this path.
    XAttrEntry* existing = nullptr;
    for (auto& e : entries) {
        if (e.name == attr_name) {
            existing = &e;
            break;
        }
    }

    if (existing != nullptr) {
        // Attribute exists — update or reject based on overwrite flag.
        if (!overwrite) {
            log.debug("VFSXAttr", "set_xattr rejected: attribute '" + attr_name +
                      "' already exists on '" + path + "' (overwrite=false)");
            return XAttrResult::ERR_ALREADY_EXISTS;
        }

        // Overwrite: update value, timestamp, and potentially namespace
        // (namespace shouldn't change since the name hasn't changed, but
        // we recalculate for defensive correctness).
        size_t old_size = existing->value.size();
        existing->value = value;
        existing->namespace_prefix = ns_prefix;
        existing->last_modified = current_timestamp();

        log.debug("VFSXAttr", "Updated attribute '" + attr_name + "' on '" +
                  path + "' (old_size=" + std::to_string(old_size) +
                  ", new_size=" + std::to_string(value.size()) + ")");
        return XAttrResult::OK;
    }

    // Attribute is new — check per-path limit before inserting.
    if (entries.size() >= XATTR_MAX_PER_PATH) {
        log.error("VFSXAttr", "set_xattr rejected: path '" + path +
                  "' has reached the maximum of " +
                  std::to_string(XATTR_MAX_PER_PATH) + " attributes");
        // Remove the empty vector we may have created for a new path.
        if (entries.empty()) {
            attr_store_.erase(path);
        }
        return XAttrResult::ERR_LIMIT_EXCEEDED;
    }

    // Insert the new attribute.
    entries.emplace_back(attr_name, value, ns_prefix, current_timestamp());

    log.info("VFSXAttr", "Set new attribute '" + attr_name + "' on '" +
             path + "' (namespace=" + ns_prefix +
             ", value_size=" + std::to_string(value.size()) +
             ", total_attrs_on_path=" + std::to_string(entries.size()) + ")");

    return XAttrResult::OK;
}

// ---------------------------------------------------------------------------
// get_xattr
// ---------------------------------------------------------------------------

XAttrResult VFSExtendedAttributes::get_xattr(
        const std::string& path,
        const std::string& attr_name,
        std::vector<uint8_t>& out_value) const {

    VFSLogger& log = VFSLogger::get_instance();

    if (!validate_path(path)) {
        log.warn("VFSXAttr", "get_xattr: invalid path '" + path + "'");
        return XAttrResult::ERR_INVALID_PATH;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    const XAttrEntry* entry = find_entry(path, attr_name);
    if (entry == nullptr) {
        log.debug("VFSXAttr", "get_xattr: attribute '" + attr_name +
                  "' not found on path '" + path + "'");
        return XAttrResult::ERR_NOT_FOUND;
    }

    // Copy the value to the output buffer.
    out_value = entry->value;

    log.debug("VFSXAttr", "get_xattr: retrieved '" + attr_name + "' from '" +
              path + "' (" + std::to_string(out_value.size()) + " bytes)");

    return XAttrResult::OK;
}

// ---------------------------------------------------------------------------
// remove_xattr
// ---------------------------------------------------------------------------

XAttrResult VFSExtendedAttributes::remove_xattr(
        const std::string& path,
        const std::string& attr_name) {

    VFSLogger& log = VFSLogger::get_instance();

    if (!validate_path(path)) {
        log.warn("VFSXAttr", "remove_xattr: invalid path '" + path + "'");
        return XAttrResult::ERR_INVALID_PATH;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto path_it = attr_store_.find(path);
    if (path_it == attr_store_.end()) {
        log.debug("VFSXAttr", "remove_xattr: path '" + path +
                  "' has no attributes");
        return XAttrResult::ERR_NOT_FOUND;
    }

    auto& entries = path_it->second;

    // Find and erase the matching entry using the erase-remove idiom
    // adapted for a single match (we expect names to be unique per path).
    auto it = std::find_if(entries.begin(), entries.end(),
        [&attr_name](const XAttrEntry& e) { return e.name == attr_name; });

    if (it == entries.end()) {
        log.debug("VFSXAttr", "remove_xattr: attribute '" + attr_name +
                  "' not found on path '" + path + "'");
        return XAttrResult::ERR_NOT_FOUND;
    }

    size_t removed_size = it->value.size();
    std::string removed_ns = it->namespace_prefix;
    entries.erase(it);

    // If the path has no remaining attributes, remove the path entry
    // entirely to avoid accumulating empty vectors in the map.
    if (entries.empty()) {
        attr_store_.erase(path_it);
        log.debug("VFSXAttr", "Removed last attribute from path '" + path +
                  "', path entry cleaned up");
    }

    log.info("VFSXAttr", "Removed attribute '" + attr_name + "' from '" +
             path + "' (namespace=" + removed_ns +
             ", freed=" + std::to_string(removed_size) + " bytes)");

    return XAttrResult::OK;
}

// ---------------------------------------------------------------------------
// list_xattrs
// ---------------------------------------------------------------------------

XAttrResult VFSExtendedAttributes::list_xattrs(
        const std::string& path,
        std::vector<std::string>& out_names,
        const std::string& namespace_filter) const {

    VFSLogger& log = VFSLogger::get_instance();

    if (!validate_path(path)) {
        log.warn("VFSXAttr", "list_xattrs: invalid path '" + path + "'");
        return XAttrResult::ERR_INVALID_PATH;
    }

    // If a namespace filter is provided, validate that it is recognized.
    if (!namespace_filter.empty()) {
        bool valid_filter = false;
        for (size_t i = 0; i < XATTR_NAMESPACE_COUNT; ++i) {
            if (namespace_filter == XATTR_VALID_NAMESPACES[i]) {
                valid_filter = true;
                break;
            }
        }
        if (!valid_filter) {
            log.warn("VFSXAttr", "list_xattrs: unrecognized namespace filter '" +
                     namespace_filter + "'");
            return XAttrResult::ERR_INVALID_NAMESPACE;
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);

    out_names.clear();

    auto path_it = attr_store_.find(path);
    if (path_it == attr_store_.end()) {
        // Path has no attributes — this is not an error, just empty results.
        log.debug("VFSXAttr", "list_xattrs: path '" + path +
                  "' has no attributes (filter='" + namespace_filter + "')");
        return XAttrResult::OK;
    }

    const auto& entries = path_it->second;
    out_names.reserve(entries.size());

    for (const auto& entry : entries) {
        // Apply namespace filter if one was specified.
        if (!namespace_filter.empty() &&
            entry.namespace_prefix != namespace_filter) {
            continue;
        }
        out_names.push_back(entry.name);
    }

    log.debug("VFSXAttr", "list_xattrs: path '" + path + "' returned " +
              std::to_string(out_names.size()) + " attribute(s)" +
              (namespace_filter.empty() ? "" : " (filter=" + namespace_filter + ")"));

    return XAttrResult::OK;
}

// ---------------------------------------------------------------------------
// has_xattr
// ---------------------------------------------------------------------------

bool VFSExtendedAttributes::has_xattr(
        const std::string& path,
        const std::string& attr_name) const {

    if (!validate_path(path)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    return find_entry(path, attr_name) != nullptr;
}

// ---------------------------------------------------------------------------
// get_xattr_size
// ---------------------------------------------------------------------------

XAttrResult VFSExtendedAttributes::get_xattr_size(
        const std::string& path,
        const std::string& attr_name,
        size_t& out_size) const {

    VFSLogger& log = VFSLogger::get_instance();

    if (!validate_path(path)) {
        log.warn("VFSXAttr", "get_xattr_size: invalid path '" + path + "'");
        return XAttrResult::ERR_INVALID_PATH;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    const XAttrEntry* entry = find_entry(path, attr_name);
    if (entry == nullptr) {
        log.debug("VFSXAttr", "get_xattr_size: attribute '" + attr_name +
                  "' not found on path '" + path + "'");
        out_size = 0;
        return XAttrResult::ERR_NOT_FOUND;
    }

    out_size = entry->value.size();

    log.debug("VFSXAttr", "get_xattr_size: '" + attr_name + "' on '" +
              path + "' is " + std::to_string(out_size) + " bytes");

    return XAttrResult::OK;
}

// ---------------------------------------------------------------------------
// copy_xattrs
// ---------------------------------------------------------------------------

XAttrResult VFSExtendedAttributes::copy_xattrs(
        const std::string& from_path,
        const std::string& to_path) {

    VFSLogger& log = VFSLogger::get_instance();

    if (!validate_path(from_path)) {
        log.warn("VFSXAttr", "copy_xattrs: invalid source path '" + from_path + "'");
        return XAttrResult::ERR_INVALID_PATH;
    }
    if (!validate_path(to_path)) {
        log.warn("VFSXAttr", "copy_xattrs: invalid destination path '" + to_path + "'");
        return XAttrResult::ERR_INVALID_PATH;
    }
    if (from_path == to_path) {
        log.debug("VFSXAttr", "copy_xattrs: source and destination are the same '"
                  + from_path + "', no-op");
        return XAttrResult::OK;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto src_it = attr_store_.find(from_path);
    if (src_it == attr_store_.end() || src_it->second.empty()) {
        log.debug("VFSXAttr", "copy_xattrs: source path '" + from_path +
                  "' has no attributes to copy");
        return XAttrResult::ERR_NOT_FOUND;
    }

    const auto& src_entries = src_it->second;
    auto& dst_entries = attr_store_[to_path];

    size_t copied = 0;
    size_t overwritten = 0;
    size_t total_bytes = 0;

    for (const auto& src_entry : src_entries) {
        // Check if this attribute already exists on the destination path.
        bool found_existing = false;
        for (auto& dst_entry : dst_entries) {
            if (dst_entry.name == src_entry.name) {
                // Overwrite existing attribute on destination.
                dst_entry.value = src_entry.value;
                dst_entry.namespace_prefix = src_entry.namespace_prefix;
                dst_entry.last_modified = current_timestamp();
                found_existing = true;
                overwritten++;
                total_bytes += src_entry.value.size();
                break;
            }
        }

        if (!found_existing) {
            // Check per-path limit on destination before adding.
            if (dst_entries.size() >= XATTR_MAX_PER_PATH) {
                log.warn("VFSXAttr", "copy_xattrs: destination path '" + to_path +
                         "' reached attribute limit during copy. Copied " +
                         std::to_string(copied) + " of " +
                         std::to_string(src_entries.size()) + " attributes.");
                return XAttrResult::ERR_LIMIT_EXCEEDED;
            }

            // Create new attribute on destination with a fresh timestamp.
            dst_entries.emplace_back(
                src_entry.name,
                src_entry.value,
                src_entry.namespace_prefix,
                current_timestamp()
            );
            copied++;
            total_bytes += src_entry.value.size();
        }
    }

    log.info("VFSXAttr", "Copied attributes from '" + from_path + "' to '" +
             to_path + "' (new=" + std::to_string(copied) +
             ", overwritten=" + std::to_string(overwritten) +
             ", total_bytes=" + std::to_string(total_bytes) + ")");

    return XAttrResult::OK;
}

// ---------------------------------------------------------------------------
// clear_xattrs
// ---------------------------------------------------------------------------

size_t VFSExtendedAttributes::clear_xattrs(const std::string& path) {
    VFSLogger& log = VFSLogger::get_instance();

    if (!validate_path(path)) {
        log.warn("VFSXAttr", "clear_xattrs: invalid path '" + path + "'");
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attr_store_.find(path);
    if (it == attr_store_.end()) {
        log.debug("VFSXAttr", "clear_xattrs: path '" + path +
                  "' has no attributes to clear");
        return 0;
    }

    size_t count = it->second.size();
    attr_store_.erase(it);

    log.info("VFSXAttr", "Cleared " + std::to_string(count) +
             " attribute(s) from path '" + path + "'");

    return count;
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

size_t VFSExtendedAttributes::total_attribute_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t total = 0;
    for (const auto& pair : attr_store_) {
        total += pair.second.size();
    }
    return total;
}

size_t VFSExtendedAttributes::path_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return attr_store_.size();
}

size_t VFSExtendedAttributes::total_value_bytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t total_bytes = 0;
    for (const auto& pair : attr_store_) {
        for (const auto& entry : pair.second) {
            // Account for the value data itself plus the overhead of the name
            // string and namespace prefix string (approximate).
            total_bytes += entry.value.size();
            total_bytes += entry.name.size();
            total_bytes += entry.namespace_prefix.size();
        }
    }
    return total_bytes;
}
