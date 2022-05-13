#pragma once

#include "./error.hpp"
#include "./handle.hpp"
#ifdef UVPP_TASK_INCLUDE
#include UVPP_TASK_INCLUDE
#endif
#include "uv.h"
#include <functional>

namespace uv {
struct timer : public handle {
public:
  struct data : public handle::data {
    uv_timer_t* _native_timer;
    std::function<void()> timer_cb;

    data(uv_timer_t* native_timer);

    virtual ~data();
  };

  timer(uv_loop_t* native_loop, uv_timer_t* native_timer);

  timer(uv_loop_t* native_loop);

  timer(uv_timer_t* native_timer);

  timer();

  timer(timer&& source) noexcept;

  operator uv_timer_t*() noexcept;

  operator const uv_timer_t*() const noexcept;

  void start(std::function<void()> timer_cb, uint64_t timeout, uint64_t repeat = 0);

  void startOnce(uint64_t timeout, std::function<void()> cb);

#ifdef UVPP_TASK_INCLUDE
  task<void> startOnce(uint64_t timeout);
#endif

  void stop();

  void again();

  void repeat(uint64_t repeat) noexcept;

  uint64_t repeat() const noexcept;

private:
  uv_timer_t* _native_timer;
};

void timeout(uint64_t timeout, std::function<void()> cb);

#ifdef UVPP_TASK_INCLUDE
task<void> timeout(uint64_t timeout);
#endif
} // namespace uv
