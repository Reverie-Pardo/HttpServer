#pragma once

#include <functional>
#include <fcntl.h>

#include "http_parser.hpp"
#include "http_router.hpp"
#include "process_error.hpp"
#include <sys/epoll.h>
#include <unistd.h>
#include <queue>
#include <iostream>

class http_connection_handler {
public:
    http_connection_handler(Router& router) : _router(router) { }

    void do_init(int fd, int epollfd) {
        _buffer.resize(2048);

        int flags = CHECK_CALL(fcntl, fd, F_GETFL);
        flags |= O_NONBLOCK;
        CHECK_CALL(fcntl, fd, F_SETFL, flags);
        _conn = fd;
        _epollfd = epollfd;


        // 注册 EPOLL_IN EPOLL_OUT
        epoll_event event;
        event.events = EPOLLIN | EPOLLOUT | EPOLLET;
        event.data.ptr = this;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    }

    void read_async() {
        while (true) {
            ssize_t ret = read(_conn, _buffer.data(), _buffer.size());
            if (ret != -1) {
                if (ret == 0) {
                    std::printf("Other close connection: %d\n", _conn);
                    do_close();
                    return;
                }
                //std::printf("Read %ld bytes from %d\n", ret, _conn);
                _http_parser.push_chunk(std::string_view(_buffer.data(), ret));
                if (_http_parser.body_finished()) {
                    process();
                    _http_parser.reset_header_and_body();
                    if(write_async() == -1) {
                        return;
                    }
                }
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            } else if (errno == ECONNRESET) {
                std::printf("Connection reset by peer: %d\n", _conn);
                do_close();
                return;
            } else {
                auto ec = std::error_code(errno, std::system_category());
                std::printf("%s: %s\n", "read", ec.message().c_str());
                throw std::system_error(ec, "read");
            }
        }
    }

    int write_async() {
        while (true) {
            if (_http_writers.size() == 0) {
                return 0;
            }
            
            std::string_view to_write = _http_writers.front().buffer();
            int ret = write(_conn, to_write.data(), to_write.size());
            if (ret != -1) {
                if (ret == to_write.size()) {
                    _http_writers.pop();
                } else {
                    _http_writers.front().change_offset(ret);
                    continue;
                }
            } else if (errno == EPIPE || errno == ECONNRESET) {
                do_close();
                return -1;
            } else if (errno == EWOULDBLOCK || errno == EAGAIN){
                return 0;
            } else {
                auto ec = std::error_code(errno, std::system_category());
                std::printf("%s: %s\n", "write", ec.message().c_str());
                throw std::system_error(ec, "write");
            }
        }
    }

    void do_close() {
        epoll_ctl(_epollfd, EPOLL_CTL_DEL, _conn, nullptr);
        close(_conn);
        delete this;
    }

    void process() {
        _http_writers.push(http_writer<>());
        _router.dispatch(_http_parser, _http_writers.back());
        // std::cout << _http_parser.headerline() << '\n';
        //std::cout << _http_parser.headerline_first() << ' ' 
        //        << _http_parser.headerline_second() << ' '
        //        << _http_parser.headerline_third() << '\n';
    }
private:
    int _conn;
    std::string _buffer;
    http_parser <http11_header_parser> _http_parser;
    std::queue <http_writer<>> _http_writers;
    int _epollfd;
    const Router& _router;
};