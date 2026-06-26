///////////////////////////////////////////////////////////////////////////////
/// @file thread_pool.cc
/// @brief Implementation of VFSThreadPool — priority-aware thread pool
///
/// Worker threads loop on a condition variable, dequeuing tasks from a
/// priority min-heap. Each task is wrapped in an exception-catching harness
/// so that failures are captured into the std::future without crashing
/// the worker. All lifecycle events are logged via VFSLogger.
///////////////////////////////////////////////////////////////////////////////

#include "thread_pool.h"
#include "logger.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <sstream>
#include <stdexcept>

///////////////////////////////////////////////////////////////////////////////
// Static helpers
///////////////////////////////////////////////////////////////////////////////

/// @brief Convert a TaskPriority enum to a human-readable string.
const char* VFSThreadPool::priority_to_string(TaskPriority p)
{
    switch (p) {
        case TaskPriority::CRITICAL: return "CRITICAL";
        case TaskPriority::HIGH:     return "HIGH";
        case TaskPriority::NORMAL:   return "NORMAL";
        case TaskPriority::LOW:      return "LOW";
        case TaskPriority::IDLE:     return "IDLE";
        default:                     return "UNKNOWN";
    }
}

/// @brief Format a duration in microseconds into a human-readable string.
///        e.g. "1.234 ms" or "456 us"
static std::string format_duration_us(int64_t microseconds)
{
    std::ostringstream oss;
    if (microseconds >= 1000000) {
        oss << (microseconds / 1000000.0) << " s";
    } else if (microseconds >= 1000) {
        oss << (microseconds / 1000.0) << " ms";
    } else {
        oss << microseconds << " us";
    }
    return oss.str();
}

///////////////////////////////////////////////////////////////////////////////
// Construction / Destruction
///////////////////////////////////////////////////////////////////////////////

VFSThreadPool::VFSThreadPool(size_t num_threads, const std::string& pool_name)
    : pool_name_(pool_name)
    , stop_flag_(false)
    , drain_then_stop_(false)
    , active_tasks_(0)
    , desired_workers_(0)
    , next_sequence_(0)
    , next_task_id_(0)
    , total_submitted_(0)
    , total_completed_(0)
    , total_failed_(0)
{
    // Clamp thread count to a sane range.
    if (num_threads < 1) num_threads = 1;
    if (num_threads > 256) num_threads = 256;

    desired_workers_.store(num_threads, std::memory_order_relaxed);

    VFSLogger::get_instance().info("ThreadPool",
        pool_name_ + ": Initializing with " + std::to_string(num_threads) + " worker(s)");

    spawn_workers(num_threads);

    VFSLogger::get_instance().info("ThreadPool",
        pool_name_ + ": All " + std::to_string(num_threads) + " worker(s) started");
}

VFSThreadPool::~VFSThreadPool()
{
    // If shutdown was not explicitly called, do it now.
    if (!stop_flag_.load(std::memory_order_acquire)) {
        VFSLogger::get_instance().warn("ThreadPool",
            pool_name_ + ": Destructor invoked without explicit shutdown — "
                         "initiating drain-then-stop");
        shutdown(true);
    }

    VFSLogger::get_instance().info("ThreadPool",
        pool_name_ + ": Destroyed. Lifetime stats: submitted=" +
        std::to_string(total_submitted_) + " completed=" +
        std::to_string(total_completed_) + " failed=" +
        std::to_string(total_failed_));
}

///////////////////////////////////////////////////////////////////////////////
// Worker lifecycle
///////////////////////////////////////////////////////////////////////////////

/// @brief Spawn @p count new worker threads and append them to workers_.
void VFSThreadPool::spawn_workers(size_t count)
{
    // workers_ is only mutated under mutex_ in resize(), or during
    // construction when no other thread can access the pool yet.
    size_t base_id = workers_.size();

    for (size_t i = 0; i < count; ++i) {
        size_t worker_id = base_id + i;
        workers_.emplace_back(&VFSThreadPool::worker_loop, this, worker_id);

        VFSLogger::get_instance().debug("ThreadPool",
            pool_name_ + ": Spawned worker #" + std::to_string(worker_id));
    }
}

