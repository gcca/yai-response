#include <boost/asio/awaitable.hpp>
#include <cstdint>

#include "utils.hpp"

#define YAI_PROTO(T)                                                           \
public:                                                                        \
  virtual ~T();                                                                \
                                                                               \
protected:                                                                     \
  T(const T &) = delete;                                                       \
  T(const T &&) = delete;                                                      \
  T &operator=(const T &) = delete;                                            \
  T &operator=(const T &&) = delete;                                           \
  T() = default;

namespace yai {

template <class T> using Awaitable = boost::asio::awaitable<T>;

class Stream {
  YAI_PROTO(Stream)
public:
  [[nodiscard]]
  virtual Awaitable<std::size_t> Write(const void *data, std::size_t size) = 0;

  [[nodiscard]]
  virtual Awaitable<std::size_t> Read(char *data, std::size_t size) = 0;
};

typedef Awaitable<void> (*Handler)(Stream &);

class Server {
public:
  explicit Server(std::uint16_t port, Handler *handlers, std::size_t size);

  template <std::size_t size>
  explicit Server(std::uint16_t port, Handler (&handlers)[size])
      : Server(port, handlers, size) {}

  void Run();

private:
  std::uint16_t port_;
  Handler *handlers_;
  std::size_t size_;
};

class Messager {
public:
  explicit Messager(std::size_t reserve);

  void Status(std::uint32_t status);

  void AppendLength(std::uint32_t length);

  void AppendStr(const char *str);

  void *Flush();

  std::uint32_t size();

  void AppendNarrow(int n) { AppendLength(static_cast<std::uint32_t>(n)); }

  void AppendNarrow(std::uint32_t n) { AppendLength(n); }

  void AppendNarrow(const char *s) { AppendStr(s); }

  template <class... Errors, std::size_t reserve = 128>
  static Messager MakeErrors(Errors &&...errors) {
    Messager messager{sizeof...(errors) * reserve};
    messager.Status(1);
    messager.AppendLength(sizeof...(errors));

    ((messager.AppendLength(static_cast<std::uint32_t>(std::strlen(errors))),
      messager.AppendStr(errors)),
     ...);

    return messager;
  }

private:
  std::unique_ptr<std::uint8_t[]> data_;
  std::uint8_t *carret_;
};

} // namespace yai
