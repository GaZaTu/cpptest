#pragma once

#include "./error.hpp"
#include "./req.hpp"
#ifdef UVPP_TASK_INCLUDE
#include UVPP_TASK_INCLUDE
#endif
#include "uv.h"
#include <functional>
#include <string_view>

namespace uv {
struct file {
public:
  file(uv_file fd = 0, uv_loop_t* native_loop = uv_default_loop());

  ~file();

  file(const file&) = delete;

  file(file&& o);

  file& operator=(const file&) = delete;

  file& operator=(file&& o);

  file& operator=(uv_file fd);

  operator uv_file() const;

  operator bool() const;

private:
  uv_file _fd = 0;
  uv_loop_t* _native_loop = nullptr;
};

namespace fs {
using buf = uv_buf_t;

void close(uv::file& file, std::function<void()> cb, uv_loop_t* native_loop = uv_default_loop());

#ifdef UVPP_TASK_INCLUDE
task<void> close(uv::file& file, uv_loop_t* native_loop = uv_default_loop());
#endif

void open(std::string_view path, int flags, int mode, std::function<void(uv::file, uv::error)> cb,
    uv_loop_t* native_loop = uv_default_loop());

#ifdef UVPP_TASK_INCLUDE
task<uv::file> open(std::string_view path, int flags, int mode, uv_loop_t* native_loop = uv_default_loop());
#endif

void read(uv_file file, std::function<void(std::string_view, uv::error)> cb, char* buf = nullptr, size_t buf_len = 0,
    int64_t offset = 0, uv_loop_t* native_loop = uv_default_loop());

#ifdef UVPP_TASK_INCLUDE
task<std::string_view> read(uv_file file, char* buf = nullptr, size_t buf_len = 0, int64_t offset = 0,
    uv_loop_t* native_loop = uv_default_loop());
#endif

#ifdef UVPP_TASK_INCLUDE
task<std::string> readAll(uv_file file, int64_t offset = 0, uv_loop_t* native_loop = uv_default_loop());
#endif

#ifdef UVPP_TASK_INCLUDE
task<std::string> readAll(std::string_view path, int64_t offset = 0, uv_loop_t* native_loop = uv_default_loop());
#endif
} // namespace fs
} // namespace uv