/// @brief Main loop for each worker thread.
///
/// The worker waits on the condition variable until either:
///   - A task is available in the queue, OR
///   - The stop flag is set.
///
/// If the pool is being resized down and the number of live workers
/// exceeds the desired count, this worker voluntarily exits after
/// finishing its current task.
///
/// Each task is executed inside a try/catch block. Exceptions are
/// captured by the packaged_task and propagated to the caller's future.
/// The outer catch is a safety net that should never fire under normal
/// operation — it logs an error but keeps the worker alive.
void VFSThreadPool::worker_loop(size_t worker_id)
{
    VFSLogger::get_instance().debug("ThreadPool",
        pool_name_ + ": Worker #" + std::to_string(worker_id) + " entering main loop");

    while (true) {
        PrioritizedTask task;
        bool got_task = false;

        //---------------------------------------------------------------------
        // Wait phase: block until there's work or we should stop.
        //---------------------------------------------------------------------
        {
            std::unique_lock<std::mutex> lock(mutex_);

            cv_.wait(lock, [this] {
                // Wake if: (a) there's a task, (b) we should stop, or
                // (c) the pool is shrinking and we're an excess worker.
                return !task_queue_.empty()
                    || stop_flag_.load(std::memory_order_relaxed)
                    || workers_.size() > desired_workers_.load(std::memory_order_relaxed);
            });

            //-- Check if this worker should retire (resize down) --------
            // We only retire if there are no pending tasks or if we are
            // above the desired count and someone else can pick up work.
            if (workers_.size() > desired_workers_.load(std::memory_order_relaxed)
                && task_queue_.empty())
            {
                VFSLogger::get_instance().debug("ThreadPool",
                    pool_name_ + ": Worker #" + std::to_string(worker_id) +
                    " retiring (pool shrink)");
                break;  // exit the while loop → thread ends
            }

            //-- Check for shutdown conditions ---------------------------
            if (stop_flag_.load(std::memory_order_relaxed)) {
                if (!drain_then_stop_.load(std::memory_order_relaxed)
                    || task_queue_.empty())
                {
                    VFSLogger::get_instance().debug("ThreadPool",
                        pool_name_ + ": Worker #" + std::to_string(worker_id) +
                        " exiting (shutdown)");
                    break;
                }
                // drain_then_stop is true and queue is non-empty: keep going.
            }

            //-- Dequeue a task ------------------------------------------
            if (!task_queue_.empty()) {
                task = std::move(const_cast<PrioritizedTask&>(task_queue_.top()));
                task_queue_.pop();
                got_task = true;
            }
        }

        //---------------------------------------------------------------------
        // Execute phase: run the task outside the lock.
        //---------------------------------------------------------------------
        if (got_task) {
            active_tasks_.fetch_add(1, std::memory_order_relaxed);

            // Compute wait time (time spent in queue).
            auto now = std::chrono::steady_clock::now();
            auto wait_us = std::chrono::duration_cast<std::chrono::microseconds>(
                now - task.metrics.enqueue_time).count();

            std::string task_label = task.metrics.label.empty()
                ? ("task#" + std::to_string(task.metrics.task_id))
                : task.metrics.label;

            VFSLogger::get_instance().debug("ThreadPool",
                pool_name_ + ": Worker #" + std::to_string(worker_id) +
                " starting " + task_label +
                " [priority=" + priority_to_string(task.metrics.priority) +
                ", queued=" + format_duration_us(wait_us) + "]");

            bool task_failed = false;

            // The packaged_task inside `callable` handles the inner exception
            // capture for the std::future. This outer try/catch is a safety
            // net in case something goes wrong outside the packaged_task
            // (e.g., the callable wrapper itself throws during invocation).
            try {
                task.callable();
            }
            catch (const std::exception& ex) {
                task_failed = true;
                VFSLogger::get_instance().error("ThreadPool",
                    pool_name_ + ": Worker #" + std::to_string(worker_id) +
                    " caught unhandled exception in " + task_label +
                    ": " + ex.what());
            }
            catch (...) {
                task_failed = true;
                VFSLogger::get_instance().error("ThreadPool",
                    pool_name_ + ": Worker #" + std::to_string(worker_id) +
                    " caught unknown exception in " + task_label);
            }

            auto end = std::chrono::steady_clock::now();
            auto exec_us = std::chrono::duration_cast<std::chrono::microseconds>(
                end - now).count();

            active_tasks_.fetch_sub(1, std::memory_order_relaxed);

            // Update completion counters.
            {
                std::lock_guard<std::mutex> lock(mutex_);
                total_completed_++;
                if (task_failed) total_failed_++;

                // If the queue is now empty, signal the drain CV so that
                // shutdown(true) can unblock.
                if (task_queue_.empty()) {
                    drain_cv_.notify_all();
                }
            }

            VFSLogger::get_instance().debug("ThreadPool",
                pool_name_ + ": Worker #" + std::to_string(worker_id) +
                " finished " + task_label +
                " [exec=" + format_duration_us(exec_us) +
                ", status=" + (task_failed ? "FAILED" : "OK") + "]");
        }
    }

    VFSLogger::get_instance().debug("ThreadPool",
        pool_name_ + ": Worker #" + std::to_string(worker_id) +
        " thread exiting");
}

