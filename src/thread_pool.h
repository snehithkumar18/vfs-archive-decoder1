///////////////////////////////////////////////////////////////////////////////
/// @file thread_pool.h
/// @brief VFSThreadPool — A priority-aware thread pool for FenrerVFS
///
/// Provides asynchronous task execution with configurable worker threads,
/// a priority-based task queue, dynamic resizing, and graceful shutdown.
/// Tasks return std::future so callers can retrieve results or exceptions.
///
/// Usage:
///   VFSThreadPool pool(4);  // 4 worker threads
///   auto future = pool.submit(TaskPriority::NORMAL, []{ return 42; });
///   int result = future.get();
///   pool.shutdown();
///
/// Thread safety: All public methods are safe to call from any thread.
///////////////////////////////////////////////////////////////////////////////

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

///////////////////////////////////////////////////////////////////////////////
/// Task priority levels for the priority queue.
/// Lower numeric value = higher priority (CRITICAL runs before LOW).
///////////////////////////////////////////////////////////////////////////////
enum class TaskPriority : int {
    CRITICAL = 0,   ///< Highest priority — system-critical tasks
    HIGH     = 1,   ///< High priority — user-initiated blocking operations
    NORMAL   = 2,   ///< Default priority — standard background work
    LOW      = 3,   ///< Low priority — deferred maintenance tasks
    IDLE     = 4    ///< Lowest priority — runs only when pool is otherwise idle
};

///////////////////////////////////////////////////////////////////////////////
/// Metadata for a submitted task, used for logging and diagnostics.
///////////////////////////////////////////////////////////////////////////////
struct TaskMetrics {
    uint64_t task_id;                              ///< Unique monotonic task ID
    TaskPriority priority;                         ///< Priority at submission time
    std::chrono::steady_clock::time_point enqueue_time; ///< When the task was submitted
    std::string label;                             ///< Optional human-readable label
};

///////////////////////////////////////////////////////////////////////////////
/// Internal wrapper stored in the priority queue. Holds the callable and
/// its associated metadata so the worker can log lifecycle events.
///////////////////////////////////////////////////////////////////////////////
struct PrioritizedTask {
    TaskPriority priority;
    uint64_t sequence;               ///< Tie-breaker: FIFO within same priority
    std::function<void()> callable;  ///< Type-erased task (wraps packaged_task)
    TaskMetrics metrics;

    /// Comparison: lower priority value = higher urgency. For equal priority
    /// the earlier sequence number wins (FIFO ordering).
    bool operator>(const PrioritizedTask& other) const {
        if (static_cast<int>(priority) != static_cast<int>(other.priority))
            return static_cast<int>(priority) > static_cast<int>(other.priority);
        return sequence > other.sequence;
    }
};

///////////////////////////////////////////////////////////////////////////////
/// Statistics snapshot returned by get_stats().
///////////////////////////////////////////////////////////////////////////////
struct ThreadPoolStats {
    size_t worker_count;         ///< Number of live worker threads
    size_t active_count;         ///< Workers currently executing a task
    size_t pending_count;        ///< Tasks waiting in queue
    uint64_t total_submitted;    ///< Total tasks ever submitted
    uint64_t total_completed;    ///< Total tasks that finished (success or fail)
    uint64_t total_failed;       ///< Tasks that threw an exception
    bool is_shutdown;            ///< True if shutdown() has been called
};

///////////////////////////////////////////////////////////////////////////////
/// @class VFSThreadPool
/// @brief A priority-aware, dynamically-resizable thread pool.
///
/// Workers pull tasks from a min-heap ordered by (priority, FIFO sequence).
/// submit() returns a std::future<R> wrapping the task's return value.
/// Exceptions thrown inside tasks are captured and re-thrown via the future.
///
/// The pool integrates with VFSLogger for lifecycle tracing. All internal
/// state is protected by a single mutex + condition variable pair.
///////////////////////////////////////////////////////////////////////////////
class VFSThreadPool {
public:
    //=========================================================================
    // Construction / Destruction
    //=========================================================================

    /// @brief Construct a pool with @p num_threads workers.
    /// @param num_threads  Number of worker threads to spawn (clamped to [1, 256]).
    /// @param pool_name    Human-readable name used in log messages.
    explicit VFSThreadPool(size_t num_threads = 4,
                           const std::string& pool_name = "VFSThreadPool");

    /// @brief Destructor. Calls shutdown(true) if not already shut down.
    ~VFSThreadPool();

    // Non-copyable, non-movable (threads hold references to internal state).
    VFSThreadPool(const VFSThreadPool&) = delete;
    VFSThreadPool& operator=(const VFSThreadPool&) = delete;
    VFSThreadPool(VFSThreadPool&&) = delete;
    VFSThreadPool& operator=(VFSThreadPool&&) = delete;

    //=========================================================================
    // Task submission
    //=========================================================================

