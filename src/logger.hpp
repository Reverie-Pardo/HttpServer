
#pragma once
#include <string>
#include <string_view>
#include <memory>
#include <chrono>
#include "log_sink.hpp"

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void init(std::string path, bool async = true, size_t flush_interval_ms = 3000) {
        if (async) {
            _logsink = std::make_unique<AsyncDoubleBufferSink>(
                std::move(path), flush_interval_ms);
        } else {
            _logsink = std::make_unique<FileSink>(std::move(path));
        }
    }

    void log(LogLevel lv, std::string msg, const char* file, int line) {
        if (!_logsink) return;
        LogRecord rec{lv, std::move(msg), std::chrono::system_clock::now(), file, line};
        _logsink->write(rec);
    }

    void log(LogLevel lv, std::string_view msg, const char* file, int line) {
        log(lv, std::string(msg), file, line);
    }

private:
    Logger() = default;
    Logger(const Logger& other) = delete;
    Logger& operator=(const Logger& other) = delete;
    std::unique_ptr<LogSink> _logsink;
};

// 宏：自动填充文件名和行号
#define LOG_DEBUG(msg) Logger::instance().log(LogLevel::DEBUG, msg, __FILE__, __LINE__)
#define LOG_INFO(msg)  Logger::instance().log(LogLevel::INFO,  msg, __FILE__, __LINE__)
#define LOG_WARN(msg)  Logger::instance().log(LogLevel::WARN,  msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::instance().log(LogLevel::ERROR, msg, __FILE__, __LINE__)