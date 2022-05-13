#include "uvpp/handle.hpp"

namespace uv {
handle::data::~data() {
}

handle::handle(uv_handle_t* native_handle, data* data_ptr) : _native_handle(native_handle) {
#if (UV_VERSION_MAJOR >= 1) && (UV_VERSION_MINOR >= 34)
  uv_handle_set_data(native_handle, data_ptr);
#else
  native_handle->data = (void*)data_ptr;
#endif
}

handle::~handle() noexcept {
  data* data_ptr = getData<data>();

  if (isClosing()) {
    delete data_ptr;
  } else {
    close([data_ptr]() {
      delete data_ptr;
    });
  }
}

handle::operator uv_handle_t*() noexcept {
  return _native_handle;
}

handle::operator const uv_handle_t*() const noexcept {
  return _native_handle;
}

bool handle::isActive() const noexcept {
  return uv_is_active(*this) != 0;
}

bool handle::isClosing() const noexcept {
  return uv_is_closing(*this) != 0;
}

void handle::close(std::function<void()> close_cb) noexcept {
  data* data_ptr = getData<data>();
  data_ptr->close_cb = close_cb;

  uv_close(*this, [](uv_handle_t* native_handle) {
    data* data_ptr = handle::getData<data>(native_handle);
    data_ptr->close_cb();
  });
}

#ifdef UVPP_TASK_INCLUDE
task<void> handle::close() {
  return task<void>::create([this](auto& resolve, auto& reject) {
    close([&resolve]() {
      resolve();
    });
  });
}
#endif
} // namespace uv
