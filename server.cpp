#include "server.hpp"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

// Return a reasonable mime type based on the extension of a file.
beast::string_view
mime_type(beast::string_view path)
{
  using beast::iequals;
  auto const ext = [&path]
    {
      auto const pos = path.rfind(".");
      if (pos == beast::string_view::npos)
        return beast::string_view{};
      return path.substr(pos);
    }();
  if (iequals(ext, ".htm"))
    return "text/html";
  if (iequals(ext, ".html"))
    return "text/html";
  if (iequals(ext, ".php"))
    return "text/html";
  if (iequals(ext, ".css"))
    return "text/css";
  if (iequals(ext, ".txt"))
    return "text/plain";
  if (iequals(ext, ".js"))
    return "application/javascript";
  if (iequals(ext, ".json"))
    return "application/json";
  if (iequals(ext, ".xml"))
    return "application/xml";
  if (iequals(ext, ".swf"))
    return "application/x-shockwave-flash";
  if (iequals(ext, ".flv"))
    return "video/x-flv";
  if (iequals(ext, ".png"))
    return "image/png";
  if (iequals(ext, ".jpe"))
    return "image/jpeg";
  if (iequals(ext, ".jpeg"))
    return "image/jpeg";
  if (iequals(ext, ".jpg"))
    return "image/jpeg";
  if (iequals(ext, ".gif"))
    return "image/gif";
  if (iequals(ext, ".bmp"))
    return "image/bmp";
  if (iequals(ext, ".ico"))
    return "image/vnd.microsoft.icon";
  if (iequals(ext, ".tiff"))
    return "image/tiff";
  if (iequals(ext, ".tif"))
    return "image/tiff";
  if (iequals(ext, ".svg"))
    return "image/svg+xml";
  if (iequals(ext, ".svgz"))
    return "image/svg+xml";
  if (iequals(ext, ".mp4"))
    return "video/mp4";
  return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string
path_cat(
  beast::string_view base,
  beast::string_view path)
{
  if (base.empty())
    return std::string(path);
  std::string result(base);
#ifdef BOOST_MSVC
  char constexpr path_separator = '\\';
  if (result.back() == path_separator)
    result.resize(result.size() - 1);
  result.append(path.data(), path.size());
  for (auto& c : result)
    if (c == '/')
      c = path_separator;
#else
  char constexpr path_separator = '/';
  if (result.back() == path_separator)
    result.resize(result.size() - 1);
  result.append(path.data(), path.size());
#endif
  return result;
}


template <bool isRequest, typename Body>
struct fmt::formatter<http::message<isRequest, Body>>
{
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
  {
    return ctx.end();
  }

  template <typename FormatContext>
  constexpr auto format(const http::message<isRequest, Body>& input, FormatContext& ctx) -> decltype(ctx.out())
  {
    auto c = input.base();
    auto out = ctx.out();
    const std::string s[2]{ "response:", "request:" };
    fmt::format_to(out, "{}\n", s[isRequest]);
    for (auto it = c.begin(); it != c.end(); ++it)
    {
      out = fmt::format_to(out, "{}{:<30}{}\n", std::string(35, ' '), it->name_string(), it->value());
    }
    if constexpr (isRequest)
      out = fmt::format_to(out, "{}Method: {} for {}", std::string(34, ' '), input.method(), input.target());
    else
      out = fmt::format_to(out, "{}Result: {} ({})", std::string(34, ' '), input.result(), static_cast<int>(input.result()));
    return out;
  }
};

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template <
  class Body, class Allocator,
  class Send>
void handle_request(
  beast::string_view doc_root,
  http::request<Body, http::basic_fields<Allocator>>&& req,
  Send&& send)
{
  spdlog::info("::handle_request");
  spdlog::info("{}{}", std::string(2, ' '), req);

  // Returns a bad request response
  auto const bad_request =
    [&req](beast::string_view why)
    {
      http::response<http::string_body> res{ http::status::bad_request, req.version() };
      res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
      res.set(http::field::content_type, "text/html");
      res.keep_alive(req.keep_alive());
      res.body() = std::string(why);
      res.prepare_payload();
      return res;
    };

  // Returns a not found response
  auto const not_found =
    [&req](beast::string_view target)
    {
      http::response<http::string_body> res{ http::status::not_found, req.version() };
      res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
      res.set(http::field::content_type, "text/html");
      res.keep_alive(req.keep_alive());
      res.body() = "The resource '" + std::string(target) + "' was not found.";
      res.prepare_payload();
      return res;
    };

  // Returns a server error response
  auto const server_error =
    [&req](beast::string_view what)
    {
      http::response<http::string_body> res{ http::status::internal_server_error, req.version() };
      res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
      res.set(http::field::content_type, "text/html");
      res.keep_alive(req.keep_alive());
      res.body() = "An error occurred: '" + std::string(what) + "'";
      res.prepare_payload();
      return res;
    };

  // Make sure we can handle the method
  if (req.method() != http::verb::get &&
    req.method() != http::verb::head)
    return send(bad_request("Unknown HTTP-method"));

  // Request path must be absolute and not contain "..".
  if (req.target().empty() ||
    req.target()[0] != '/' ||
    req.target().find("..") != beast::string_view::npos)
    return send(bad_request("Illegal request-target"));

  // Build the path to the requested file
  std::string path = path_cat(doc_root, req.target());
  if (req.target().back() == '/')
    path.append("index.html");

  // Attempt to open the file
  beast::error_code ec;
  http::file_body::value_type body;
  body.open(path.c_str(), beast::file_mode::scan, ec);

  // Handle the case where the file doesn't exist
  if (ec == beast::errc::no_such_file_or_directory)
    return send(not_found(req.target()));

  // Handle an unknown error
  if (ec)
    return send(server_error(ec.message()));

  // Cache the size since we need it after the move
  auto const file_size = body.size();
  spdlog::info("file_size:{:>20}", file_size);

  //---new-logic---------------------------------------------------------------------
  std::uint64_t start = 0, end = file_size - 1;
  auto range_hdr = req.base()["Range"];
  bool partial = false;

  if (!range_hdr.empty())
  {
    partial = true;
    std::string range_str = range_hdr.to_string(); // "bytes=500-"
    if (range_str.find("bytes=") == 0)
    {
      range_str = range_str.substr(6);
      auto dash = range_str.find('-');
      start = std::stoll(range_str.substr(0, dash));
      if (dash + 1 < range_str.size())
      {
        end = std::stoll(range_str.substr(dash + 1));
      }
    }
  }

  std::uint64_t length = end - start + 1;
  std::string sbody(length, '\0');
  spdlog::info("start    :{:>20}", start);
  spdlog::info("length   :{:>20}", length);
  body.file().seek(start, ec);
  // Handle an unknown error
  if (ec)
    return send(server_error(ec.message()));
  body.file().read(static_cast<void*>(&sbody[0]), length, ec);
  // Handle an unknown error
  if (ec)
    return send(server_error(ec.message()));
  //---new-logic---------------------------------------------------------------------

  // Respond to HEAD request
  if (req.method() == http::verb::head)
  {
    http::response<http::empty_body> res{ http::status::ok, req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, mime_type(path));
    res.content_length(file_size);
    res.keep_alive(req.keep_alive());
    return send(std::move(res));
  }

  // Respond to GET request
  http::response<http::string_body> res{
      partial ? http::status::partial_content : http::status::ok, req.version() };
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, mime_type(path));
  res.set(http::field::content_length, std::to_string(body.size()));
  res.keep_alive(req.keep_alive());
  if (partial)
    res.set(http::field::content_range,
      "bytes " + std::to_string(start) + "-" + std::to_string(end) + "/" + std::to_string(file_size));
  res.body() = std::move(sbody);
  res.prepare_payload();
  spdlog::info("{}{}", std::string(2, ' '), res);
  return send(std::move(res));
}

