// =============================================================================
// FenrerVFS - Virtual File System Library
// vfs_events.cc - File System Event Notification Implementation
//
// Implements the VFSEventSystem class declared in vfs_events.h. The event
// system follows a synchronous observer pattern: handlers are invoked on
// the thread that calls emit(). A bounded history ring buffer retains
// recent events for retrospective queries.
//
// Thread safety: Every public method acquires the mutex before accessing
// shared state. Handler invocations occur while the mutex is held to ensure
// consistent subscription state during dispatch. This means:
//   - Handlers MUST NOT call back into VFSEventSystem (deadlock risk).
//   - Handlers should execute quickly; defer heavy work to another thread.
//
// Exception safety: Handler exceptions are caught, logged, and swallowed.
// A faulty handler will not prevent other handlers from receiving the event,
// and will not crash the emitter.
//
// Copyright (c) 2026 FenrerVFS Project
// =============================================================================

#include "vfs_events.h"
#include "logger.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Free function: event_type_to_string
// ---------------------------------------------------------------------------

const char* event_type_to_string(EventType type) {
    switch (type) {
        case EventType::FILE_CREATED:       return "FILE_CREATED";
        case EventType::FILE_DELETED:       return "FILE_DELETED";
        case EventType::FILE_MODIFIED:      return "FILE_MODIFIED";
        case EventType::FILE_RENAMED:       return "FILE_RENAMED";
        case EventType::DIR_CREATED:        return "DIR_CREATED";
        case EventType::DIR_DELETED:        return "DIR_DELETED";
        case EventType::ARCHIVE_MOUNTED:    return "ARCHIVE_MOUNTED";
        case EventType::ARCHIVE_UNMOUNTED:  return "ARCHIVE_UNMOUNTED";
        case EventType::CACHE_EVICTED:      return "CACHE_EVICTED";
        case EventType::PERMISSION_CHANGED: return "PERMISSION_CHANGED";
        default:                            return "UNKNOWN_EVENT";
    }
}

// ---------------------------------------------------------------------------
// Static helper: enumerate all defined EventType values
// ---------------------------------------------------------------------------

std::vector<EventType> VFSEventSystem::all_event_types() {
    return {
        EventType::FILE_CREATED,
        EventType::FILE_DELETED,
        EventType::FILE_MODIFIED,
        EventType::FILE_RENAMED,
        EventType::DIR_CREATED,
        EventType::DIR_DELETED,
        EventType::ARCHIVE_MOUNTED,
        EventType::ARCHIVE_UNMOUNTED,
        EventType::CACHE_EVICTED,
        EventType::PERMISSION_CHANGED
    };
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

VFSEventSystem::VFSEventSystem()
    : history_limit_(DEFAULT_HISTORY_LIMIT),
      next_sub_id_(0),
      next_event_id_(0),
      total_events_emitted_(0) {

    // Initialize per-type event counters to zero for all known types.
    // This ensures get_event_counts_by_type always returns a complete map
    // even if no events of a particular type have been emitted yet.
    for (EventType t : all_event_types()) {
        event_counts_[t] = 0;
        subscriptions_[t] = {};  // empty subscription list
    }

    VFSLogger::get_instance().info("VFSEvents",
        "Event system initialized (history_limit=" +
        std::to_string(history_limit_) + ")");
}

VFSEventSystem::~VFSEventSystem() {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t total_subs = 0;
    for (const auto& pair : subscriptions_) {
        total_subs += pair.second.size();
    }

    VFSLogger::get_instance().info("VFSEvents",
        "Shutting down event system. Total events emitted: " +
        std::to_string(total_events_emitted_) +
        ", active subscriptions: " + std::to_string(total_subs) +
        ", history entries: " + std::to_string(history_.size()));

    // Clear all subscriptions and history to release handler captures
    // (which may hold shared_ptrs or other resources).
    subscriptions_.clear();
    history_.clear();
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

uint64_t VFSEventSystem::generate_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto since_epoch = now.time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(since_epoch).count());
}