///////////////////////////////////////////////////////////////////////////////
// Shutdown
///////////////////////////////////////////////////////////////////////////////

void VFSThreadPool::shutdown(bool wait_for_pending)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (stop_flag_.load(std::memory_order_relaxed)) {
            VFSLogger::get_instance().warn("ThreadPool",
                pool_name_ + ": shutdown() called multiple times — ignoring");
            return;
        }

        size_t pending = task_queue_.size();

        VFSLogger::get_instance().info("ThreadPool",
            pool_name_ + ": Shutting down (wait_for_pending=" +
            std::string(wait_for_pending ? "true" : "false") +
            ", pending_tasks=" + std::to_string(pending) +
            ", active_tasks=" + std::to_string(active_tasks_.load()) + ")");

        drain_then_stop_.store(wait_for_pending, std::memory_order_relaxed);
        stop_flag_.store(true, std::memory_order_release);
    }

    // Wake all workers so they can see the stop flag.
    cv_.notify_all();

    // If draining, wait until the queue is empty.
    if (wait_for_pending) {
        std::unique_lock<std::mutex> lock(mutex_);
        drain_cv_.wait(lock, [this] {
            return task_queue_.empty()
                && active_tasks_.load(std::memory_order_relaxed) == 0;
        });

        VFSLogger::get_instance().info("ThreadPool",
            pool_name_ + ": Queue drained, joining workers");

        // Notify again after drain is confirmed so sleeping workers wake up
        // and see that stop_flag is set AND queue is empty.
        lock.unlock();
        cv_.notify_all();
    }

    // Join all worker threads. This is safe even if some have already exited.
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    workers_.clear();

    VFSLogger::get_instance().info("ThreadPool",
        pool_name_ + ": Shutdown complete. Final stats: submitted=" +
        std::to_string(total_submitted_) + " completed=" +
        std::to_string(total_completed_) + " failed=" +
        std::to_string(total_failed_));
}

///////////////////////////////////////////////////////////////////////////////
// Resize
///////////////////////////////////////////////////////////////////////////////

