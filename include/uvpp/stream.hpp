#pragma once

#include "./error.hpp"
#include "./handle.hpp"
#include "./req.hpp"
#ifdef UVPP_TASK_INCLUDE
#include UVPP_TASK_INCLUDE
#endif
#ifdef UVPP_SSL_INCLUDE
#include UVPP_SSL_INCLUDE
#endif
#include "uv.h"
#include <functional>
#include <sstream>
#include <string>
#include <string_view>

namespace uv {
struct stream : public handle {
public:
  struct data : public handle::data {
    bool sent_eof = false;

    std::function<void(uv::error)> connection_cb;
    std::function<void(std::string_view, uv::error)> read_cb;
#ifdef UVPP_SSL_INCLUDE
    std::function<void(std::string_view, uv::error)> read_decrypted_cb;
#endif
  };

  stream(uv_stream_t* native_stream, data* data_ptr);

  template <typename T>
  stream(T* native_stream, data* data_ptr) : stream(reinterpret_cast<uv_stream_t*>(native_stream), data_ptr) {
  }

  stream() = delete;

  stream(const stream&) = delete;

  virtual ~stream() noexcept;

  operator uv_stream_t*() noexcept;

  operator const uv_stream_t*() const noexcept;

  virtual void close(std::function<void()> close_cb) noexcept override;

  void shutdown(std::function<void(uv::error)> cb);

#ifdef UVPP_TASK_INCLUDE
  task<void> shutdown();
#endif

  void listen(std::function<void(uv::error)> cb, int backlog = 128);

  void accept(stream& client);

#ifdef UVPP_SSL_INCLUDE
  void readStart(std::function<void(std::string_view, uv::error)> cb, bool encrypted = true);
#else
  void readStart(std::function<void(std::string_view, uv::error)> cb);
#endif

#ifdef UVPP_TASK_INCLUDE
  task<void> readStartUntilEOF(std::function<void(std::string_view)> cb);
#endif

  void readStop();

  void readPause();

  void readLines(std::function<void(std::string&&, uv::error)> cb);

#ifdef UVPP_TASK_INCLUDE
  task<void> readLinesUntilEOF(std::function<void(std::string&&)> cb);

  // task<void> readLinesUntilEOF(std::function<task<void>(std::string&&)> cb) {
  //   return readLinesUntilEOF([this, cb{std::move(cb)}](auto&& line) {
  //     cb(std::move(line)).start();
  //   });
  // }
#endif

  void readLinesAsViews(std::function<void(std::string_view, uv::error)> cb);

#ifdef UVPP_TASK_INCLUDE
  task<void> readLinesAsViewsUntilEOF(std::function<void(std::string_view)> cb);
#endif

  void readAll(std::function<void(std::string&&, uv::error)> cb);

#ifdef UVPP_TASK_INCLUDE
  task<std::string> readAll();
#endif

#ifdef UVPP_SSL_INCLUDE
  void write(std::string&& input, std::function<void(uv::error)> cb, bool encrypted = true);
#else
  void write(std::string&& input, std::function<void(uv::error)> cb);
#endif

#ifdef UVPP_SSL_INCLUDE
  void write(std::string_view input, std::function<void(uv::error)> cb, bool encrypted = true);
#else
  void write(std::string_view input, std::function<void(uv::error)> cb);
#endif

#ifdef UVPP_TASK_INCLUDE
  task<void> write(std::string&& input);

  task<void> write(std::string_view input);
#endif

  bool isReadable() const noexcept;

  bool isWritable() const noexcept;

#ifdef UVPP_SSL_INCLUDE
  void useSSL(ssl::context& ssl_context);

  void handshake(std::function<void(uv::error)> cb);

#ifdef UVPP_TASK_INCLUDE
  task<void> handshake();
#endif

  ssl::state& sslState();
#endif

protected:
#ifdef UVPP_SSL_INCLUDE
  ssl::context* _ssl_context = nullptr;
  ssl::state _ssl_state;
#endif

private:
  uv_stream_t* _native_stream;
};
} // namespace uv
