#include "uvpp/signal.hpp"
#include "uvpp/async.hpp"

namespace uv {
signal::data::data(uv_signal_t* native_signal) : _native_signal(native_signal) {
}

signal::data::~data() {
  delete _native_signal;
}

signal::signal(uv_loop_t* native_loop, uv_signal_t* native_signal)
    : handle(native_signal, new data(native_signal)), _native_signal(native_signal) {
  error::test(uv_signal_init(native_loop, native_signal));
}

signal::signal(uv_loop_t* native_loop) : signal(native_loop, new uv_signal_t()) {
}

signal::signal(uv_signal_t* native_signal) : signal(uv_default_loop(), native_signal) {
}

signal::signal() : signal(uv_default_loop(), new uv_signal_t()) {
}

signal::signal(signal&& source) noexcept
    : handle(source._native_signal, handle::getData<data>(source._native_signal)),
      _native_signal(std::exchange(source._native_signal, nullptr)) {
}

signal::operator uv_signal_t*() noexcept {
  return _native_signal;
}

signal::operator const uv_signal_t*() const noexcept {
  return _native_signal;
}

void signal::start(int signum, std::function<void(int)> signal_cb) {
  data* data_ptr = getData<data>();
  data_ptr->signal_cb = signal_cb;

  if (!signum) {
    return;
  }

  error::test(uv_signal_start(*this, [](uv_signal_t* native_signal, int signum) {
    data* data_ptr = handle::getData<data>(native_signal);
    data_ptr->signal_cb(signum);
  }, signum));
}

void signal::once(int signum, std::function<void(int)> signal_cb) {
  data* data_ptr = getData<data>();
  data_ptr->signal_cb = signal_cb;

  if (!signum) {
    return;
  }

  error::test(uv_signal_start_oneshot(*this, [](uv_signal_t* native_signal, int signum) {
    data* data_ptr = handle::getData<data>(native_signal);
    data_ptr->signal_cb(signum);
  }, signum));
}

#ifdef UVPP_TASK_INCLUDE
task<int> signal::once(int signum) {
  return task<int>::create([this, signum](auto& resolve, auto& reject) {
    once(signum, resolve);
  });
}
#endif

void signal::sonce(int signum, std::function<void(int)> cb) {
  uv::signal* signal = new uv::signal();
  signal->once(signum, [signal, _cb{std::move(cb)}](int signum) {
    auto cb = std::move(_cb);
    delete signal;
    cb(signum);
  });
}

#ifdef UVPP_TASK_INCLUDE
task<int> signal::sonce(int signum) {
  return task<int>::create([signum](auto& resolve, auto& reject) {
    sonce(signum, resolve);
  });
}
#endif

void signal::stop() {
  uv_signal_stop(*this);
}

void signal::trigger(int signum) {
  data* data_ptr = getData<data>();
  uv::async::ssend([=]() {
    data_ptr->signal_cb(signum);
  });
}
} // namespace uv
