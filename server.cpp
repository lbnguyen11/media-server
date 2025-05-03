#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <functional>
#include <memory>
#include <thread>
#include <vector>
#include <chrono>
#include <future>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include "server.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// This is the C++11 equivalent of a generic lambda.
// The function object is used to send an HTTP message.
template<class Stream>
struct send_lambda
{
    Stream& stream_;
    bool& close_;
    beast::error_code& ec_;

    explicit
    send_lambda(
        Stream& stream,
        bool& close,
        beast::error_code& ec)
        : stream_(stream)
        , close_(close)
        , ec_(ec)
    {
    }

    template<bool isRequest, class Body, class Fields>
    auto
    operator()(http::message<isRequest, Body, Fields>& msg) const
    {
        // Determine if we should close the connection after
        close_ = msg.need_eof();

        // We need the serializer here because the serializer requires
        // a non-const file_body, and the message oriented version of
        // http::write only works with const messages.
        //http::serializer<isRequest, Body, Fields> sr{msg};
        return http::write(stream_, msg, ec_);
    }
};

template<bool isRequest, typename Body>
struct fmt::formatter<http::message<isRequest,Body>> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.end();
    }

    template <typename FormatContext>
    constexpr auto format(const http::message<isRequest,Body>& input, FormatContext& ctx) -> decltype(ctx.out()) {
        auto c = input.base();
        auto out = ctx.out();
		const std::string s[2] {"response:", "request:"};
        fmt::format_to(out, "{}\n", s[isRequest]);
        for (auto it = c.begin(); it != c.end(); ++it) {
            out = fmt::format_to(out, "{}{:<30}{}\n", std::string(35,' '), it->name_string(), it->value());
		}
		if constexpr (isRequest)
			out = fmt::format_to(out, "{}Method: {}",      std::string(34,' '), input.method());
		else
			out = fmt::format_to(out, "{}Result: {} ({})", std::string(34,' '), input.result(), static_cast<int>(input.result()));
        return out;
    }
};

void handle_request(tcp::socket& stream, const std::string& media_path,
                    send_lambda<tcp::socket>& send, beast::error_code ec) {
    spdlog::info("::handle_request");
    beast::flat_buffer buffer;
    http::request<http::string_body> req;
    http::read(stream, buffer, req);

    spdlog::info("{}{}", std::string(2,' '), req);
    // spdlog::info("{:{}}{}", 1, req);

    if (req.method() != http::verb::get) {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::content_type, "text/plain");
        res.body() = "Only GET supported";
        res.prepare_payload();
        //http::write(stream, res);
		send(res);
        return;
    }

    std::ifstream file(media_path, std::ios::binary | std::ios::ate);
    if (!file) {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.body() = "File not found";
        res.prepare_payload();
        // http::write(stream, res);
		send(res);
        return;
    }

    std::streamsize file_size = file.tellg();
	spdlog::info("file_size:{:>20}", file_size);
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
	spdlog::info("start    :{:>20}", start);
	spdlog::info("length   :{:>20}", length);
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

	spdlog::info("{}{}", std::string(2,' '), res);
	if (start*2 > file_size)
	{
		spdlog::info("start*2 > file_size => throw");
		throw std::exception();
	}
	spdlog::info("ready to write!!");
    // auto written = http::write(stream, res, ec);
	auto written = send(res);
	spdlog::info("written  :{:>20}", written);
    // http::async_write(
    //     stream, res,
    //     [](boost::system::error_code ec, std::size_t bytes_transferred) {
    //         std::cout << "CALLBACK:\n: " << std::endl;
    //         if (ec == boost::asio::error::broken_pipe ||
    //             ec == boost::asio::error::connection_reset ||
    //             ec == boost::asio::error::connection_aborted) {
                
    //             std::cout << "Client disconnected: " << ec.message() << std::endl;
    //             return;
    //         } else if (ec) {
    //             std::cout << "Error writing response: " << ec.message() << std::endl;
    //             // Handle error
    //             return;
    //         }
    //         else {
    //             std::cout << "Sent: " << bytes_transferred << std::endl;
    //         }
            
    //         // Success case handling here
    //     });
    // std::cout << "async_write:\n";
}

// Handles an HTTP server connection
void
do_session(
    tcp::socket& socket,
    const std::string& media_path)
{
    spdlog::debug("Start new session!!!");
    bool close = false;
    beast::error_code ec;
    // tcp::socket stream(std::move(socket));
	tcp::socket& stream = socket;

    // This lambda is used to send messages
    send_lambda<tcp::socket> lambda{socket, close, ec};
    for(;;)
    {
        // Read a request and Send a reponse
        handle_request(stream, media_path, lambda, ec);
        if(ec)
            return fail(ec, "write");
        if(close)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            std::cout << "connection is closed!!!\n";
            break;
        }
    }

    // Send a TCP shutdown
    socket.shutdown(tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
}

void run_server(const std::string& address, int port, const std::string& media_path) {
    net::io_context ioc{1};
    spdlog::info("Serving {} on http://{}:{}", media_path, address,port);

    // The acceptor receives incoming connections
    tcp::acceptor acceptor{ioc, {net::ip::make_address(address), static_cast<unsigned short>(port)}};

	std::vector<std::future<void>> v;

    for(;;)
    {
        // This will receive the new connection
        tcp::socket socket{ioc};

        // Block until we get a connection
        acceptor.accept(socket);

        // Launch the session, transferring ownership of the socket
        // std::thread{std::bind(
        //     &do_session,
        //     std::move(socket),
        //     media_path)}.detach();
        auto fut = std::async(std::launch::async, std::bind(&do_session,std::move(socket),media_path));
		v.push_back(std::move(fut));
		spdlog::debug("v.size(): {}", v.size());

		using namespace std::chrono_literals;
		for (auto& f : v)
		{
			if (f.valid() && f.wait_for(1ms) == std::future_status::ready) f.get();
		}
    }
}
