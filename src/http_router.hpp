// http_router.hpp
#pragma once
#include <functional>
#include <string>
#include <vector>

#include <string>
#include <string_view>

#include "http_parser.hpp"
#include "http_writer.hpp"

using Handler = std::function<void(const http_parser<>&, http_writer<>&)>;

struct Route {
    std::string method;
    std::string path;
    Handler handler;
};

class Router {
public:
    void add(std::string method, std::string path, Handler handler) {
        _routes.push_back({std::move(method), std::move(path), std::move(handler)});
    }

    // 便捷方法
    void GET(std::string path, Handler h)  { add("GET",  std::move(path), std::move(h)); }
    void POST(std::string path, Handler h) { add("POST", std::move(path), std::move(h)); }

    void dispatch(const http_parser<>& req, http_writer<>& response) const {
        for (auto& route : _routes) {
            if (route.method == req.headerline_first() && match(route.path, req.headerline_second())) {
                route.handler(req, response);
                return;
            }
        }
        response.not_found();
    }

private:
    std::vector<Route> _routes;

    // 简单匹配
    bool match(std::string_view pattern, std::string_view path) const {
        return pattern == path;
    }
};