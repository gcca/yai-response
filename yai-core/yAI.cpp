#include <cstring>
#include <iostream>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>

#include "yAI.hpp"

namespace yai {

namespace {

class Stream_ : public yai::Stream {
public:
  explicit Stream_(boost::asio::ip::tcp::socket &socket) : socket_{socket} {}

  boost::asio::awaitable<std::size_t> Write(const void *data,
                                            std::size_t size) final {
    return boost::asio::async_write(socket_, boost::asio::buffer(data, size),
                                    boost::asio::use_awaitable);
  }

  boost::asio::awaitable<std::size_t> Read(char *data, std::size_t size) final {
    return socket_.async_read_some(boost::asio::buffer(data, size),
                                   boost::asio::use_awaitable);
  }

  boost::asio::ip::tcp::socket &socket_;
};

inline static boost::asio::awaitable<std::size_t>
HandleUnrecognized(boost::asio::ip::tcp::socket &socket) {
  std::cout << "Unrecognized handler" << std::endl;

  const std::array<char, sizeof(std::size_t)> status = {0x01, 0x00};

  return boost::asio::async_write(
      socket, boost::asio::buffer(status.data(), status.size()),
      boost::asio::use_awaitable);
}

static boost::asio::awaitable<void>
Dispatch(boost::asio::ip::tcp::socket socket, Handler *handlers,
         std::size_t handlers_size) {
  try {
    std::size_t handler_id = 0;
    if (sizeof(handler_id) !=
        co_await socket.async_read_some(
            boost::asio::buffer(&handler_id, sizeof(handler_id)),
            boost::asio::use_awaitable)) {
      co_await HandleUnrecognized(socket);
      co_return;
    }
    std::cout << "Handler ID: " << handler_id << std::endl;

    if (handler_id >= handlers_size) {
      co_await HandleUnrecognized(socket);
    } else {
      auto stream = std::make_unique<Stream_>(socket);
      co_await handlers[handler_id](*stream);
    }

  } catch (const boost::system::system_error &e) {
    if (e.code() != boost::asio::error::eof) {
      std::cerr << "Server error: " << e.what() << std::endl;
    }
  } catch (std::exception &e) {
    std::cerr << "Server Exception: " << e.what() << std::endl;
  }
}

static boost::asio::awaitable<void>
Listener(std::uint16_t port, Handler *handlers, std::size_t size) {
  auto executor = co_await boost::asio::this_coro::executor;
  boost::asio::ip::tcp::acceptor acceptor(executor,
                                          {boost::asio::ip::tcp::v4(), port});
  for (;;) {
    boost::asio::ip::tcp::socket socket =
        co_await acceptor.async_accept(boost::asio::use_awaitable);
    co_spawn(executor, Dispatch(std::move(socket), handlers, size),
             boost::asio::detached);
  }
}

} // namespace

Stream::~Stream() {}

Server::Server(std::uint16_t port, Handler *handlers, std::size_t size)
    : port_(port), handlers_(handlers), size_(size) {}

void Server::Run() {
  try {
    boost::asio::io_context io_context{1};

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto) { io_context.stop(); });

    co_spawn(io_context, Listener(port_, handlers_, size_),
             boost::asio::detached);

    io_context.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
}

Messager::Messager(std::size_t reserve)
    : data_{std::make_unique<std::uint8_t[]>(reserve)},
      carret_{data_.get() + 2 * sizeof(std::uint32_t)} {}

void Messager::Status(std::uint32_t status) {
  std::memcpy(data_.get(), &status, sizeof(status));
}

void Messager::AppendLength(std::uint32_t length) {
  std::memcpy(carret_, &length, sizeof(length));
  carret_ += sizeof(length);
}

void Messager::AppendStr(const char *str) {
  while (*str)
    *carret_++ = static_cast<std::uint8_t>(*str++);
}

void *Messager::Flush() {
  std::uint32_t size = static_cast<std::uint32_t>(carret_ - data_.get()) -
                       2 * sizeof(std::uint32_t);
  std::memcpy(data_.get() + sizeof(std::uint32_t), &size, sizeof(size));
  return data_.get();
}

std::uint32_t Messager::size() {
  return static_cast<std::uint32_t>(carret_ - data_.get());
}

} // namespace yai
