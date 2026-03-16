#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

// simple thread pool - i wrote this based on the pattern from cppreference
// probably not the most optimal thing ever but it works fine for file I/O
// bound tasks which is what we care about here

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads) : stopping(false) {
        for (size_t i = 0; i < numThreads; i++) {
            workers.emplace_back([this] {
                workerLoop();
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stopping = true;
        }
        cv.notify_all();
        for (auto& t : workers)
            t.join();
    }

    // no copy, no move - doesnt make sense for a pool
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (stopping) return; // pool is shutting down, ignore
            tasks.push(std::move(task));
            pendingTasks++;
        }
        cv.notify_one();
    }

    // blocks until all queued work is done
    void waitAll() {
        std::unique_lock<std::mutex> lock(doneMutex);
        doneCv.wait(lock, [this] {
            return pendingTasks.load() == 0;
        });
    }

private:
    void workerLoop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                cv.wait(lock, [this] {
                    return stopping || !tasks.empty();
                });

                if (stopping && tasks.empty())
                    return;

                task = std::move(tasks.front());
                tasks.pop();
            }

            task(); // run it

            // decrement pending and wake anyone waiting in waitAll()
            if (--pendingTasks == 0) {
                std::unique_lock<std::mutex> lock(doneMutex);
                doneCv.notify_all();
            }
        }
    }

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queueMutex;
    std::condition_variable cv;

    // separate mutex/cv for waitAll - avoids deadlock with the queue lock
    std::mutex doneMutex;
    std::condition_variable doneCv;

    std::atomic<int> pendingTasks{0};
    bool stopping;
};
