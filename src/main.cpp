#include <boost/asio.hpp>
#include <boost/asio/experimental/co_spawn.hpp>
#include <boost/asio/experimental/detached.hpp>
#include <boost/asio/experimental/redirect_error.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <iostream>
#include <memory>
#include <optional>
#include <cstdlib>

using boost::asio::ip::tcp;
using boost::asio::experimental::co_spawn;
using boost::asio::experimental::detached;
using boost::asio::experimental::redirect_error;
namespace this_coro = boost::asio::experimental::this_coro;
namespace http = boost::beast::http;

using strand = boost::asio::strand<boost::asio::io_context::executor_type>;

#if 1
template <class T>
using awaitable = boost::asio::experimental::awaitable<T, boost::asio::io_context::executor_type>;
#else
template <class T>
using awaitable = boost::asio::experimental::awaitable<T, strand>;
#endif

// Report a failure
void fail(boost::system::error_code ec, char const* what) {
  std::cerr << what << ": " << ec.message() << "\n";
}

// Return a reasonable mime type based on the extension of a file.
boost::beast::string_view mime_type(boost::beast::string_view path) {
  using boost::beast::iequals;
  auto const ext = [&path] {
    auto const pos = path.rfind(".");
    if (pos == boost::beast::string_view::npos)
      return boost::beast::string_view{};
    return path.substr(pos);
  }();
  // clang-format off
  if (iequals(ext, ".htm")) return "text/html";
  if (iequals(ext, ".html")) return "text/html";
  if (iequals(ext, ".php")) return "text/html";
  if (iequals(ext, ".css")) return "text/css";
  if (iequals(ext, ".txt")) return "text/plain";
  if (iequals(ext, ".js")) return "application/javascript";
  if (iequals(ext, ".json")) return "application/json";
  if (iequals(ext, ".xml")) return "application/xml";
  if (iequals(ext, ".swf")) return "application/x-shockwave-flash";
  if (iequals(ext, ".flv")) return "video/x-flv";
  if (iequals(ext, ".png")) return "image/png";
  if (iequals(ext, ".jpe")) return "image/jpeg";
  if (iequals(ext, ".jpeg")) return "image/jpeg";
  if (iequals(ext, ".jpg")) return "image/jpeg";
  if (iequals(ext, ".gif")) return "image/gif";
  if (iequals(ext, ".bmp")) return "image/bmp";
  if (iequals(ext, ".ico")) return "image/vnd.microsoft.icon";
  if (iequals(ext, ".tiff")) return "image/tiff";
  if (iequals(ext, ".tif")) return "image/tiff";
  if (iequals(ext, ".svg")) return "image/svg+xml";
  if (iequals(ext, ".svgz")) return "image/svg+xml";
  // clang-format on
  return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string path_cat(boost::beast::string_view base, boost::beast::string_view path) {
  if (base.empty())
    return path.to_string();
  std::string result = base.to_string();
#if BOOST_MSVC
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

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template <class Body, class Allocator, class Send>
auto handle_request(
  boost::beast::string_view doc_root, http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
  // Returns a bad request response
  auto const bad_request = [&req](boost::beast::string_view why) {
    http::response<http::string_body> res{ http::status::bad_request, req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = why.to_string();
    res.prepare_payload();
    return res;
  };

  // Returns a not found response
  auto const not_found = [&req](boost::beast::string_view target) {
    http::response<http::string_body> res{ http::status::not_found, req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "The resource '" + target.to_string() + "' was not found.";
    res.prepare_payload();
    return res;
  };

  // Returns a server error response
  auto const server_error = [&req](boost::beast::string_view what) {
    http::response<http::string_body> res{ http::status::internal_server_error, req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "An error occurred: '" + what.to_string() + "'";
    res.prepare_payload();
    return res;
  };

  // Make sure we can handle the method
  if (req.method() != http::verb::get && req.method() != http::verb::head)
    return send(bad_request("Unknown HTTP-method"));

  // Request path must be absolute and not contain "..".
  if (req.target().empty() || req.target()[0] != '/' || req.target().find("..") != boost::beast::string_view::npos)
    return send(bad_request("Illegal request-target"));

  // Build the path to the requested file
  std::string path = path_cat(doc_root, req.target());
  if (req.target().back() == '/')
    path.append("index.html");

  // Attempt to open the file
  boost::beast::error_code ec;
  http::file_body::value_type body;
  body.open(path.c_str(), boost::beast::file_mode::scan, ec);

  // Handle the case where the file doesn't exist
  if (ec == boost::system::errc::no_such_file_or_directory)
    return send(not_found(req.target()));

  // Handle an unknown error
  if (ec)
    return send(server_error(ec.message()));

  // Cache the size since we need it after the move
  auto const size = body.size();

  // Respond to HEAD request
  if (req.method() == http::verb::head) {
    http::response<http::empty_body> res{ http::status::ok, req.version() };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, mime_type(path));
    res.content_length(size);
    res.keep_alive(req.keep_alive());
    return send(std::move(res));
  }

  // Respond to GET request
  http::response<http::file_body> res{ std::piecewise_construct, std::make_tuple(std::move(body)),
    std::make_tuple(http::status::ok, req.version()) };
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, mime_type(path));
  res.content_length(size);
  res.keep_alive(req.keep_alive());
  return send(std::move(res));
}

template <class Stream, class Message>
awaitable<bool> write_message(Stream& stream, Message msg) {
  co_await http::async_write(stream, msg, co_await this_coro::token());
  co_return msg.need_eof();
}

awaitable<void> session(tcp::socket socket, std::shared_ptr<std::string const> doc_root) {
  boost::system::error_code ec;
  boost::beast::flat_buffer buffer;
  http::request<http::string_body> req;

  // Get executor
  auto executor = co_await this_coro::executor();

  // Get token and redirect error
  auto token = redirect_error(co_await this_coro::token(), ec);

  // Start reading requests
  for (;;) {
    // Make the request empty before reading,
    // otherwise the operation behavior is undefined.
    req = {};

    // Read a request
    co_await http::async_read(socket, buffer, req, token);
    if (ec == http::error::end_of_stream) {
      // The remote host closed the connection
      break;
    }
    if (ec)
      co_return fail(ec, "read");

    std::optional<awaitable<bool>> send;
    handle_request(*doc_root, std::move(req), [&](auto&& msg) {
      send.emplace(write_message(socket, std::move(msg)));
    });

    if (co_await *send) {
      // This means we should close the connection, usually because
      // the response indicated the "Connection: close" semantic.
      break;
    }
  }

  // Send a TCP shutdown
  socket.shutdown(tcp::socket::shutdown_send, ec);
}

awaitable<void> listen(tcp::endpoint endpoint, std::shared_ptr<std::string const> doc_root) {
  boost::system::error_code ec;

  // Get executor
  auto executor = co_await this_coro::executor();

  // Create acceptor
  tcp::acceptor acceptor(executor.context());

  // Open the acceptor
  acceptor.open(endpoint.protocol(), ec);
  if (ec) {
    fail(ec, "open");
    co_return;
  }

  // Allow address reuse
  acceptor.set_option(boost::asio::socket_base::reuse_address(true));
  if (ec) {
    fail(ec, "set_option");
    co_return;
  }

  // Bind to the server address
  acceptor.bind(endpoint, ec);
  if (ec) {
    fail(ec, "bind");
    co_return;
  }

  // Start listening for connections
  acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
  if (ec) {
    fail(ec, "listen");
    co_return;
  }

  // Get token and redirect error
  auto token = redirect_error(co_await this_coro::token(), ec);

  // Start accepting incoming connections
  while (acceptor.is_open()) {
    auto socket = co_await acceptor.async_accept(token);
    if (ec) {
      fail(ec, "accept");
      continue;
    }

    // Run session in a detached coroutine
    co_spawn(executor, [&, s = std::move(socket)]() mutable { return session(std::move(s), doc_root); }, detached);
  }
}

int main(int argc, char* argv[]) {
  // Check command line arguments.
  //if (argc != 5) {
  //  std::cerr <<
  //    "Usage: http-server-coroutine-ts <address> <port> <doc_root> <threads>\n" <<
  //    "Example:\n" <<
  //    "    http-server-stackless 0.0.0.0 8080 . 1\n";
  //  return EXIT_FAILURE;
  //}
  auto const address = boost::asio::ip::make_address(argc > 1 ? argv[1] : "0.0.0.0");
  auto const port = static_cast<unsigned short>(std::atoi(argc > 2 ? argv[2] : "8080"));
  auto const doc_root = std::make_shared<std::string>(argc > 3 ? argv[3] : "C:\\Libraries\\main");
  auto const threads = std::max<int>(1, std::atoi(argc > 4 ? argv[4] : "4"));

  std::cout << address << ':' << port << ' ' << *doc_root << " (" << threads << ")" << std::endl;

  // The io_context is required for all I/O
  boost::asio::io_context ioc{ threads };

  // Create and launch a listening port
  co_spawn(ioc, [&]() { return listen({ address, port }, doc_root); }, detached);

  // Run the I/O service on the requested number of threads
  std::vector<std::thread> v;
  v.reserve(static_cast<std::size_t>(threads - 1));
  for (auto i = threads - 1; i > 0; --i)
    v.emplace_back([&ioc] { ioc.run(); });
  ioc.run();

  return EXIT_SUCCESS;
}
