#pragma once
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <stdexcept>

class ResourceManager {
public:
    ResourceManager(const std::vector<int>& cores) {
        for (int core : cores) {
            free_cores.push(core);
        }
        total_cores = cores.size();
    }

    int checkoutCore() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return !free_cores.empty(); });
        int core = free_cores.front();
        free_cores.pop();
        return core;
    }

    void returnCore(int core) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            free_cores.push(core);
        }
        cv.notify_one();
    }

    size_t getAvailableCount() {
        std::lock_guard<std::mutex> lock(mtx);
        return free_cores.size();
    }
    
    size_t getTotalCount() const {
        return total_cores;
    }

private:
    std::queue<int> free_cores;
    std::mutex mtx;
    std::condition_variable cv;
    size_t total_cores;
};
