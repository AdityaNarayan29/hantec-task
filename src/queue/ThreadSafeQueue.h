#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

/// Thread-safe, blocking queue used as the central request buffer.
/// Multiple client threads push requests; worker threads pop them.
/// Uses std::mutex + std::condition_variable for synchronization.
template <typename T>
class ThreadSafeQueue {
public:
    void push(T item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(item));
        }
        cv_.notify_one();
    }

    /// Blocking pop - waits until an item is available or shutdown is signaled.
    /// Returns std::nullopt on shutdown with empty queue.
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || shutdown_; });

        if (queue_.empty()) {
            return std::nullopt; // shutdown signaled, no more items
        }

        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    /// Non-blocking pop attempt.
    std::optional<T> tryPop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return std::nullopt;

        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    /// Signal all waiting threads to wake up and exit.
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        cv_.notify_all();
    }

private:
    std::queue<T>           queue_;
    mutable std::mutex      mutex_;
    std::condition_variable cv_;
    bool                    shutdown_ = false;
};
