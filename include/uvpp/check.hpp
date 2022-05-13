#pragma once

#include "./error.hpp"
#include "./handle.hpp"
#include "uv.h"
#include <functional>

namespace uv {
struct check : public handle {
public:
  struct data : public handle::data {
    uv_check_t* _native_check;
    std::function<void()> check_cb;

    data(uv_check_t* native_check);

    virtual ~data();
  };

  check(uv_loop_t* native_loop, uv_check_t* native_check);

  check(uv_loop_t* native_loop);

  check(uv_check_t* native_check);

  check();

  check(check&& source) noexcept;

  operator uv_check_t*() noexcept;

  operator const uv_check_t*() const noexcept;

  void start(std::function<void()> check_cb);

  void stop();

private:
  uv_check_t* _native_check;
};
} // namespace uv
