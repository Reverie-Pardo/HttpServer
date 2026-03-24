#pragma once

// #include "http_connection_handler.hpp"
#include "ThreadPool.hpp"

class http_server {
public:
    http_server(size_t n) : _router(), _pool(n, _router) {
        _epollfd = -1;
        _sockfd = -1;
        _addrinfo = nullptr;
    }

    void init(const char* name, const char* service) {
        _epollfd = create_epoll();
        _sockfd = create_socket_and_build(name, service);

        CHECK_CALL(listen, _sockfd, SOMAXCONN);
        std::printf("Listening on %s:%s\n", name, service);

        init_acceptor();
    }
    
    void add_router(std::string method, std::string path, Handler handle) {
        _router.add(std::move(method), std::move(path), std::move(handle));
    }

    void run() {
        epoll_event events[64];
        while (true) {
            int ready = epoll_wait(_epollfd, events, 64, -1);
            // if (ready > 0) {
            //     std::printf("%d events\n", ready);
            // }
            for (int i = 0; i < ready; ++i) {
                if ((events[i].events & EPOLLIN) && events[i].data.ptr == nullptr) {
                    accept_new_connection();
                }
            }
            // std::printf("%d new events\n", ready);
        }
    }

    void do_close() {
        if (_epollfd != -1 && _sockfd != -1){
            epoll_ctl(_epollfd, EPOLL_CTL_DEL, _sockfd, nullptr);
        }
        if (_epollfd != -1) {
            close(_epollfd);
            _epollfd = -1;
        }
        if (_sockfd != -1) {
            close(_sockfd);
            _sockfd = -1;
        }

        if (_addrinfo != nullptr) {
            freeaddrinfo(_addrinfo);
        }
    }

    ~http_server() {
        do_close();
    }
private:
    int _epollfd;
    int _sockfd;
    addrinfo* _addrinfo;
    Router _router;
    ThreadPool _pool;

    [[nodiscard]] int create_socket_and_build(const char* name, const char* service) {
        if(_addrinfo != nullptr) {
            freeaddrinfo(_addrinfo);
        }

        int res = getaddrinfo(name, service, nullptr, &_addrinfo);
        if (res != 0) {
            auto ec = std::error_code(res, gai_error_category::gai_category());
            throw std::system_error(ec, "addressinfo");
        }

        int sockfd = CHECK_CALL(socket, _addrinfo->ai_family, _addrinfo->ai_socktype, _addrinfo->ai_protocol);
        int flags = CHECK_CALL(fcntl, sockfd, F_GETFL);
        CHECK_CALL(fcntl, sockfd, F_SETFL, flags | O_NONBLOCK);

        int opt = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        CHECK_CALL(bind, sockfd, _addrinfo->ai_addr, _addrinfo->ai_addrlen);
        return sockfd;
    }

    [[nodiscard]] int create_epoll() {
        return epoll_create1(0);
    }

    void init_acceptor() {
        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.ptr = nullptr;
        epoll_ctl(_epollfd, EPOLL_CTL_ADD, _sockfd, &ev); 
    }

    void accept_new_connection() {
        struct sockaddr addr;
        socklen_t len = static_cast<socklen_t>(sizeof(addr));
        int connid = CHECK_CALL_EXCEPT(EAGAIN, accept, _sockfd, &addr, &len);
        if (connid != -1) {
            std::printf("receive connection request from: %d\n", connid);
            _pool.submit(connid);
            return;
        }
    }
};