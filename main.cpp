#include <iostream>
#include <spdlog/spdlog.h>
#include "server.hpp"

int main() {
    try {
        spdlog::set_level(spdlog::level::trace);
        spdlog::set_pattern("%^[%D %T][%t][%L]%$ %v");
        run_server("0.0.0.0", 8080, "sample.mp4");
    } catch (const std::exception& e) {
        // std::cerr << "Server error: " << e.what() << std::endl;
        spdlog::debug("Server error: {}", e.what());
    }
    return 0;
}
