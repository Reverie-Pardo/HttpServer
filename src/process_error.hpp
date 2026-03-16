#pragma once

#include <system_error>
#include <netdb.h>

#define SOURCE_INFO_IMPL(file, line) "In" file ":" #line ": "
#define SOURCE_INFO() SOURCE_INFO_IMPL(__FILE__, __LINE__)
#define CHECK_CALL(func, ...) check_error(SOURCE_INFO() #func, func(__VA_ARGS__))
#define CHECK_CALL_EXCEPT(except, func, ...) check_error<except>(SOURCE_INFO() #func, func(__VA_ARGS__))


class gai_error_category : std::error_category {
public:
    ~gai_error_category() = default;

    const char* name() const noexcept override {
        return "gai_error";
    }

    std::string message(int err) const override {
        return gai_strerror(err);
    }

    static const std::error_category& gai_category() {
        static gai_error_category instance;
        return instance;
    }

private:
    gai_error_category() = default;
    gai_error_category(const gai_error_category& other) = delete;
    gai_error_category& operator=(const gai_error_category& other) = delete;

};

template <int Except = 0, typename T>
T check_error(const char* msg, T res) {
    if (res == -1) {
        if constexpr (Except != 0) {
            if (errno == Except) {
                return -1;
            }
        }
        auto ec = std::error_code(errno, std::system_category());
        std::printf("%s: %s\n", msg, ec.message().c_str());          //printlen(stderr)
        throw std::system_error(ec, msg);
    }
    return res;
}