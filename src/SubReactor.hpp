#pragma once
#include <sys/epoll.h>

#include "http_connection_handler.hpp"

class SubReactor {
public:
    SubReactor(Router& router) : _router(router) {
        _epollfd = epoll_create1(0);
    }

    // 把新连接的 fd 分配给这个 Sub Reactor
    void assign(int connfd) {
        auto conn_handle = new http_connection_handler(_router);
        conn_handle->do_init(connfd, _epollfd);
        conn_handle->read_async();
    }

    void subreactor_wait() {
        int ready = epoll_wait(_epollfd, _events, 64, 0);
        for (int i = 0; i < ready; ++i) {
            if (_events[i].data.ptr == nullptr) {
                continue;
            }
            if (_events[i].events & EPOLLIN) {
                http_connection_handler* handler = static_cast<http_connection_handler*>(_events[i].data.ptr);
                handler->read_async();
            }
            if (_events[i].events & EPOLLOUT) {
                http_connection_handler* handler = static_cast<http_connection_handler*>(_events[i].data.ptr);
                handler->write_async();
            }
        }
    }


    ~SubReactor() {
        close(_epollfd);
    }

private:
    int _epollfd;
    epoll_event _events[64];
    Router& _router;
};