uint64_t VFSEventSystem::next_subscription_id() {
    // Pre-increment so IDs start at 1 (0 is reserved as "invalid/none").
    return ++next_sub_id_;
}

uint64_t VFSEventSystem::next_event_id() {
    return ++next_event_id_;
}

void VFSEventSystem::trim_history() {
    // Pop from the front (oldest events) until we're within the limit.
    while (history_.size() > history_limit_) {
        history_.pop_front();
    }
}

// ---------------------------------------------------------------------------
// subscribe
// ---------------------------------------------------------------------------

uint64_t VFSEventSystem::subscribe(EventType type,
                                    EventHandler handler,
                                    const std::string& description) {
    VFSLogger& log = VFSLogger::get_instance();

    if (!handler) {
        log.error("VFSEvents", "subscribe: null handler provided for event type "
                  + std::string(event_type_to_string(type)));
        return 0;  // 0 indicates failure; valid IDs are always >= 1
    }

    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t id = next_subscription_id();

    EventSubscription sub(id, type, std::move(handler), description);
    subscriptions_[type].push_back(std::move(sub));

    std::string desc_label = description.empty() ? "<anonymous>" : description;
    log.info("VFSEvents", "Subscription #" + std::to_string(id) +
             " registered for " + event_type_to_string(type) +
             " (desc='" + desc_label + "', total_for_type=" +
             std::to_string(subscriptions_[type].size()) + ")");

    return id;
}

// ---------------------------------------------------------------------------
// unsubscribe (by ID)
// ---------------------------------------------------------------------------

