#pragma once

#include "./error.hpp"
#include "./handle.hpp"
#include "uv.h"
#include <functional>

namespace uv {
struct signal : public handle {
public:
  struct data : public handle::data {
    uv_signal_t* _native_signal;
    std::function<void(int)> signal_cb;

    data(uv_signal_t* native_signal);

    virtual ~data();
  };

  signal(uv_loop_t* native_loop, uv_signal_t* native_signal);

  signal(uv_loop_t* native_loop);

  signal(uv_signal_t* native_signal);

  signal();

  signal(signal&& source) noexcept;

  operator uv_signal_t*() noexcept;

  operator const uv_signal_t*() const noexcept;

  void start(int signum, std::function<void(int)> signal_cb);

  void once(int signum, std::function<void(int)> signal_cb);

#ifdef UVPP_TASK_INCLUDE
  task<int> once(int signum);
#endif

  static void sonce(int signum, std::function<void(int)> cb);

#ifdef UVPP_TASK_INCLUDE
  static task<int> sonce(int signum);
#endif

  void stop();

  void trigger(int signum);

private:
  uv_signal_t* _native_signal;
};

template <int32_t SIGNUM = 0>
struct signalof : public signal {
public:
  using signal::signal;

  void start(std::function<void(int)> signal_cb) {
    signal::start(SIGNUM, signal_cb);
  }

  void once(std::function<void(int)> signal_cb) {
    signal::once(SIGNUM, signal_cb);
  }

#ifdef UVPP_TASK_INCLUDE
  task<int> once() {
    return signal::once(SIGNUM);
  }
#endif

  static void sonce(std::function<void(int)> cb) {
    signal::sonce(SIGNUM, cb);
  }

#ifdef UVPP_TASK_INCLUDE
  static task<int> sonce() {
    return signal::sonce(SIGNUM);
  }
#endif

  void trigger() {
    signal::trigger(SIGNUM);
  }

  void trigger(int signum) {
    signal::trigger(signum);
  }

  void cancel() {
    signal::trigger(0);
  }
};

using cancellation = signalof<0>;
} // namespace uv
