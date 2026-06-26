#include "thread_pool.h"

namespace PixelForge {

ThreadPool::ThreadPool(size_t num_threads) {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    for (size_t i = 0; i < num_threads; ++i) {
        auto stop_flag = std::make_shared<std::atomic<bool>>(false);
        workers_.push_back(Worker{
            std::thread(&ThreadPool::WorkerLoop, this, stop_flag),
            stop_flag
        });
    }
}

ThreadPool::~ThreadPool() {
    Shutdown();
}

void ThreadPool::Resize(size_t new_size) {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    if (stop_all_) return;

    if (new_size > workers_.size()) {
        size_t to_add = new_size - workers_.size();
        for (size_t i = 0; i < to_add; ++i) {
            auto stop_flag = std::make_shared<std::atomic<bool>>(false);
            workers_.push_back(Worker{
                std::thread(&ThreadPool::WorkerLoop, this, stop_flag),
                stop_flag
            });
        }
    } else if (new_size < workers_.size()) {
        size_t to_remove = workers_.size() - new_size;
        std::vector<Worker> workers_to_stop;
        for (size_t i = 0; i < to_remove; ++i) {
            workers_to_stop.push_back(std::move(workers_.back()));
            workers_.pop_back();
        }

        for (auto& w : workers_to_stop) {
            *w.stop_flag = true;
        }

        lock.unlock();
        pool_cv_.notify_all();

        for (auto& w : workers_to_stop) {
            if (w.thread.joinable()) {
                w.thread.join();
            }
        }
    }
}

size_t ThreadPool::GetWorkerCount() const {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    return workers_.size();
}

size_t ThreadPool::GetPendingTaskCount() const {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    return tasks_.size();
}

void ThreadPool::Shutdown() {
    std::vector<Worker> workers_to_join;
    {
        std::unique_lock<std::mutex> lock(pool_mutex_);
        if (stop_all_) return;
        stop_all_ = true;
        
        for (auto& w : workers_) {
            *w.stop_flag = true;
        }
        workers_to_join = std::move(workers_);
    }
    pool_cv_.notify_all();

    for (auto& w : workers_to_join) {
        if (w.thread.joinable()) {
            w.thread.join();
        }
    }
}

void ThreadPool::WorkerLoop(std::shared_ptr<std::atomic<bool>> stop_flag) {
    while (true) {
        TaskElement task_element;
        {
            std::unique_lock<std::mutex> lock(pool_mutex_);
            pool_cv_.wait(lock, [this, stop_flag]() {
                return stop_all_ || *stop_flag || !tasks_.empty();
            });

            if (stop_all_ || *stop_flag) {
                // If the pool is stopping, we may process remaining tasks or just exit.
                // Standard behavior during resize-down: thread exits immediately.
                if (*stop_flag && !stop_all_) {
                    return;
                }
                if (stop_all_ && tasks_.empty()) {
                    return;
                }
            }

            if (!tasks_.empty()) {
                task_element = std::move(const_cast<TaskElement&>(tasks_.top()));
                tasks_.pop();
            } else {
                continue;
            }
        }

        if (task_element.task) {
            try {
                task_element.task();
            } catch (...) {
                // Ignore exceptions in worker thread
            }
        }
    }
}

} // namespace PixelForge
