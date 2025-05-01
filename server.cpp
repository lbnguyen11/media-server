#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
#include "server.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

void handle_request(beast::tcp_stream& stream, const std::string& media_path) {
    beast::flat_buffer buffer;
    http::request<http::string_body> req;
    http::read(stream, buffer, req);

    if (req.method() != http::verb::get) {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::content_type, "text/plain");
        res.body() = "Only GET supported";
        res.prepare_payload();
        http::write(stream, res);
        return;
    }

    std::ifstream file(media_path, std::ios::binary | std::ios::ate);
    if (!file) {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.body() = "File not found";
        res.prepare_payload();
        http::write(stream, res);
        return;
    }

    std::streamsize file_size = file.tellg();
    file.seekg(0);

    std::streamsize start = 0, end = file_size - 1;
    auto range_hdr = req.base()["Range"];
    bool partial = false;

    if (!range_hdr.empty()) {
        partial = true;
        std::string range_str = range_hdr.to_string(); // "bytes=500-"
        if (range_str.find("bytes=") == 0) {
            range_str = range_str.substr(6);
            auto dash = range_str.find('-');
            start = std::stoll(range_str.substr(0, dash));
            if (dash + 1 < range_str.size()) {
                end = std::stoll(range_str.substr(dash + 1));
            }
        }
    }

    std::streamsize length = end - start + 1;
    std::string body(length, '\0');
    file.seekg(start);
    file.read(&body[0], length);

    http::response<http::string_body> res{
        partial ? http::status::partial_content : http::status::ok, req.version()};
    res.set(http::field::content_type, "video/mp4");
    res.set(http::field::content_length, std::to_string(body.size()));
    if (partial)
        res.set(http::field::content_range, 
            "bytes " + std::to_string(start) + "-" + std::to_string(end) + "/" + std::to_string(file_size));
    res.body() = std::move(body);
    res.prepare_payload();

    http::write(stream, res);
}

void run_server(const std::string& address, int port, const std::string& media_path) {
    net::io_context ioc{1};
    tcp::acceptor acceptor{ioc, {net::ip::make_address(address), static_cast<unsigned short>(port)}};

    std::cout << "Serving " << media_path << " on http://" << address << ":" << port << std::endl;

    while (true) {
        tcp::socket socket = acceptor.accept();
        beast::tcp_stream stream(std::move(socket));
        handle_request(stream, media_path);
        stream.socket().shutdown(tcp::socket::shutdown_send);
    }
}
