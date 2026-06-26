// =============================================================================
// FenrerVFS - Virtual File System Library
// vfs_events.h - File System Event Notification Interface
//
// Implements an observer-pattern event system for the virtual file system.
// Components can subscribe to specific event types and receive asynchronous
// notifications when file system mutations occur. This enables features like:
//   - Audit logging of all file operations
//   - Cache invalidation on file modification
//   - Real-time UI updates in a file browser shell
//   - Plugin hooks that react to archive mount/unmount
//
// Design notes:
//   - Handlers are invoked synchronously on the thread that calls emit().
//     If a handler needs to perform expensive work, it should dispatch to
//     its own worker thread to avoid blocking the emitter.
//   - Each subscription is identified by a unique 64-bit ID that can be
//     used to unsubscribe later. IDs are never reused within a single
//     VFSEventSystem instance.
//   - Event history is stored in a bounded ring buffer. When the buffer
//     is full, the oldest event is overwritten. History can be queried
//     with optional type and path filters.
//   - All public methods are mutex-protected for thread safety.
//   - Handler exceptions are caught and logged but do not propagate to
//     the emitter, ensuring one faulty handler cannot crash the system.
//
// Copyright (c) 2026 FenrerVFS Project
// =============================================================================

#ifndef VFS_EVENTS_H
#define VFS_EVENTS_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>
#include <mutex>
#include <deque>
#include <chrono>

// ---------------------------------------------------------------------------
// EventType - enumeration of all observable VFS events.
// ---------------------------------------------------------------------------

/// Each value represents a distinct file system operation that can be
/// subscribed to independently. The enum values are powers of two to
/// allow future bitmask-based multi-type subscriptions if needed.
enum class EventType : uint32_t {
    FILE_CREATED        = 0x0001,  // A new file was created
    FILE_DELETED        = 0x0002,  // An existing file was deleted
    FILE_MODIFIED       = 0x0004,  // File content or metadata was changed
    FILE_RENAMED        = 0x0008,  // A file was moved/renamed
    DIR_CREATED         = 0x0010,  // A new directory was created
    DIR_DELETED         = 0x0020,  // A directory was removed
    ARCHIVE_MOUNTED     = 0x0040,  // A .fnfs archive was mounted into the VFS
    ARCHIVE_UNMOUNTED   = 0x0080,  // A mounted archive was unmounted
    CACHE_EVICTED       = 0x0100,  // An entry was evicted from the LRU cache
    PERMISSION_CHANGED  = 0x0200   // File/directory permissions were modified
};

/// Convert an EventType to a human-readable string for logging and display.
const char* event_type_to_string(EventType type);

/// Total number of distinct event types defined above.
static constexpr size_t EVENT_TYPE_COUNT = 10;

/// Default maximum number of events retained in the history ring buffer.
static constexpr size_t DEFAULT_HISTORY_LIMIT = 1000;

/// Minimum allowed history limit (can't set below this).
static constexpr size_t MIN_HISTORY_LIMIT = 10;

/// Maximum allowed history limit (prevents runaway memory usage).
static constexpr size_t MAX_HISTORY_LIMIT = 100000;

// ---------------------------------------------------------------------------
// VFSEvent - a single event record.
// ---------------------------------------------------------------------------

/// Represents a single file system event with all associated context.
/// Events are created by the emitter and delivered to subscribers by
/// const reference (handlers should not attempt to modify the event).
struct VFSEvent {
    /// The type of operation that occurred.
    EventType type;

    /// Primary path affected by the event (e.g., the file that was created).
    /// Always a canonical VFS path starting with '/'.
    std::string path;

    /// Secondary path used only for rename/move events.
    /// Contains the original (old) path before the rename. Empty for all
    /// other event types.
    std::string old_path;

    /// Monotonic timestamp of when the event was emitted.
    /// Stored as microseconds since epoch for high-resolution ordering.
    uint64_t timestamp_us;

    /// Optional key-value metadata providing additional event context.
    /// Examples: {"compression": "huffman"}, {"old_size": "1024"}
    std::map<std::string, std::string> metadata;

    /// Unique sequential ID assigned by the event system when the event
    /// is emitted. Useful for deduplication and ordering.
    uint64_t event_id;

    /// Default constructor (for container compatibility).
    VFSEvent()
        : type(EventType::FILE_CREATED),
          timestamp_us(0),
          event_id(0) {}

    /// Full constructor for creating events before emission.
    VFSEvent(EventType t,
             std::string p,
             std::string old_p = "",
             std::map<std::string, std::string> meta = {})
        : type(t),
          path(std::move(p)),
          old_path(std::move(old_p)),
          timestamp_us(0),
          event_id(0),
          metadata(std::move(meta)) {}
};

// ---------------------------------------------------------------------------
// EventHandler callback type.
// ---------------------------------------------------------------------------

/// Signature for event handler callbacks. Handlers receive a const reference
/// to the event and return void. Handlers MUST NOT throw exceptions that
/// escape their body—the event system will catch and log them, but it is
/// poor practice. Handlers are invoked on the emitting thread.
using EventHandler = std::function<void(const VFSEvent&)>;

// ---------------------------------------------------------------------------
// Subscription record (internal).
// ---------------------------------------------------------------------------

/// Internal bookkeeping for a single subscription. Pairs a handler function
/// with its subscription ID and the event type it is registered for.
struct EventSubscription {
    uint64_t subscription_id;   // Unique ID returned to the subscriber
    EventType event_type;       // Which event type this handler listens to
    EventHandler handler;       // The callback function
    std::string description;    // Optional human-readable label for debugging

