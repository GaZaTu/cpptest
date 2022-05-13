#pragma once

#include "./dns.hpp"
#include "./error.hpp"
#include "./stream.hpp"
#ifdef UVPP_TASK_INCLUDE
#include UVPP_TASK_INCLUDE
#endif
#include "uv.h"
#include <functional>

namespace uv {
struct tcp : public stream {
public:
  struct data : public stream::data {
    uv_tcp_t* _native_tcp;
    std::function<void()> tcp_cb;

    data(uv_tcp_t* native_tcp);

    virtual ~data();
  };

  tcp(uv_loop_t* native_loop, uv_tcp_t* native_tcp);

  tcp(uv_loop_t* native_loop);

  tcp(uv_tcp_t* native_tcp);

  tcp();

  operator uv_tcp_t*() noexcept;

  operator const uv_tcp_t*() const noexcept;

  void accept(tcp& client, std::function<void(uv::error)> cb);

#ifdef UVPP_TASK_INCLUDE
  task<void> accept(tcp& client);
#endif

  void nodelay(bool enable);

  void simultaneousAccepts(bool enable);

  void bind(const sockaddr* addr, unsigned int flags = 0);

  void bind4(const char* ip, int port, unsigned int flags = 0);

  void bind6(const char* ip, int port, unsigned int flags = 0);

  std::string sockname();

  std::string peername();

  void connect(uv::dns::addrinfo addr, std::function<void(uv::error)> cb);

#ifdef UVPP_TASK_INCLUDE
  task<void> connect(uv::dns::addrinfo addr);
#endif

  void connect(const std::string& node, const std::string& service, std::function<void(uv::error)> cb);

#ifdef UVPP_TASK_INCLUDE
  task<void> connect(const std::string& node, const std::string& service);
#endif

  void connect(const std::string& node, short port, std::function<void(uv::error)> cb);

#ifdef UVPP_TASK_INCLUDE
  task<void> connect(const std::string& node, short port);
#endif

private:
  uv_tcp_t* _native_tcp;
};
} // namespace uv
