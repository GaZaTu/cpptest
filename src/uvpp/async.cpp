#include "uvpp/async.hpp"

namespace uv {
async::data::data(uv_async_t* native_async) : _native_async(native_async) {
}

async::data::~data() {
  delete _native_async;
}

async::async(uv_loop_t* native_loop, uv_async_t* native_async)
    : handle(native_async, new data(native_async)), _native_async(native_async) {
  error::test(uv_async_init(native_loop, native_async, [](uv_async_t* native_async) {
    data* data_ptr = handle::getData<data>(native_async);
    data_ptr->async_cb();
  }));
}

async::async(uv_loop_t* native_loop) : async(native_loop, new uv_async_t()) {
}

async::async(uv_async_t* native_async) : async(uv_default_loop(), native_async) {
}

async::async() : async(uv_default_loop(), new uv_async_t()) {
}

async::async(async&& source) noexcept
    : handle(source._native_async, handle::getData<data>(source._native_async)),
      _native_async(std::exchange(source._native_async, nullptr)) {
}

async::operator uv_async_t*() noexcept {
  return _native_async;
}

async::operator const uv_async_t*() const noexcept {
  return _native_async;
}

void async::send(std::function<void()> async_cb) {
  data* data_ptr = getData<data>();
  data_ptr->async_cb = async_cb;

  error::test(uv_async_send(*this));
}

#ifdef UVPP_TASK_INCLUDE
task<void> async::send() {
  return task<void>::create([this](auto& resolve, auto& reject) {
    send(resolve);
  });
}
#endif

void async::ssend(std::function<void()> cb) {
  uv::async* async = new uv::async();
  async->send([async, _cb{std::move(cb)}]() {
    auto cb = std::move(_cb);
    delete async;
    cb();
  });
}

#ifdef UVPP_TASK_INCLUDE
task<void> async::ssend() {
  return task<void>::create([](auto& resolve, auto& reject) {
    ssend(resolve);
  });
}
#endif
} // namespace uv
