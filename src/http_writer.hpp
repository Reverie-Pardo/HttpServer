#pragma once

#include <string>
#include <sys/sendfile.h>
#include <unistd.h>
#include <utility>

struct WriteTask {
    enum Type { BUFFER, SENDFILE } type;
    std::string buffer;   // BUFFER 类型
    int fd;               // SENDFILE 类型
    off_t offset;
    size_t total_bytes;
    size_t send_bytes;

    WriteTask() : type(Type::BUFFER), fd(-1), offset(0), total_bytes(0), send_bytes(0) {

    }

    WriteTask(WriteTask&& other) noexcept {
        *this = std::move(other);
    }

    WriteTask& operator=(WriteTask&& other) {
        reset_state();
        type = other.type;
        if (type == Type::BUFFER) {
            buffer = std::move(other.buffer);
        } else {
            fd = other.fd;
            other.fd = -1;
        }
        offset = other.offset;
        total_bytes = other.total_bytes;
        send_bytes = other.send_bytes;
        return *this;
    }

    // [[nodiscard]] bool finished() {
    //     return offset >= total_size;
    // }

    // void change_offset(off_t off) {
    //     offset += off;
    // }

    int send(int clientfd) {
        if (type == Type::BUFFER) {
            while(send_bytes < total_bytes) {
                int ret = write(clientfd, buffer.data() + offset, total_bytes - send_bytes);
                if (ret != -1) {
                    send_bytes += ret;
                } else {
                    return -1;
                }
            }
        } else {
            while(offset < total_bytes) {
                int ret = sendfile(clientfd, fd, &offset, total_bytes - send_bytes);
                if (ret <= 0) {
                    return ret;
                } else {
                    send_bytes += ret;
                }
            }
        }
        return 0;
    }

    void reset_state() {
        if (type == Type::SENDFILE) {
            if (fd != -1) {
                close(fd);
            }
        }
        offset = 0;
        total_bytes = 0;
        send_bytes = 0;
    }

    ~WriteTask() {
        reset_state();
    }
};

class http11_header_writer {
public:
    void reset_state() {
        _buffer.clear();
        _offset = 0;
    }

    // std::string_view buffer() {
    //     return {_buffer.data() + _offset, _buffer.size() - _offset};
    // }

    // void change_offset(size_t bytes_send) {
    //     _offset += bytes_send;
    // }

    // [[nodiscard]] bool header_finished() {
    //     return _offset >= _buffer.size();
    // }

    int send(int fd) {
        while(_offset < _buffer.size()) {
            int ret = write(fd, _buffer.data() + _offset, _buffer.size() - _offset);
            if (ret != -1) {
                _offset += ret;
            } else {
                return -1;
            }
        }
        return 0;
    }

    void write_headerline(std::string_view first, std::string_view second, std::string_view third) {
        _buffer.append(first);
        _buffer.append(" ");
        _buffer.append(second);
        _buffer.append(" ");
        _buffer.append(third);
        _buffer.append("\r\n");
    }

    void write_header(std::string_view key, std::string_view value) {
        _buffer.append(key);
        _buffer.append(": ");
        _buffer.append(value);
        _buffer.append("\r\n");
    }

    void end_header() {
        _buffer.append("\r\n");
    }

private:
    size_t _offset = 0;
    std::string _buffer;
};

template <class http_header_writer = http11_header_writer>
class http_writer {
public:
    void reset_state() {
        _header_writer.reset_state();
        _task.reset_state();
    }

    void write_headerline(std::string_view first, std::string_view second, std::string_view third) {
        _header_writer.write_headerline(first, second, third);
    }

    void write_header(std::string_view key, std::string_view value) {
        _header_writer.write_header(key, value);
    }

    void end_header() {
        _header_writer.end_header();
    }

    void write_body(WriteTask&& task) {
        _task = std::move(task);
    }

    // 便捷方法
    void not_found() {
        write_headerline("HTTP/1.1", "404", "Not Found");
        end_header();
    }

    int send(int fd) {
        int ret = _header_writer.send(fd);
        if (ret == -1) {
            return -1;
        }
        ret = _task.send(fd);
        if (ret == -1) {
            return -1;
        }
        return 0;
    }
private:
    http_header_writer _header_writer;
    WriteTask _task;
};