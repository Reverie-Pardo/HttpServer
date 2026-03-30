#pragma once
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <chrono>
#include "log_sink.hpp"


class Logger {
public:
    // 单例访问
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void init(std::string path, bool async = false) {
        if (async) {
            _logsink.reset(new AsyncSink(std::move(path)));
        } else {
            _logsink.reset(new FileSink(std::move(path)));
        }
    }

    void log(LogLevel lv, std::string msg, const char* file, int line) {
        LogRecord rec {
            lv, std::move(msg),
            std::chrono::system_clock::now(),
            file, line
        };
        std::lock_guard<std::mutex> lock(_mutex);
        _logsink->write(rec);
    }

    void log(LogLevel lv, std::string_view msg, const char* file, int line) {
        std::string message(msg.data(), msg.size());
        LogRecord rec {
            lv, std::move(message),
            std::chrono::system_clock::now(),
            file, line
        };
        std::lock_guard<std::mutex> lock(_mutex);
        _logsink->write(rec);
    }

private:
    Logger() = default;
    //std::vector<std::shared_ptr<LogSink>> _sinks;
    std::unique_ptr<LogSink> _logsink;
    std::mutex _mutex;
};

// 宏：自动填充文件名和行号
#define LOG_DEBUG(msg) Logger::instance().log(LogLevel::DEBUG, msg, __FILE__, __LINE__)
#define LOG_INFO(msg)  Logger::instance().log(LogLevel::INFO,  msg, __FILE__, __LINE__)
#define LOG_WARN(msg)  Logger::instance().log(LogLevel::WARN,  msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::instance().log(LogLevel::ERROR, msg, __FILE__, __LINE__)