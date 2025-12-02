#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>

namespace vfs {
namespace utils {

/**
 * @brief High-performance thread pool for concurrent task execution
 */
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();

    // Prevent copying
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * @brief Submit a task to the thread pool
     * @param f Function to execute
     * @param args Arguments for the function
     * @return Future for the result
     */
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;

    /**
     * @brief Get number of active threads
     */
    size_t getNumThreads() const { return threads_.size(); }

    /**
     * @brief Get number of pending tasks
     */
    size_t getQueueSize();

private:
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};

    void workerThread();
};

// Template implementation
template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        if (stop_) {
            throw std::runtime_error("Cannot submit task to stopped thread pool");
        }
        
        tasks_.emplace([task]() { (*task)(); });
    }
    
    condition_.notify_one();
    return result;
}

} // namespace utils
} // namespace vfs

#endif // THREAD_POOL_H