bool VFSEventSystem::unsubscribe(uint64_t subscription_id) {
    VFSLogger& log = VFSLogger::get_instance();

    if (subscription_id == 0) {
        log.warn("VFSEvents", "unsubscribe: invalid subscription ID 0");
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Search all event types for the subscription ID.
    // This is O(N) over total subscriptions, but N is expected to be small
    // (typically < 100 subscribers in a VFS application).
    for (auto& pair : subscriptions_) {
        auto& subs = pair.second;
        for (auto it = subs.begin(); it != subs.end(); ++it) {
            if (it->subscription_id == subscription_id) {
                std::string desc = it->description.empty() ? "<anonymous>"
                                                           : it->description;
                EventType type = it->event_type;
                subs.erase(it);

                log.info("VFSEvents", "Unsubscribed #" +
                         std::to_string(subscription_id) + " from " +
                         event_type_to_string(type) + " (desc='" + desc +
                         "', remaining_for_type=" +
                         std::to_string(subs.size()) + ")");
                return true;
            }
        }
    }

    log.warn("VFSEvents", "unsubscribe: subscription #" +
             std::to_string(subscription_id) + " not found");
    return false;
}

// ---------------------------------------------------------------------------
// unsubscribe_all (by type)
// ---------------------------------------------------------------------------

size_t VFSEventSystem::unsubscribe_all(EventType type) {
    VFSLogger& log = VFSLogger::get_instance();

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = subscriptions_.find(type);
    if (it == subscriptions_.end() || it->second.empty()) {
        log.debug("VFSEvents", "unsubscribe_all(" +
                  std::string(event_type_to_string(type)) +
                  "): no subscriptions to remove");
        return 0;
    }

    size_t count = it->second.size();
    it->second.clear();

    log.info("VFSEvents", "Removed all " + std::to_string(count) +
             " subscription(s) for " + event_type_to_string(type));

    return count;
}

// ---------------------------------------------------------------------------
// unsubscribe_all (global)
// ---------------------------------------------------------------------------

size_t VFSEventSystem::unsubscribe_all() {
    VFSLogger& log = VFSLogger::get_instance();

    std::lock_guard<std::mutex> lock(mutex_);

    size_t total = 0;
    for (auto& pair : subscriptions_) {
        total += pair.second.size();
        pair.second.clear();
    }

    log.info("VFSEvents", "Removed all " + std::to_string(total) +
             " subscription(s) across all event types");

    return total;
}

// ---------------------------------------------------------------------------
// emit
// ---------------------------------------------------------------------------

void VFSEventSystem::emit(VFSEvent event) {
    VFSLogger& log = VFSLogger::get_instance();

    std::lock_guard<std::mutex> lock(mutex_);

    // Stamp the event with system-generated metadata.
    event.timestamp_us = generate_timestamp();
    event.event_id = next_event_id();

    // Update emission counters.
    total_events_emitted_++;
    event_counts_[event.type]++;

    log.debug("VFSEvents", "Emitting event #" +
              std::to_string(event.event_id) + " " +
              event_type_to_string(event.type) + " path='" + event.path +
              "'" + (event.old_path.empty() ? "" :
                     " old_path='" + event.old_path + "'"));

    // Append to history buffer before dispatching to handlers.
    // This way, even if all handlers fail, the event is still recorded.
    history_.push_back(event);
    trim_history();

    // Dispatch to all registered handlers for this event type.
    auto sub_it = subscriptions_.find(event.type);
    if (sub_it == subscriptions_.end() || sub_it->second.empty()) {
        log.debug("VFSEvents", "Event #" + std::to_string(event.event_id) +
                  " (" + event_type_to_string(event.type) +
                  ") had no subscribers");
        return;
    }

    const auto& handlers = sub_it->second;
    size_t handler_count = handlers.size();
    size_t success_count = 0;
    size_t failure_count = 0;

    for (size_t i = 0; i < handlers.size(); ++i) {
        const auto& sub = handlers[i];
        try {
            // Invoke the handler with a const reference to the event.
            // The handler should NOT modify the event (enforced by const).
            sub.handler(event);
            success_count++;
        } catch (const std::exception& ex) {
            failure_count++;
            std::string desc = sub.description.empty() ? "<anonymous>"
                                                       : sub.description;
            log.error("VFSEvents",
                "Handler #" + std::to_string(sub.subscription_id) +
                " ('" + desc + "') threw exception for event " +
                event_type_to_string(event.type) + " #" +
                std::to_string(event.event_id) + ": " + ex.what());
        } catch (...) {
            failure_count++;
            std::string desc = sub.description.empty() ? "<anonymous>"
                                                       : sub.description;
            log.error("VFSEvents",
                "Handler #" + std::to_string(sub.subscription_id) +
                " ('" + desc + "') threw unknown exception for event " +
                event_type_to_string(event.type) + " #" +
                std::to_string(event.event_id));
        }
    }

    if (failure_count > 0) {
        log.warn("VFSEvents", "Event #" + std::to_string(event.event_id) +
                 " dispatch completed with " + std::to_string(failure_count) +
                 " failure(s) out of " + std::to_string(handler_count) +
                 " handler(s)");
    } else {
        log.debug("VFSEvents", "Event #" + std::to_string(event.event_id) +
                  " dispatched to " + std::to_string(success_count) +
                  " handler(s) successfully");
    }
}

// ---------------------------------------------------------------------------
// get_event_history
// ---------------------------------------------------------------------------

std::vector<VFSEvent> VFSEventSystem::get_event_history(
        size_t max_count,
        const EventType* type_filter,
        const std::string& path_filter) const {

    VFSLogger& log = VFSLogger::get_instance();

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<VFSEvent> result;

    // Determine effective maximum. 0 means "return everything that matches".
    size_t effective_max = (max_count == 0) ? history_.size() : max_count;
    result.reserve(std::min(effective_max, history_.size()));

    // Iterate from most recent (back) to oldest (front) so the caller
    // receives events in reverse chronological order.
    for (auto it = history_.rbegin(); it != history_.rend(); ++it) {
        if (result.size() >= effective_max) {
            break;
        }

        // Apply type filter if specified.
        if (type_filter != nullptr && it->type != *type_filter) {
            continue;
        }

        // Apply path substring filter if specified.
        // We check both primary path and old_path (for rename events).
        if (!path_filter.empty()) {
            bool path_matches = (it->path.find(path_filter) != std::string::npos);
            bool old_path_matches = (!it->old_path.empty() &&
                it->old_path.find(path_filter) != std::string::npos);
            if (!path_matches && !old_path_matches) {
                continue;
            }
        }

        result.push_back(*it);
    }

    std::string filter_desc;
    if (type_filter != nullptr) {
        filter_desc += " type=" + std::string(event_type_to_string(*type_filter));
    }
    if (!path_filter.empty()) {
        filter_desc += " path_contains='" + path_filter + "'";
    }

    log.debug("VFSEvents", "get_event_history: returned " +
              std::to_string(result.size()) + " event(s) from " +
              std::to_string(history_.size()) + " total" +
              (filter_desc.empty() ? " (no filters)" : filter_desc));

    return result;
}

// ---------------------------------------------------------------------------
// clear_history
// ---------------------------------------------------------------------------

void VFSEventSystem::clear_history() {
    VFSLogger& log = VFSLogger::get_instance();

    std::lock_guard<std::mutex> lock(mutex_);

    size_t count = history_.size();
    history_.clear();

    log.info("VFSEvents", "Event history cleared (" + std::to_string(count) +
             " events removed)");
}

// ---------------------------------------------------------------------------
// set_history_limit
// ---------------------------------------------------------------------------

void VFSEventSystem::set_history_limit(size_t limit) {
    VFSLogger& log = VFSLogger::get_instance();

    // Clamp to the valid range.
    size_t clamped = limit;
    if (clamped < MIN_HISTORY_LIMIT) {
        log.warn("VFSEvents", "History limit " + std::to_string(limit) +
                 " below minimum, clamping to " +
                 std::to_string(MIN_HISTORY_LIMIT));
        clamped = MIN_HISTORY_LIMIT;
    }
    if (clamped > MAX_HISTORY_LIMIT) {
        log.warn("VFSEvents", "History limit " + std::to_string(limit) +
                 " above maximum, clamping to " +
                 std::to_string(MAX_HISTORY_LIMIT));
        clamped = MAX_HISTORY_LIMIT;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    size_t old_limit = history_limit_;
    history_limit_ = clamped;

    // If the new limit is smaller, immediately trim excess history.
    size_t trimmed = 0;
    if (history_.size() > history_limit_) {
        trimmed = history_.size() - history_limit_;
        trim_history();
    }

    log.info("VFSEvents", "History limit changed from " +
             std::to_string(old_limit) + " to " +
             std::to_string(history_limit_) +
             (trimmed > 0 ? " (trimmed " + std::to_string(trimmed) +
                            " oldest events)" : ""));
}

// ---------------------------------------------------------------------------
// get_history_limit
// ---------------------------------------------------------------------------

size_t VFSEventSystem::get_history_limit() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return history_limit_;
}

// ---------------------------------------------------------------------------
// get_history_size
// ---------------------------------------------------------------------------

size_t VFSEventSystem::get_history_size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return history_.size();
}

// ---------------------------------------------------------------------------
// get_subscriber_count (per type)
// ---------------------------------------------------------------------------

size_t VFSEventSystem::get_subscriber_count(EventType type) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = subscriptions_.find(type);
    if (it == subscriptions_.end()) {
        return 0;
    }
    return it->second.size();
}

// ---------------------------------------------------------------------------
// get_total_subscriber_count
// ---------------------------------------------------------------------------

size_t VFSEventSystem::get_total_subscriber_count() const {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t total = 0;
    for (const auto& pair : subscriptions_) {
        total += pair.second.size();
    }
    return total;
}

// ---------------------------------------------------------------------------
// get_total_events_emitted
// ---------------------------------------------------------------------------

uint64_t VFSEventSystem::get_total_events_emitted() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_events_emitted_;
}

// ---------------------------------------------------------------------------
// get_event_counts_by_type
// ---------------------------------------------------------------------------

std::map<EventType, uint64_t> VFSEventSystem::get_event_counts_by_type() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return event_counts_;
}
