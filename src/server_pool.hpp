#pragma once
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <unistd.h>

class ServerPool {
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<int> queue_;
    bool stopping_ = false;
    std::function<void()> cancel_;
    std::vector<std::thread> workers_;
public:
    ServerPool(std::function<void(int)> handle, std::function<void()> cancel) : cancel_(std::move(cancel)) {
        try {
            for (int i = 0; i < 4; ++i) workers_.emplace_back([this, handle] {
                while (true) {
                    int fd;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        ready_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
                        if (stopping_) return;
                        fd = queue_.front(); queue_.pop_front();
                    }
                    try { handle(fd); } catch (...) {}
                    ::close(fd);
                }
            });
        } catch (...) { shutdown(); throw; }
    }
    void submit(int fd) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= 16 || stopping_) { ::close(fd); return; }
        queue_.push_back(fd); ready_.notify_one();
    }
    void shutdown() {
        cancel_();
        { std::lock_guard<std::mutex> lock(mutex_); stopping_ = true; }
        ready_.notify_all();
        for (auto& worker : workers_) if (worker.joinable()) worker.join();
        for (int fd : queue_) ::close(fd);
        queue_.clear();
    }
    ~ServerPool() { shutdown(); }
};
