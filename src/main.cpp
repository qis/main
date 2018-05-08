#if 0
#include <config.h>
#include <ice/async.h>
#include <ice/net/tcp/socket.h>
#include <ice/scope.h>
#include <array>
#include <exception>
#include <iostream>
#include <string_view>

constexpr std::string_view g_response =
  "HTTP/1.1 200 OK\r\n"
  "Date: Thu, 19 Apr 2018 04:34:52 GMT\r\n"
  "Connection: keep-alive\r\n"
  "Server: ice/0.1.0\r\n"
  "Content-Type: text/html; charset=utf-8\r\n"
  "Content-Length: 0\r\n"
  "\r\n";

ice::task respond(ice::net::tcp::socket& client, ice::async_mutex& mutex, std::string_view data) {
  const auto lock = co_await mutex.scoped_lock_async();
  const auto sent = co_await client.send(data.data(), data.size());
  if (sent != data.size()) {
    std::cerr << "could not send response to " << client << " (" << sent << "/" << data.size() << ")" << std::endl;
  }
  co_return;
}

ice::task handle(ice::net::tcp::socket client) {
  client.set(ice::net::option::no_delay(true));
  std::array<char, 1024> buffer;
  ice::async_mutex mutex;
  bool newline = false;
  while (true) {
    const auto size = co_await client.recv(buffer.data(), buffer.size());
    if (!size) {
      break;
    }
    for (std::size_t i = 0; i < size; i++) {
      switch (buffer[i]) {
      case '\r': break;
      case '\n':
        if (newline) {
          respond(client, mutex, g_response);
          newline = false;
        } else {
          newline = true;
        }
        break;
      default: newline = false; break;
      }
    }
  }
  co_await mutex.lock_async();  // wait until all send operations finish
  co_return;
}

ice::task server(ice::context& context) {
  const auto se = ice::on_scope_exit([&]() { context.stop(); });
  ice::net::endpoint endpoint("127.0.0.1", 8080);
  ice::net::tcp::socket socket(context, endpoint.family());
  socket.set(ice::net::option::reuse_address(true));
  socket.bind(endpoint);
  socket.listen();
  std::cout << PROJECT << ' ' << VERSION << ' ' << COMPANY << ' ' << COPYING << std::endl;
  while (true) {
    handle(co_await socket.accept());
  }
  co_return;
}

int main() {
  try {
    ice::context context;
    server(context);
    context.run();
  }
  catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
}
#endif

#include <date/tz.h>
#include <chrono>
#include <filesystem>
#include <iostream>

int main() {
  auto path = std::filesystem::current_path();
  while (true) {
    if (std::filesystem::is_directory(path / "res" / "tzdata") || path == path.parent_path()) {
      break;
    }
    path = path.parent_path();
  }
  date::set_install((path / "res" / "tzdata").u8string());
  const auto time = date::make_zoned(date::locate_zone("Europe/Berlin"), std::chrono::system_clock::now());
  std::cout << time << std::endl;
}
