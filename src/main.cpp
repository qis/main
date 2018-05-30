#include <common.h>

class server {
public:
  server(boost::asio::io_context& ioc) : acceptor_(ioc) {
  }

  awaitable<void> run(const std::string& host, const std::string& port) {
    boost::system::error_code ec;
    auto token = redirect_error(co_await this_coro::token(), ec);

    tcp::resolver resolver{ acceptor_.get_io_service() };
    auto it = co_await resolver.async_resolve(host, port, token);

    acceptor_.open(it->endpoint().protocol());
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_.bind(it->endpoint());
    acceptor_.listen(boost::asio::socket_base::max_listen_connections);

    while (true) {
      auto socket = co_await acceptor_.async_accept(token);
      if (ec) {
        std::cerr << "server accept error: [" << ec << "] " << ec.message() << std::endl;
        continue;
      }
      co_spawn(acceptor_.get_io_context(),
        [this, socket = std::move(socket)]() mutable -> awaitable<void> {
          std::string name;
          try {
            name =
              socket.remote_endpoint().address().to_string() + ':' + std::to_string(socket.remote_endpoint().port());
            std::cout << name << " opened" << std::endl;
            co_await session(name, socket);
          }
          catch (const boost::system::system_error& e) {
            std::cerr << name << " error: [" << e.code() << "] " << e.what() << std::endl;
          }
          catch (const std::exception& e) {
            std::cerr << name << " error: " << e.what() << std::endl;
          }
          std::cout << name << " closed" << std::endl;
        },
        detached);
    }
  }

private:
  awaitable<void> session(const std::string& name, tcp::socket& socket) {
    boost::system::error_code ec;
    boost::beast::flat_buffer buffer;
    http::request<http::string_body> req;
    auto token = redirect_error(co_await this_coro::token(), ec);
    while (true) {
      req = {};
      co_await http::async_read(socket, buffer, req, token);
      if (ec) {
        if (ec != boost::system::errc::connection_reset && ec != http::error::end_of_stream) {
          throw boost::system::system_error(ec, "http read error");
        }
        break;
      }
      co_await handle(socket, req);
    }
  }

  awaitable<void> handle(tcp::socket& socket, http::request<http::string_body>& req) {
    const json data = { { "ip", socket.remote_endpoint().address().to_string() } };
    http::response<http::string_body> res{ http::status::ok, req.version() };
    res.set(http::field::server, COMPANY " " PROJECT "/" VERSION);
    res.set(http::field::content_type, "text/plain");
    res.keep_alive(req.keep_alive());
    res.body() = data.dump(2);
    res.prepare_payload();
    co_await http::async_write(socket, res, co_await this_coro::token());
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
  int ret = 0;
  const std::string host = argc > 1 ? argv[1] : "0.0.0.0";
  const std::string port = argc > 2 ? argv[2] : "8080";
  boost::asio::io_context ioc{ 1 };
  boost::asio::signal_set sig{ ioc, SIGINT, SIGTERM };
  sig.async_wait([&](auto, auto) { ioc.stop(); });
  co_spawn(ioc,
    [&]() -> awaitable<void> {
      try {
        server server{ ioc };
        co_await server.run(host, port);
      }
      catch (const boost::system::system_error& e) {
        std::cerr << "client error: [" << e.code() << "] " << e.what() << std::endl;
      }
      catch (const std::exception& e) {
        std::cerr << "client error: " << e.what() << std::endl;
        ret = 1;
      }
    },
    detached);
  ioc.run();
  return ret;
}
