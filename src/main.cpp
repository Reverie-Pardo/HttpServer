#include "http_server.hpp"

int main() {
    //setlocale(LC_ALL, "zh_CN.UTF-8");
    try {
        http_server server;
        server.add_router("GET", "/", [](const http_parser<>& request, http_writer<>& response) {
            response.write_headerline("HTTP/1.1", "200", "OK");
            response.write_header("Server", "co_http");
            response.write_header("Connection", "keep-alive");
            response.write_header("content-length", "5");
            response.end_header();
            response.write_body("Hello");
        });
        server.init("127.0.0.1", "8080");
        server.run();
    } catch (std::system_error const &e) {
        std::printf("错误:%s\n", e.what());
    }
    return 0;
}