void VFSThreadPool::resize(size_t new_size)
{
    if (new_size < 1) new_size = 1;
    if (new_size > 256) new_size = 256;

    if (stop_flag_.load(std::memory_order_acquire)) {
        VFSLogger::get_instance().warn("ThreadPool",
            pool_name_ + ": Cannot resize after shutdown");
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    size_t current = workers_.size();
    desired_workers_.store(new_size, std::memory_order_relaxed);

    VFSLogger::get_instance().info("ThreadPool",
        pool_name_ + ": Resizing from " + std::to_string(current) +
        " to " + std::to_string(new_size) + " worker(s)");

    if (new_size > current) {
        //-- Growing: spawn additional workers ---------------------------------
        size_t delta = new_size - current;
        // Note: spawn_workers appends to workers_, using workers_.size()
        // as the base ID, so IDs continue from where we left off.
        // We temporarily release the lock strategy: spawn_workers pushes
        // threads that will immediately try to acquire mutex_ in worker_loop.
        // Since we hold the lock, they'll block until we release — which is
        // fine; they'll see the correct desired_workers_ value.

        // Actually, we need to be careful: std::thread ctor in spawn_workers
        // does not require the lock. The new thread will block on cv_.wait()
        // until we release this lock_guard. So no deadlock.
        spawn_workers(delta);

        VFSLogger::get_instance().info("ThreadPool",
            pool_name_ + ": Spawned " + std::to_string(delta) +
            " additional worker(s)");
    }
    else if (new_size < current) {
        //-- Shrinking: signal excess workers to retire ------------------------
        // We don't forcibly kill threads. Instead, we set desired_workers_ to
        // the new lower count. When workers wake from the CV (we notify them
        // below), they check if workers_.size() > desired_workers_ and if the
        // queue is empty. If so, they break out of their loop and the thread
        // ends. We then join and remove dead threads.
        //
        // Wake all workers so they can check the new desired count.
        cv_.notify_all();

        VFSLogger::get_instance().info("ThreadPool",
            pool_name_ + ": Signaled " + std::to_string(current - new_size) +
            " excess worker(s) to retire. They will exit after current task.");

        // Note: actual thread joining happens lazily. Workers that have exited
        // will have their std::thread objects in a joinable state. We can clean
        // them up here by joining finished threads, but since std::thread
        // doesn't expose an "is_finished" API, we use a pragmatic approach:
        // we detach the excess threads and let them self-terminate, then
        // remove them from the vector.
        //
        // However, detaching is dangerous if the pool is destroyed before
        // detached threads finish. Instead, we leave them in the vector and
        // let shutdown() join everything. The excess workers will exit shortly
        // after we release this lock (they'll see desired_workers_ < size and
        // queue empty, or they'll finish their current task first).
        //
        // To keep workers_.size() accurate for the next resize call, we don't
        // remove entries here. The workers_.size() check in worker_loop is
        // compared against desired_workers_, so it still works correctly as
        // long as we don't add more threads between now and when they exit.
    }
    // else: new_size == current → nothing to do.
}

///////////////////////////////////////////////////////////////////////////////
// Diagnostics
///////////////////////////////////////////////////////////////////////////////

size_t VFSThreadPool::get_active_count() const
{
    return active_tasks_.load(std::memory_order_relaxed);
}

size_t VFSThreadPool::get_pending_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return task_queue_.size();
}

size_t VFSThreadPool::get_worker_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return workers_.size();
}

ThreadPoolStats VFSThreadPool::get_stats() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    ThreadPoolStats stats;
    stats.worker_count    = workers_.size();
    stats.active_count    = active_tasks_.load(std::memory_order_relaxed);
    stats.pending_count   = task_queue_.size();
    stats.total_submitted = total_submitted_;
    stats.total_completed = total_completed_;
    stats.total_failed    = total_failed_;
    stats.is_shutdown     = stop_flag_.load(std::memory_order_relaxed);
    return stats;
}

const std::string& VFSThreadPool::get_name() const
{
    return pool_name_;  // Immutable after construction; no lock needed.
}
