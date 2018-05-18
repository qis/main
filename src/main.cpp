// git clone git@github.com:qis/bcrypt ${VCPKG}/ports/bcrypt
// git clone git@github.com:qis/compat ${VCPKG}/ports/compat
// git clone git@github.com:qis/sql ${VCPKG}/ports/sql
// vcpkg install benchmark boost-beast date fmt gtest jsoncpp openssl utfcpp bcrypt compat sql

#include <config.h>
#include <boost/asio/experimental/co_spawn.hpp>
#include <boost/asio/experimental/detached.hpp>
#include <boost/asio/experimental/redirect_error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <array>
#include <cstdio>

using boost::asio::ip::tcp;
using boost::asio::experimental::co_spawn;
using boost::asio::experimental::detached;
using boost::asio::experimental::redirect_error;
namespace this_coro = boost::asio::experimental::this_coro;

template <typename T>
using awaitable = boost::asio::experimental::awaitable<T, boost::asio::io_context::executor_type>;

awaitable<void> echo(tcp::socket socket) {
  boost::system::error_code ec;
  auto token = redirect_error(co_await this_coro::token(), ec);
  std::array<char, 4096> data;
  while (true) {
    std::size_t size = co_await socket.async_read_some(boost::asio::buffer(data), token);
    if (ec) {
      std::fprintf(stderr, "read error: %s\n", ec.message().data());
      break;
    }
    co_await async_write(socket, boost::asio::buffer(data, size), token);
    if (ec) {
      std::fprintf(stderr, "write error: %s\n", ec.message().data());
      break;
    }
  }
}

awaitable<void> listener() {
  boost::system::error_code ec;
  auto token = redirect_error(co_await this_coro::token(), ec);
  auto executor = co_await this_coro::executor();
  tcp::acceptor acceptor(executor.context(), { tcp::v4(), 8080 });
  while (true) {
    auto socket = co_await acceptor.async_accept(token);
    if (ec) {
      std::fprintf(stderr, "accept error: %s\n", ec.message().data());
      continue;
    }
    co_spawn(executor, [socket = std::move(socket)]() mutable { return echo(std::move(socket)); }, detached);
  }
}

int main() {
  try {
    boost::asio::io_context io_context(1);
    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto) { io_context.stop(); });
    co_spawn(io_context, listener, detached);
    io_context.run();
  }
  catch (std::exception& e) {
    std::fprintf(stderr, "critical error: %s\n", e.what());
  }
}