//------------------------------------------------------------------------------

// // Report a failure
// void fail(beast::error_code ec, char const *what)
// {
//   std::cerr << what << ": " << ec.message() << "\n";
// }

// Echoes back all received WebSocket messages
class websocket_session : public std::enable_shared_from_this<websocket_session>
{
  websocket::stream<beast::tcp_stream> ws_;
  beast::flat_buffer buffer_;

public:
  // Take ownership of the socket
  explicit websocket_session(tcp::socket&& socket)
    : ws_(std::move(socket))
  {
  }

  // Start the asynchronous accept operation
  template <class Body, class Allocator>
  void
    do_accept(http::request<Body, http::basic_fields<Allocator>> req)
  {
    // Set suggested timeout settings for the websocket
    ws_.set_option(
      websocket::stream_base::timeout::suggested(
        beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    ws_.set_option(websocket::stream_base::decorator(
      [](websocket::response_type& res)
      {
        res.set(http::field::server,
          std::string(BOOST_BEAST_VERSION_STRING) +
          " advanced-server");
      }));

    // Accept the websocket handshake
    ws_.async_accept(
      req,
      beast::bind_front_handler(
        &websocket_session::on_accept,
        shared_from_this()));
  }

private:
  void
    on_accept(beast::error_code ec)
  {
    if (ec)
      return fail(ec, "accept");

    // Read a message
    do_read();
  }

  void
    do_read()
  {
    // Read a message into our buffer
    ws_.async_read(
      buffer_,
      beast::bind_front_handler(
        &websocket_session::on_read,
        shared_from_this()));
  }

  void
    on_read(
      beast::error_code ec,
      std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    // This indicates that the websocket_session was closed
    if (ec == websocket::error::closed)
      return;

    if (ec)
      fail(ec, "read");

    // Echo the message
    ws_.text(ws_.got_text());
    ws_.async_write(
      buffer_.data(),
      beast::bind_front_handler(
        &websocket_session::on_write,
        shared_from_this()));
  }

  void
    on_write(
      beast::error_code ec,
      std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    if (ec)
      return fail(ec, "write");

    // Clear the buffer
    buffer_.consume(buffer_.size());

    // Do another read
    do_read();
  }
};

//------------------------------------------------------------------------------

// Handles an HTTP server connection
class http_session : public std::enable_shared_from_this<http_session>
{
  // This queue is used for HTTP pipelining.
  class queue
  {
    enum
    {
      // Maximum number of responses we will queue
      limit = 16
    };

    // The type-erased, saved work item
    struct work
    {
      virtual ~work() = default;
      virtual void operator()() = 0;
    };

    http_session& self_;
    std::vector<std::unique_ptr<work>> items_;

  public:
    explicit queue(http_session& self)
      : self_(self)
    {
      static_assert(limit > 0, "queue limit must be positive");
      items_.reserve(limit);
    }

    // Returns `true` if we have reached the queue limit
    bool
      is_full() const
    {
      return items_.size() >= limit;
    }

    // Called when a message finishes sending
    // Returns `true` if the caller should initiate a read
    bool
      on_write()
    {
      BOOST_ASSERT(!items_.empty());
      auto const was_full = is_full();
      items_.erase(items_.begin());
      spdlog::debug("pending works of\t\t\t{}: {}/{}", static_cast<void*>(&self_), items_.size(), limit);
      if (!items_.empty())
        (*items_.front())();
      return was_full;
    }

    // Called by the HTTP handler to send a response.
    template <bool isRequest, class Body, class Fields>
    void
      operator()(http::message<isRequest, Body, Fields>&& msg)
    {
      // This holds a work item
      struct work_impl : work
      {
        http_session& self_;
        http::message<isRequest, Body, Fields> msg_;

        work_impl(
          http_session& self,
          http::message<isRequest, Body, Fields>&& msg)
          : self_(self), msg_(std::move(msg))
        {
        }

        void
          operator()()
        {
          http::async_write(
            self_.stream_,
            msg_,
            beast::bind_front_handler(
              &http_session::on_write,
              self_.shared_from_this(),
              msg_.need_eof()));
        }
      };

      // Allocate and store the work
      items_.push_back(
        boost::make_unique<work_impl>(self_, std::move(msg)));
      spdlog::debug("pending works of\t\t\t{}: {}/{}", static_cast<void*>(&self_), items_.size(), limit);

      // If there was no previous work, start this one
      if (items_.size() == 1)
        (*items_.front())();
    }
  };

  beast::tcp_stream stream_;
  beast::flat_buffer buffer_;
  std::shared_ptr<std::string const> doc_root_;
  queue queue_;

  // The parser is stored in an optional container so we can
  // construct it from scratch it at the beginning of each new message.
  boost::optional<http::request_parser<http::string_body>> parser_;

public:
  // Take ownership of the socket
  http_session(
    tcp::socket&& socket,
    std::shared_ptr<std::string const> const& doc_root)
    : stream_(std::move(socket)), doc_root_(doc_root), queue_(*this)
  {
    spdlog::debug("http_session::http_session() for\t {}", static_cast<void*>(this));
  }

  // Start the session
  void
    run()
  {
    // We need to be executing within a strand to perform async operations
    // on the I/O objects in this session. Although not strictly necessary
    // for single-threaded contexts, this example code is written to be
    // thread-safe by default.
    net::dispatch(
      stream_.get_executor(),
      beast::bind_front_handler(
        &http_session::do_read,
        this->shared_from_this()));
  }

private:
  void
    do_read()
  {
    // Construct a new parser for each message
    parser_.emplace();

    // Apply a reasonable limit to the allowed size
    // of the body in bytes to prevent abuse.
    parser_->body_limit(10000);

    // Set the timeout.
    stream_.expires_after(std::chrono::seconds(30));

    // Read a request using the parser-oriented interface
    http::async_read(
      stream_,
      buffer_,
      *parser_,
      beast::bind_front_handler(
        &http_session::on_read,
        shared_from_this()));
  }

  void
    on_read(beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    // This means they closed the connection
    if (ec == http::error::end_of_stream)
      return do_close();

    if (ec)
      return fail(ec, "read");

    // See if it is a WebSocket Upgrade
    if (websocket::is_upgrade(parser_->get()))
    {
      // Create a websocket session, transferring ownership
      // of both the socket and the HTTP request.
      std::make_shared<websocket_session>(
        stream_.release_socket())
        ->do_accept(parser_->release());
      return;
    }

    // Send the response
    handle_request(*doc_root_, parser_->release(), queue_);

    // If we aren't at the queue limit, try to pipeline another request
    if (!queue_.is_full())
      do_read();
  }

  void
    on_write(bool close, beast::error_code ec, std::size_t bytes_transferred)
  {
    if (ec)
      return fail(ec, "write");

    spdlog::info("written  :{:>20}", bytes_transferred);

    if (close)
    {
      // This means we should close the connection, usually because
      // the response indicated the "Connection: close" semantic.
      return do_close();
    }

    // Inform the queue that a write completed
    if (queue_.on_write())
    {
      // Read another request
      do_read();
    }
  }

  void
    do_close()
  {
    spdlog::debug("http_session::do_close() for\t {}", static_cast<void*>(this));
    // Send a TCP shutdown
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
  }
};

//------------------------------------------------------------------------------

void
listener::do_accept()
{
  spdlog::debug("listener::do_accept()");
  // The new connection gets its own strand
  acceptor_.async_accept(
    net::make_strand(ioc_),
    beast::bind_front_handler(
      &listener::on_accept,
      shared_from_this()));
}

void
listener::on_accept(beast::error_code ec, tcp::socket socket)
{
  if (ec)
  {
    fail(ec, "accept");
  }
  else
  {
    spdlog::debug("Accept new connection: total ({})", ++connections);
    // Create the http session and run it
    std::make_shared<http_session>(
      std::move(socket),
      doc_root_)
      ->run();
  }

  // Accept another connection
  do_accept();
}
