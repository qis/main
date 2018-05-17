// git clone git@github.com:qis/bcrypt ${VCPKG}/ports/bcrypt
// git clone git@github.com:qis/compat ${VCPKG}/ports/compat
// git clone git@github.com:qis/sql ${VCPKG}/ports/sql
// vcpkg install benchmark boost-beast date fmt gtest jsoncpp openssl utfcpp bcrypt compat sql

#include <config.h>
#include <boost/asio/experimental/co_spawn.hpp>
#include <boost/asio/experimental/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <cstdio>

using boost::asio::ip::tcp;
using boost::asio::experimental::co_spawn;
using boost::asio::experimental::detached;
namespace this_coro = boost::asio::experimental::this_coro;

template <typename T>
using awaitable = boost::asio::experimental::awaitable<T, boost::asio::io_context::executor_type>;

awaitable<void> echo(tcp::socket socket) {
  auto token = co_await this_coro::token();
  try {
    char data[1024];
    while (true) {
      std::size_t n = co_await socket.async_read_some(boost::asio::buffer(data), token);
      co_await async_write(socket, boost::asio::buffer(data, n), token);
    }
  }
  catch (std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
  }
}

awaitable<void> listener() {
  auto executor = co_await this_coro::executor();
  auto token = co_await this_coro::token();
  tcp::acceptor acceptor(executor.context(), { tcp::v4(), 8080 });
  while (true) {
    tcp::socket socket = co_await acceptor.async_accept(token);
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
