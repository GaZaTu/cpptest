#pragma once

#include "uv.h"

namespace uv {
inline int run() {
  return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
} // namespace uv