    EventSubscription()
        : subscription_id(0),
          event_type(EventType::FILE_CREATED) {}

    EventSubscription(uint64_t id, EventType type, EventHandler h, std::string desc)
        : subscription_id(id),
          event_type(type),
          handler(std::move(h)),
          description(std::move(desc)) {}
};

// ---------------------------------------------------------------------------
// VFSEventSystem - the main event manager.
// ---------------------------------------------------------------------------

/// Central event bus for the FenrerVFS file system. Components register
/// interest in specific event types via subscribe(), and the VFS core calls
/// emit() whenever a mutation occurs.
///
/// Usage example:
///   VFSEventSystem events;
///   auto id = events.subscribe(EventType::FILE_CREATED, [](const VFSEvent& e) {
///       std::cout << "File created: " << e.path << std::endl;
///   });
///   events.emit(VFSEvent(EventType::FILE_CREATED, "/docs/readme.txt"));
///   events.unsubscribe(id);
///
class VFSEventSystem {
public:
    VFSEventSystem();
    ~VFSEventSystem();

    // -- Subscription management --------------------------------------------

    /// Register a handler for a specific event type.
    /// @param type        The event type to listen for
    /// @param handler     Callback function invoked when the event fires
    /// @param description Optional label for debugging (e.g., "cache_invalidator")
    /// @return Unique subscription ID (always > 0) for use with unsubscribe()
    uint64_t subscribe(EventType type,
                       EventHandler handler,
                       const std::string& description = "");

    /// Remove a previously registered subscription by its ID.
    /// @param subscription_id  The ID returned by subscribe()
    /// @return true if the subscription was found and removed, false if not found
    bool unsubscribe(uint64_t subscription_id);

    /// Remove all subscriptions for a specific event type.
    /// @param type  The event type to clear subscriptions for
    /// @return Number of subscriptions that were removed
    size_t unsubscribe_all(EventType type);

    /// Remove every subscription across all event types.
    /// @return Total number of subscriptions removed
    size_t unsubscribe_all();

    // -- Event emission -----------------------------------------------------

    /// Emit an event, notifying all registered handlers for that event type.
    /// The event is also appended to the history ring buffer (if history is
    /// enabled). Handlers are called synchronously in subscription order.
    /// @param event  The event to emit (timestamp and event_id are set internally)
    void emit(VFSEvent event);

    // -- History management -------------------------------------------------

    /// Retrieve recent events from the history buffer.
    /// @param max_count   Maximum number of events to return (0 = all available)
    /// @param type_filter Optional event type filter (pass std::nullopt for all)
    /// @param path_filter Optional path substring filter (empty = no filter)
    /// @return Vector of matching events, most recent first
    std::vector<VFSEvent> get_event_history(
        size_t max_count = 0,
        const EventType* type_filter = nullptr,
        const std::string& path_filter = "") const;

    /// Clear all events from the history buffer.
    void clear_history();

    /// Set the maximum number of events retained in history.
    /// If the new limit is smaller than the current buffer size, the oldest
    /// events are discarded immediately.
    /// @param limit  New history limit (clamped to [MIN_HISTORY_LIMIT, MAX_HISTORY_LIMIT])
    void set_history_limit(size_t limit);

    /// Get the current history limit.
    size_t get_history_limit() const;

    /// Get the current number of events stored in history.
    size_t get_history_size() const;

    // -- Statistics ---------------------------------------------------------

    /// Get the number of active subscribers for a specific event type.
    size_t get_subscriber_count(EventType type) const;

    /// Get the total number of active subscriptions across all event types.
    size_t get_total_subscriber_count() const;

    /// Get the total number of events emitted since this instance was created.
    uint64_t get_total_events_emitted() const;

    /// Get a breakdown of events emitted per type.
    std::map<EventType, uint64_t> get_event_counts_by_type() const;

private:
    // -- Internal helpers ---------------------------------------------------

    /// Generate a monotonic timestamp in microseconds since epoch.
    uint64_t generate_timestamp() const;

    /// Allocate the next unique subscription ID. Never returns 0.
    uint64_t next_subscription_id();

    /// Allocate the next unique event ID. Never returns 0.
    uint64_t next_event_id();

    /// Trim the history buffer to the current limit, discarding oldest events.
    void trim_history();

    /// Get all EventType enum values as a vector (for iteration).
    static std::vector<EventType> all_event_types();

    // -- Data members -------------------------------------------------------

    /// Subscriptions indexed by EventType for O(1) lookup on emit.
    /// Each event type maps to a vector of subscriptions in registration order.
    std::map<EventType, std::vector<EventSubscription>> subscriptions_;

    /// Event history stored as a deque for efficient front/back operations.
    /// Most recent events are at the back. When the deque exceeds
    /// history_limit_, events are popped from the front.
    std::deque<VFSEvent> history_;

    /// Maximum number of events to retain in the history buffer.
    size_t history_limit_;

    /// Counter for generating unique subscription IDs.
    uint64_t next_sub_id_;

    /// Counter for generating unique event IDs.
    uint64_t next_event_id_;

    /// Total number of events emitted over the lifetime of this instance.
    uint64_t total_events_emitted_;

    /// Per-type event emission counters for statistics.
    std::map<EventType, uint64_t> event_counts_;

    /// Mutex protecting all mutable state. Mutable for const method access.
    mutable std::mutex mutex_;
};

#endif // VFS_EVENTS_H