    /// @brief Submit a callable for asynchronous execution.
    /// @tparam F    Callable type (lambda, function pointer, functor, etc.)
    /// @tparam Args Argument types forwarded to the callable.
    /// @param priority  Execution priority.
    /// @param label     Optional label for log messages.
    /// @param func      The callable to execute.
    /// @param args      Arguments forwarded to @p func.
    /// @return std::future<R> where R is the return type of func(args...).
    /// @throws std::runtime_error if the pool has been shut down.
    template <typename F, typename... Args>
    auto submit(TaskPriority priority, const std::string& label,
                F&& func, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;

    /// @brief Convenience overload: submit with NORMAL priority, no label.
    template <typename F, typename... Args>
    auto submit(F&& func, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;

    //=========================================================================
    // Lifecycle control
    //=========================================================================

    /// @brief Gracefully shut down the pool.
    /// @param wait_for_pending  If true, finish all queued tasks before
    ///                          stopping. If false, discard pending tasks.
    void shutdown(bool wait_for_pending = true);

    /// @brief Dynamically resize the pool to @p new_size workers.
    ///        Can grow or shrink. Shrinking is cooperative: excess workers
    ///        finish their current task before exiting.
    /// @param new_size  Desired number of workers (clamped to [1, 256]).
    void resize(size_t new_size);

    //=========================================================================
    // Diagnostics
    //=========================================================================

    /// @brief Number of workers currently executing a task.
    size_t get_active_count() const;

    /// @brief Number of tasks waiting in the queue.
    size_t get_pending_count() const;

    /// @brief Number of live worker threads.
    size_t get_worker_count() const;

    /// @brief Aggregate statistics snapshot.
    ThreadPoolStats get_stats() const;

    /// @brief Returns the pool name used in logging.
    const std::string& get_name() const;

private:
    //=========================================================================
    // Internal helpers
    //=========================================================================

    /// @brief Main loop executed by each worker thread.
    /// @param worker_id  Logical index of this worker (for logging).
    void worker_loop(size_t worker_id);

    /// @brief Spawn @p count new worker threads, appending to workers_.
    void spawn_workers(size_t count);

    /// @brief Returns a string representation of a TaskPriority enum.
    static const char* priority_to_string(TaskPriority p);

    //=========================================================================
    // State
    //=========================================================================

    std::string pool_name_;                ///< Human-readable pool identifier

    mutable std::mutex mutex_;             ///< Guards all mutable state below
    std::condition_variable cv_;           ///< Signaled on enqueue or shutdown
    std::condition_variable drain_cv_;     ///< Signaled when queue becomes empty

    /// Min-heap of tasks ordered by (priority, sequence).
    std::priority_queue<PrioritizedTask,
                        std::vector<PrioritizedTask>,
                        std::greater<PrioritizedTask>> task_queue_;

    std::vector<std::thread> workers_;     ///< Live worker threads

    std::atomic<bool> stop_flag_;          ///< True once shutdown() is called
    std::atomic<bool> drain_then_stop_;    ///< True if shutdown should drain first
    std::atomic<size_t> active_tasks_;     ///< Workers currently in execute phase
    std::atomic<size_t> desired_workers_;  ///< Target number of workers (for resize)

    uint64_t next_sequence_;               ///< Monotonic FIFO tie-breaker
    uint64_t next_task_id_;                ///< Monotonic unique task ID

    uint64_t total_submitted_;             ///< Lifetime submission counter
    uint64_t total_completed_;             ///< Lifetime completion counter
    uint64_t total_failed_;                ///< Lifetime failure counter
};

///////////////////////////////////////////////////////////////////////////////
// Template implementations (must be in header)
///////////////////////////////////////////////////////////////////////////////

template <typename F, typename... Args>
auto VFSThreadPool::submit(TaskPriority priority, const std::string& label,
                           F&& func, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    // Wrap the callable + args into a shared packaged_task so we can
    // move it into the std::function<void()> wrapper.
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(func), std::forward<Args>(args)...)
    );

    std::future<return_type> result = task->get_future();

    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (stop_flag_.load(std::memory_order_relaxed)) {
            throw std::runtime_error(
                "VFSThreadPool::submit() called after shutdown");
        }

        PrioritizedTask pt;
        pt.priority = priority;
        pt.sequence = next_sequence_++;
        pt.callable = [task]() { (*task)(); };
        pt.metrics.task_id = next_task_id_++;
        pt.metrics.priority = priority;
        pt.metrics.enqueue_time = std::chrono::steady_clock::now();
        pt.metrics.label = label;

        total_submitted_++;
        task_queue_.push(std::move(pt));
    }

    cv_.notify_one();
    return result;
}

template <typename F, typename... Args>
auto VFSThreadPool::submit(F&& func, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    return submit(TaskPriority::NORMAL, "",
                  std::forward<F>(func), std::forward<Args>(args)...);
}

#endif // THREAD_POOL_H
