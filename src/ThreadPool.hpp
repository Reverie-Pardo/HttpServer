#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

#include "SubReactor.hpp"

class ThreadPool {
public:
    explicit ThreadPool(size_t n, Router& router) : _router(router) {
        for (size_t i = 0; i < n; ++i) {
            _workers.emplace_back([this]{
                SubReactor subreactor(_router);
                while (true) {
                    /*
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        _cv.wait(lock, [this]{ return _stop || !_tasks.empty(); });
                        if (_stop && _tasks.empty()) return;
                        task = std::move(_tasks.front());
                        _tasks.pop();
                    }
                    task();*/
                    int conn = -1;
                    {
                        std::lock_guard<std::mutex> lock(_mutex);
                        if (_stop && _tasks.empty()) {
                            break;
                        }
                        if (!_tasks.empty()) {
                            conn = _tasks.front();
                            _tasks.pop();
                        }
                    }
                    if (conn != -1) {
                        
                        subreactor.assign(conn);
                    }
                    subreactor.subreactor_wait();
                }
            });
        }
    }

    void submit(int task) {
        std::lock_guard<std::mutex> lock(_mutex);
        _tasks.push(task);
        //_cv.notify_one();
    }

    ~ThreadPool() {
        { 
            std::lock_guard<std::mutex> lock(_mutex); 
            _stop = true;
        }
        //_cv.notify_all();
        for (auto& t : _workers) t.join();
    }

private:
    std::vector<std::thread> _workers;
    std::queue<int> _tasks;
    std::mutex _mutex;
    // std::condition_variable _cv;
    bool _stop = false;
    Router& _router;
};