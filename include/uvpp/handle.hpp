#pragma once

#ifdef UVPP_TASK_INCLUDE
#include UVPP_TASK_INCLUDE
#endif
#include "uv.h"
#include <functional>

namespace uv {
struct handle {
public:
  struct data {
    std::function<void()> close_cb;

    virtual ~data();
  };

  handle(uv_handle_t* native_handle, data* data_ptr);

  template <typename T>
  handle(T* native_handle, data* data_ptr) : handle(reinterpret_cast<uv_handle_t*>(native_handle), data_ptr) {
  }

  handle() = delete;

  handle(const handle&) = delete;

  virtual ~handle() noexcept;

  operator uv_handle_t*() noexcept;

  operator const uv_handle_t*() const noexcept;

  bool isActive() const noexcept;

  bool isClosing() const noexcept;

  virtual void close(std::function<void()> close_cb) noexcept;

#ifdef UVPP_TASK_INCLUDE
  task<void> close();
#endif

protected:
  template <typename R, typename T>
  static R* getData(const T* native_handle) {
#if (UV_VERSION_MAJOR >= 1) && (UV_VERSION_MINOR >= 34)
    return reinterpret_cast<R*>(uv_handle_get_data(reinterpret_cast<const uv_handle_t*>(native_handle)));
#else
    return reinterpret_cast<R*>(reinterpret_cast<const uv_req_t*>(native_handle)->data);
#endif
  }

  template <typename R>
  R* getData() {
    return handle::getData<R>(_native_handle);
  }

private:
  uv_handle_t* _native_handle;
};
} // namespace uv
