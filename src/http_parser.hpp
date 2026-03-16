#pragma once

#include <string>
#include <string_view>
#include <map>
#include <algorithm>
#include <stdexcept>

using StringMap = std::map<std::string, std::string_view>;

class http11_header_parser {
public:
    http11_header_parser() { reset_all(); }

    void push_chunk(std::string_view chunk) {
        if (_header_finished) {
            push_chunk_body(chunk);
        } else {
            push_chunk_header(chunk);
        }
    }

    [[nodiscard]] bool header_finished() {
        return _header_finished;
    }

    [[nodiscard]] bool body_finished() {
        return _body_finished;
    }

    void reset_header_and_body() {
        _header.clear();
        _body.clear();
        _map.clear();
        _headerline = {nullptr, 0};
        _headerline_first = {nullptr, 0};
        _headerline_second = {nullptr, 0};
        _headerline_third = {nullptr, 0};

        _body_finished = false;
        _header_finished = false;
        _body_length = 0;
    }

    void reset_all() {
        reset_header_and_body();
        _buffer.clear();
    }

    std::string_view body() const {
        return _body;
    }

    std::string_view header() const {
        return _header;
    }

    std::string_view headerline() const {
        return _headerline;
    }

    std::string_view headerline_first() const {
        return _headerline_first;
    }

    std::string_view headerline_second() const {
        return _headerline_second;
    }

    std::string_view headerline_third() const {
        return _headerline_third;
    }

private:
    std::string _header;
    std::string _body;
    std::string_view _headerline;
    std::string_view _headerline_first;
    std::string_view _headerline_second;
    std::string_view _headerline_third;
    StringMap _map;

    std::string _buffer;

    bool _body_finished;
    bool _header_finished;

    size_t _body_length;

    void push_chunk_header(std::string_view chunk) {
        // 将上一轮剩下的数据推入 header
        if (_header.size() == 0) {
            _header = std::move(_buffer);
        }

        size_t last_size = _header.size();
        _header.append(chunk);
        if (last_size >= 4) {
            last_size -= 4;
        } else {
            last_size = 0;
        }
        // 通过寻找 "\r\n\r\n" 判断头部的结尾
        size_t end_pos = _header.find("\r\n\r\n", last_size);
        if (end_pos != std::string::npos) {
            _header_finished = true;
            // 解析头部并把多余的数据推到 body 内
            parse_header();
            push_chunk_body({_header.data() + end_pos + 4, _header.size() - end_pos - 4});
            _header.resize(end_pos + 4);
        }
    }

    void push_chunk_body(std::string_view chunk) {
        _body.append(chunk);
        // 多余的数据推入 buffer 供下一次使用
        if (_body.size() >= _body_length) {
            _buffer.append(_body.data() + _body_length, _body.size() - _body_length);
            _body_finished = true;
        }
    }

    void parse_header() {
        size_t pos = _header.find("\r\n");
        _headerline = std::string_view(_header.data(), pos);
        while (pos != std::string::npos) {
            // skip "\r\n"
            pos += 2;
            //find next "\r\n"
            size_t nextpos = _header.find("\r\n", pos);
            size_t line_len = std::string::npos;
            if (nextpos != std::string::npos) {
                line_len = nextpos - pos;
            }

            // parse this line
            std::string_view line = std::string_view(_header).substr(pos, line_len);
            size_t colon = line.find(": ");
            if (colon != std::string::npos) {
                std::string key = std::string(line.substr(0, colon));
                std::string_view value = line.substr(colon + 2);
                //turn to lower-case letters
                std::transform(key.begin(), key.end(), key.begin(), [](char c) {
                    if ('A' <= c && c <= 'Z') {
                        c += 'a' - 'A';
                    }
                    return c;
                });

                _map.insert_or_assign(std::move(key), value);
            }
            pos = nextpos;
        }
        _body_length = parse_content_length();
        parse_headline();
    }

    size_t parse_content_length() {
        auto it = _map.find("content-length");
        if (it == _map.end()) {
            return 0;
        }
        try {
            return std::stoi(std::string(it->second));
        } catch (const std::logic_error& e) {
            return 0; 
        }
    }

    void parse_headline() {
        size_t space1 = _headerline.find(' ');
        if (space1 == std::string::npos) {
            _headerline_first = {nullptr, 0};
        } else {
            _headerline_first = _headerline.substr(0, space1);
        }
        size_t space2 = _headerline.find(' ', space1 + 1);
        if (space2 == std::string::npos) {
            _headerline_second = {nullptr, 0};
        } else {
            _headerline_second = _headerline.substr(space1 + 1, space2 - space1 - 1);
            _headerline_third = _headerline.substr(space2 + 1);
        }
    }
};

template <class _http_header_parser = http11_header_parser>
class http_parser {
public:
    void push_chunk(std::string_view chunk){
        _header_parser.push_chunk(chunk);
    }

    [[nodiscard]] bool header_finished() {
        return _header_parser.header_finished();
    }

    [[nodiscard]] bool body_finished() {
        return _header_parser.body_finished();
    }

    void reset_header_and_body() {
        _header_parser.reset_header_and_body();
    } 

    void reset_all() {
        _header_parser.reset_all();
    }

    std::string_view body() const {
        return _header_parser.body();
    }

    std::string_view header() const {
        return _header_parser.header();
    }

    std::string_view headerline() const {
        return _header_parser.headerline();
    }

    std::string_view headerline_first() const {
        return _header_parser.headerline_first();
    }

    std::string_view headerline_second() const {
        return _header_parser.headerline_second();
    }

    std::string_view headerline_third() const {
        return _header_parser.headerline_third();
    }

private:
    _http_header_parser _header_parser;
};