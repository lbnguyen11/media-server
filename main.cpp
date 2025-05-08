#include <spdlog/spdlog.h>
#include <iostream>
#include "server.hpp"

int main(int argc, char* argv[])
{
  try
  {
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("%^[%D %T][%t][%L]%$ %v");

    // Check command line arguments.
    if (argc != 3)
    {
      spdlog::debug("Usage: advanced-server <doc_root> <threads>");
      return EXIT_FAILURE;
    }
    auto const address = net::ip::make_address("0.0.0.0");
    auto const port = static_cast<unsigned short>(std::atoi("8080"));
    auto const doc_root = std::make_shared<std::string>(argv[1]);
    auto const threads = std::max<int>(1, std::atoi(argv[2]));
    spdlog::info("Starting server at http://0.0.0.0:8080 with {} worker_thread(s)", threads);

    // The io_context is required for all I/O
    net::io_context ioc{ threads };

    // Create and launch a listening port
    std::make_shared<listener>(
      ioc,
      tcp::endpoint{ address, port },
      doc_root)
      ->run();

    // Capture SIGINT and SIGTERM to perform a clean shutdown
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
      [&](beast::error_code const&, int)
      {
        // Stop the `io_context`. This will cause `run()`
        // to return immediately, eventually destroying the
        // `io_context` and all of the sockets in it.
        ioc.stop();
      });

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for (auto i = threads - 1; i > 0; --i)
      v.emplace_back(
        [&ioc]
        {
          ioc.run();
        });
    ioc.run();

    // (If we get here, it means we got a SIGINT or SIGTERM)

    // Block until all the threads exit
    for (auto& t : v)
      t.join();

    return EXIT_SUCCESS;
  }
  catch (const std::exception& e)
  {
    spdlog::debug("Server error: {}", e.what());
  }
  return 0;
}
