#pragma once

#include "./error.hpp"
#include "./handle.hpp"
#include "uv.h"
#include <functional>

namespace uv {
struct async : public handle {
public:
  struct data : public handle::data {
    uv_async_t* _native_async;
    std::function<void()> async_cb;

    data(uv_async_t* native_async);

    virtual ~data();
  };

  async(uv_loop_t* native_loop, uv_async_t* native_async);

  async(uv_loop_t* native_loop);

  async(uv_async_t* native_async);

  async();

  async(async&& source) noexcept;

  operator uv_async_t*() noexcept;

  operator const uv_async_t*() const noexcept;

  void send(std::function<void()> async_cb);

#ifdef UVPP_TASK_INCLUDE
  task<void> send();
#endif

  static void ssend(std::function<void()> cb);

#ifdef UVPP_TASK_INCLUDE
  static task<void> ssend();
#endif

private:
  uv_async_t* _native_async;
};
} // namespace uv
