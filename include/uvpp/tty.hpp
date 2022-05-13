#pragma once

#include "./error.hpp"
#include "./stream.hpp"
#include "uv.h"
#include <functional>

namespace uv {
struct tty : public stream {
public:
  enum file {
    STDIN = 0,
    STDOUT = 1,
    STDERR = 2,
  };

  struct data : public stream::data {
    uv_tty_t* _native_tty;

    data(uv_tty_t* native_tty);

    virtual ~data();
  };

  tty(file fd, uv_loop_t* native_loop, uv_tty_t* native_tty);

  tty(file fd, uv_loop_t* native_loop);

  tty(file fd, uv_tty_t* native_tty);

  tty(file fd);

  tty(tty&& source) noexcept;

  operator uv_tty_t*() noexcept;

  operator const uv_tty_t*() const noexcept;

private:
  uv_tty_t* _native_tty;
};
} // namespace uv
