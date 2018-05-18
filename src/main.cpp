// git clone git@github.com:qis/bcrypt ${VCPKG}/ports/bcrypt
// git clone git@github.com:qis/compat ${VCPKG}/ports/compat
// git clone git@github.com:qis/sql ${VCPKG}/ports/sql
// vcpkg install benchmark boost-beast date fmt gtest jsoncpp openssl utfcpp bcrypt compat sql

#include <config.h>

#include <boost/asio.hpp>
#include <boost/asio/experimental/co_spawn.hpp>
#include <boost/asio/experimental/detached.hpp>
#include <boost/asio/experimental/redirect_error.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/core/flat_buffer.hpp>

namespace net {

using namespace boost::asio;
namespace http = boost::beast::http;
namespace this_coro = experimental::this_coro;

using boost::system::error_code;

using boost::beast::flat_buffer;

using experimental::co_spawn;
using experimental::detached;
using experimental::redirect_error;
using experimental::awaitable;

}  // namespace net

#include <cstdlib>
#include <utility>
#include <cstdio>

template <typename T>
using awaitable = net::awaitable<T, net::io_context::executor_type>;

template <typename T>
using strand = net::awaitable<T, net::strand<net::io_context::executor_type>>;

awaitable<void> session(net::ip::tcp::socket socket) {
  net::error_code ec;
  auto token = net::redirect_error(co_await net::this_coro::token(), ec);

  net::flat_buffer buffer;
  net::http::request<net::http::string_body> request;

  while (true) {
    (void)co_await net::http::async_read(socket, buffer, request, token);
    if (ec) {
      std::fprintf(stderr, "request error: %s\n", ec.message().data());
      break;
    }
    auto response = net::http::response<net::http::string_body>(net::http::status::ok, request.version());
    response.body() = "OK\n";
    response.prepare_payload();
    (void)co_await net::http::async_write(socket, response, token);
    if (ec) {
      std::fprintf(stderr, "response error: %s\n", ec.message().data());
      break;
    }
  }
  socket.shutdown(net::ip::tcp::socket::shutdown_both);
  socket.close();
}

awaitable<void> listener() {
  net::error_code ec;
  auto token = net::redirect_error(co_await net::this_coro::token(), ec);
  auto executor = co_await net::this_coro::executor();
  net::ip::tcp::acceptor acceptor(executor.context(), { net::ip::tcp::v4(), 8080 });
  acceptor.set_option(net::socket_base::reuse_address(true));
  while (true) {
    auto socket = co_await acceptor.async_accept(token);
    if (ec) {
      std::fprintf(stderr, "accept error: %s\n", ec.message().data());
      continue;
    }
    net::co_spawn(executor, [socket = std::move(socket)]() mutable {
      return session(std::move(socket));
    }, net::detached);
  }
}

int main() {
  try {
    net::io_context io_context(1);
    net::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto) { io_context.stop(); });
    net::co_spawn(io_context, listener, net::detached);
    io_context.run();
  }
  catch (std::exception& e) {
    std::fprintf(stderr, "critical error: %s\n", e.what());
  }
}
