#pragma once

#include "uv.h"

namespace uv {
inline uint64_t hrtime() {
  return uv_hrtime();
}
} // namespace uv
