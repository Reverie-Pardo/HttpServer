#pragma once
#include <fstream>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <vector>
#include <string>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>

enum class LogLevel { DEBUG = 0, INFO, WARN, ERROR };

inline std::string_view level_str(LogLevel lv) {
    switch (lv) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
    }
    return "?????";
}

struct LogRecord {
    LogLevel   level;
    std::string message;
    std::chrono::system_clock::time_point time;
    std::string file;
    int         line;
};

class LogSink {
public:
    virtual void write(const LogRecord& rec) = 0;
    virtual ~LogSink() = default;

protected:
    // 格式化 LogRecord
    static std::string format(const LogRecord& rec) {
        auto t = std::chrono::system_clock::to_time_t(rec.time);
        struct tm tm_buf;
        localtime_r(&t, &tm_buf);
        char timebuf[32];
        std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm_buf);

        char buf[1024];
        std::snprintf(buf, sizeof(buf), "[%s] [%s] %s:%d %s\n",
            timebuf,
            std::string(level_str(rec.level)).c_str(),
            rec.file.c_str(),
            rec.line,
            rec.message.c_str());
        return buf;
    }
};

//  同步 Sink （单线程）
class FileSink : public LogSink {
public:
    explicit FileSink(std::string path, size_t max_bytes = 10 * 1024 * 1024)
        : _path(std::move(path)), _max_bytes(max_bytes) {
        _file.open(_path, std::ios::app);
    }

    ~FileSink() override { _file.close(); }

    void write(const LogRecord& rec) override {
        auto line = format(rec);
        _file.write(line.data(), line.size());
        _bytes_written += line.size();
        if (_bytes_written >= _max_bytes) rotate();
    }

    // 批量写入已格式化好的字符串
    void write_batch(const std::vector<std::string>& lines) {
        for (const auto& line : lines) {
            _file.write(line.data(), line.size());
            _bytes_written += line.size();
        }
        _file.flush();
        if (_bytes_written >= _max_bytes) rotate();
    }

private:
    void rotate() {
        _file.close();
        std::rename(_path.c_str(), (_path + ".1").c_str());
        _file.open(_path, std::ios::trunc);
        _bytes_written = 0;
    }

    std::string   _path;
    std::ofstream _file;
    size_t        _max_bytes;
    size_t        _bytes_written = 0;
};

//  双缓冲异步 Sink
class AsyncDoubleBufferSink : public LogSink {
public:
    // flush_interval_ms: 超时刷盘间隔（毫秒）
    // reserve_size:      每块缓冲预分配的条数（减少 realloc）
    explicit AsyncDoubleBufferSink(
        std::string path,
        size_t      flush_interval_ms = 3000,
        size_t      reserve_size      = 1024)
        : _backend(std::move(path))
        , _flush_interval(flush_interval_ms)
    {
        _buf_a.reserve(reserve_size);
        _buf_b.reserve(reserve_size);
        _current = &_buf_a;  // 业务线程写 A
        _standby = &_buf_b;  // 后台线程写 B（swap 后）

        _worker = std::thread([this] { run(); });
    }

    ~AsyncDoubleBufferSink() override {
        {
            std::lock_guard<std::mutex> lk(_mutex);
            _stop = true;
        }
        _cv.notify_all();
        if (_worker.joinable()) _worker.join();
    }

    void write(const LogRecord& rec) override {
        std::string line = format(rec);

        bool need_notify = false;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            _current->push_back(std::move(line));

            if (_current->size() >= _flush_threshold) {
                need_notify = true;
            }
        }
        if (need_notify) _cv.notify_one();

    }

private:
    // 后台 I/O 线程主循环
    void run() {
        while (true) {
            std::vector<std::string>* to_flush = nullptr;

            {
                std::unique_lock<std::mutex> lk(_mutex);
                // 等待：缓冲够多 or 超时 or 停止
                _cv.wait_for(lk, std::chrono::milliseconds(_flush_interval), [this] {
                    return _stop || _current->size() >= _flush_threshold;
                });

                if (!_current->empty()) {
                    std::swap(_current, _standby);
                    to_flush = _standby;
                }

                if (_stop && _current->empty() &&
                    (to_flush == nullptr || to_flush->empty())) {
                    return;
                }
            }

            if (to_flush && !to_flush->empty()) {
                _backend.write_batch(*to_flush);
                to_flush->clear();
            }

            if (_stop) {
                std::lock_guard<std::mutex> lk(_mutex);
                if (!_current->empty()) {
                    _backend.write_batch(*_current);
                    _current->clear();
                }
                return;
            }
        }
    }

    // 后台写盘用的同步 FileSink（仅 I/O 线程访问，无需额外加锁）
    FileSink _backend;

    std::vector<std::string>  _buf_a;
    std::vector<std::string>  _buf_b;
    std::vector<std::string>* _current;  // 业务线程写
    std::vector<std::string>* _standby;  // 后台线程写

    std::mutex              _mutex;
    std::condition_variable _cv;
    std::thread             _worker;

    size_t _flush_interval;              // 超时刷盘间隔（ms）
    size_t _flush_threshold = 512;       // 缓冲条数阈值
    bool   _stop = false;
};