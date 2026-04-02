#include "http_server.hpp"

constexpr size_t num_thread = 4;

int main() {
    //setlocale(LC_ALL, "zh_CN.UTF-8");
    try {
        Logger::instance().init("access.log", true, 3000);
        http_server server(num_thread);
        server.add_router("GET", "/", [](const http_parser<>& request, http_writer<>& response) {
            response.write_headerline("HTTP/1.1", "200", "OK");
            response.write_header("Server", "co_http");
            response.write_header("Connection", "keep-alive");
            response.write_header("content-length", "5");
            response.end_header();
            WriteTask task;
            task.total_bytes = 5;
            task.type = WriteTask::Type::BUFFER;
            task.buffer = std::string("Hello");
            response.write_body(std::move(task));
        });
        server.init("127.0.0.1", "8080");
        server.run();
    } catch (std::system_error const &e) {
        std::printf("错误:%s\n", e.what());
    }
    return 0;
}