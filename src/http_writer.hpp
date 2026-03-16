#pragma once

#include <string>

class http11_header_writer {
public:
    void reset_state() {
        _buffer.clear();
        _offset = 0;
    }

    std::string_view buffer() {
        return {_buffer.data() + _offset, _buffer.size() - _offset};
    }

    void change_offset(size_t bytes_send) {
        _offset += bytes_send;
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

    void write_body(std::string_view body) {
        _buffer.append(body);
    }

private:
    int _offset = 0;
    std::string _buffer;
};

template <class http_header_writer = http11_header_writer>
class http_writer {
public:
    void reset_state() {
        _header_writer.reset_state();
    }

    std::string_view buffer() {
        return _header_writer.buffer();
    }

    void change_offset(size_t bytes_send) {
        _header_writer.change_offset(bytes_send);
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

    void write_body(std::string_view body) {
        _header_writer.write_body(body);
    }

    // 便捷方法
    void not_found() {
        write_headerline("HTTP/1.1", "404", "Not Found");
        end_header();
    }
private:
    http_header_writer _header_writer;
};