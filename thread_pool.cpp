#include "utils/thread_pool.h"

namespace vfs {
namespace utils {

ThreadPool::ThreadPool(size_t num_threads) {
    for (size_t i = 0; i < num_threads; ++i) {
        threads_.emplace_back(&ThreadPool::workerThread, this);
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    
    condition_.notify_all();
    
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void ThreadPool::workerThread() {
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            condition_.wait(lock, [this] {
                return stop_ || !tasks_.empty();
            });
            
            if (stop_ && tasks_.empty()) {
                return;
            }
            
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        
        task();
    }
}

size_t ThreadPool::getQueueSize() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

} // namespace utils
} // namespace vfs
