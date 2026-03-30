#pragma once
//#include "logger.hpp"
#include <fstream>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>

enum class LogLevel { DEBUG = 0, INFO, WARN, ERROR };

inline std::string_view level_str(LogLevel lv) {
    switch(lv) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
    }
    return "?????";
}

struct LogRecord {
    LogLevel level;
    std::string message;
    std::chrono::system_clock::time_point time;
    std::string file;
    int line;
};


// Sink 基类
class LogSink {
public:
    virtual void write(const LogRecord& rec) = 0;
    virtual ~LogSink() = default;
protected:
    // 将 LogRecord 格式化成字符串
    std::string format(const LogRecord& rec) {
    auto t = std::chrono::system_clock::to_time_t(rec.time);
    struct tm tm_buf;
    localtime_r(&t, &tm_buf);

    char timebuf[32];
    std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm_buf);

    char buf[1024];
    std::snprintf(buf, sizeof(buf), "[%s] [%s] %s:%d %s",
        timebuf,
        std::string(level_str(rec.level)).c_str(),
        rec.file.c_str(),
        rec.line,
        rec.message.c_str());
    return std::string(buf);
}
};

// 同步 Sink
class FileSink : public LogSink {
public:
    explicit FileSink(std::string path, size_t max_bytes = 10 * 1024 * 1024)
        : _path(std::move(path)), _max_bytes(max_bytes) {
        _file.open(_path, std::ios::app);
    }

    virtual ~FileSink() override{
        _file.close();
    }

    void write(const LogRecord& rec) override {
        std::lock_guard<std::mutex> lock(_mutex);
        auto line = format(rec) + "\n";
        _file.write(line.data(), line.size());
        _bytes_written += line.size();
        if (_bytes_written >= _max_bytes) rotate();

        _file.flush();
    }

private:
    void rotate() {
        _file.close();
        // 重命名旧文件：access.log → access.log.1
        std::rename(_path.c_str(), (_path + ".1").c_str());
        _file.open(_path, std::ios::trunc);
        _bytes_written = 0;
    }

    std::string _path;
    std::ofstream _file;
    std::mutex _mutex;
    size_t _max_bytes;
    size_t _bytes_written = 0;
};

// 异步 Sink
class AsyncSink : public LogSink {
public:
    explicit AsyncSink(std::string path)
        : _backend(std::move(path)) {
        _worker = std::thread([this]{ run(); });
    }

    void write(const LogRecord& rec) override {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _queue.push(rec);
        }
        _cv.notify_one();
    }

    ~AsyncSink() {
        { std::lock_guard<std::mutex> lock(_mutex); _stop = true; }
        _cv.notify_all();
        if (_worker.joinable()) _worker.join();
    }

private:
    void run() {
        while (true) {
            std::unique_lock<std::mutex> lock(_mutex);
            _cv.wait(lock, [this]{ return _stop || !_queue.empty(); });
            while (!_queue.empty()) {
                auto rec = std::move(_queue.front());
                _queue.pop();
                lock.unlock();
                _backend.write(rec);  // 实际写操作在锁外
                lock.lock();
            }
            if (_stop && _queue.empty()) return;
        }
    }

    FileSink _backend;
    std::queue<LogRecord> _queue;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::thread _worker;
    bool _stop = false;
};