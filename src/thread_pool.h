#ifndef PIXELFORGE_THREAD_POOL_H
#define PIXELFORGE_THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <memory>
#include <atomic>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace PixelForge {

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename F, typename... Args>
    auto Submit(int priority, F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result_t<F, Args...>>;

    template <typename F, typename... Args>
    auto Submit(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result_t<F, Args...>> {
        return Submit(0, std::forward<F>(f), std::forward<Args>(args)...);
    }

    void Resize(size_t new_size);
    size_t GetWorkerCount() const;
    size_t GetPendingTaskCount() const;
    void Shutdown();

private:
    struct TaskElement {
        int priority;
        uint64_t sequence;
        std::function<void()> task;

        bool operator<(const TaskElement& other) const {
            if (priority != other.priority) {
                return priority < other.priority; // max-heap: highest priority first
            }
            return sequence > other.sequence; // FIFO: smaller sequence number first
        }
    };

    struct Worker {
        std::thread thread;
        std::shared_ptr<std::atomic<bool>> stop_flag;
    };

    void WorkerLoop(std::shared_ptr<std::atomic<bool>> stop_flag);

    std::vector<Worker> workers_;
    std::priority_queue<TaskElement> tasks_;
    
    mutable std::mutex pool_mutex_;
    std::condition_variable pool_cv_;
    
    std::atomic<bool> stop_all_{false};
    std::atomic<uint64_t> next_sequence_{0};
};

template <typename F, typename... Args>
auto ThreadPool::Submit(int priority, F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result_t<F, Args...>> {
    using return_type = typename std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(pool_mutex_);
        if (stop_all_) {
            throw std::runtime_error("Submit on stopped ThreadPool");
        }
        tasks_.push({priority, next_sequence_++, [task]() { (*task)(); }});
    }
    pool_cv_.notify_one();
    return res;
}

} // namespace PixelForge

#endif // PIXELFORGE_THREAD_POOL_H
