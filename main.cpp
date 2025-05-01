#include <iostream>
#include "server.hpp"

int main() {
    try {
        run_server("0.0.0.0", 8080, "sample.mp4");
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
    return 0;
}
