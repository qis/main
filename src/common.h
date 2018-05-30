#include <config.h>

#ifdef _MSC_VER
#  pragma warning(push, 0)
#else
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wall"
#  pragma clang diagnostic ignored "-Wextra"
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wshorten-64-to-32"
#endif

#include <boost/asio/connect.hpp>
#include <boost/asio/experimental/co_spawn.hpp>
#include <boost/asio/experimental/detached.hpp>
#include <boost/asio/experimental/redirect_error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#ifdef _MSC_VER
#  pragma warning(pop)
#else
#  pragma clang diagnostic pop
#endif

#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <string>

using boost::asio::ip::tcp;
using boost::asio::experimental::co_spawn;
using boost::asio::experimental::detached;
using boost::asio::experimental::redirect_error;
using boost::beast::string_view;
using nlohmann::json;

namespace this_coro = boost::asio::experimental::this_coro;
namespace http = boost::beast::http;

template <class T>
using awaitable = boost::asio::experimental::awaitable<T, boost::asio::io_context::executor_type>;
