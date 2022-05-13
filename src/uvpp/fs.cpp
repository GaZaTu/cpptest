#include "uvpp/fs.hpp"

namespace uv {
uv::file::file(uv_file fd, uv_loop_t* native_loop) : _fd(fd), _native_loop(native_loop) {}


uv::file::~file() {
  if (*this) {
    uv::fs::close(*this, []() {}, _native_loop);
  }
}

uv::file::file(file&& o) {
  *this = std::move(o);
};

uv::file& uv::file::operator=(uv::file&& o) {
  std::swap(_fd, o._fd);
  std::swap(_native_loop, o._native_loop);
  return *this;
};

uv::file& uv::file::operator=(uv_file fd) {
  _fd = fd;
  return *this;
}

uv::file::operator uv_file() const {
  return _fd;
}

uv::file::operator bool() const {
  return _fd != 0;
}

namespace fs {
void close(uv::file& file, std::function<void()> cb, uv_loop_t* native_loop) {
  struct data_t : public uv::detail::req::data {
    std::function<void()> cb;
  };
  using req_t = uv::req<uv_fs_t, data_t>;

  auto req = new req_t(&uv_fs_req_cleanup);
  auto data = req->dataPtr();
  data->cb = cb;

  error::test(uv_fs_close(native_loop, *req, file, [](uv_fs_t* req) {
    auto data = req_t::dataPtr(req);
    auto cb = std::move(data->cb);
    delete data->req;

    cb();
  }));

  file = 0;
}

#ifdef UVPP_TASK_INCLUDE
task<void> close(uv::file& file, uv_loop_t* native_loop) {
  return task<void>::create([&file, native_loop](auto& resolve, auto& reject) mutable {
    uv::fs::close(
        file,
        [&resolve, &reject]() {
          resolve();
        },
        native_loop);
  });
}
#endif

void open(std::string_view path, int flags, int mode, std::function<void(uv::file, uv::error)> cb,
    uv_loop_t* native_loop) {
  struct data_t : public uv::detail::req::data {
    std::function<void(uv::file, uv::error)> cb;
    std::string path;
  };
  using req_t = uv::req<uv_fs_t, data_t>;

  auto req = new req_t(&uv_fs_req_cleanup);
  auto data = req->dataPtr();
  data->cb = cb;
  data->path = path;

  error::test(uv_fs_open(native_loop, *req, data->path.data(), flags, mode, [](uv_fs_t* req) {
    auto data = req_t::dataPtr(req);
    auto cb = std::move(data->cb);
    delete data->req;

    if (req->result < 0) {
      cb(0, uv::error{(int)req->result});
    } else {
      cb(req->result, uv::error{0});
    }
  }));
}

#ifdef UVPP_TASK_INCLUDE
task<uv::file> open(std::string_view path, int flags, int mode, uv_loop_t* native_loop) {
  return task<uv::file>::create([path, flags, mode, native_loop](auto& resolve, auto& reject) {
    uv::fs::open(
        path, flags, mode,
        [&resolve, &reject](auto result, auto error) {
          if (error) {
            reject(make_exception_ptr(error));
          } else {
            resolve(result);
          }
        },
        native_loop);
  });
}
#endif

void read(uv_file file, std::function<void(std::string_view, uv::error)> cb, char* buf, size_t buf_len,
    int64_t offset, uv_loop_t* native_loop) {
  struct data_t : public uv::detail::req::data {
    bool owns_buf;
    uv::fs::buf buf;
    std::function<void(std::string_view, uv::error)> cb;
  };
  using req_t = uv::req<uv_fs_t, data_t>;

  bool owns_buf = buf == nullptr;
  if (owns_buf) {
    if (buf_len == 0) {
      buf_len = 65536;
    }

    buf = new char[buf_len];
  }

  auto req = new req_t(&uv_fs_req_cleanup);
  auto data = req->dataPtr();
  data->owns_buf = owns_buf;
  data->buf = uv_buf_init(buf, buf_len);
  data->cb = cb;

  error::test(uv_fs_read(native_loop, *req, file, &data->buf, 1, offset, [](uv_fs_t* req) {
    auto data = req_t::dataPtr(req);
    auto cb = std::move(data->cb);
    auto owns_buf = data->owns_buf;
    auto buf = data->buf;
    auto result = req->result;
    delete data->req;

    if (result < 0) {
      cb(std::string_view{nullptr, 0}, uv::error{(int)result});
    } else {
      cb(std::string_view{buf.base, (size_t)result}, uv::error{0});
    }

    if (owns_buf) {
      delete buf.base;
    }
  }));
}

#ifdef UVPP_TASK_INCLUDE
task<std::string_view> read(uv_file file, char* buf, size_t buf_len, int64_t offset,
    uv_loop_t* native_loop) {
  return task<std::string_view>::create([file, buf, buf_len, offset, native_loop](auto& resolve, auto& reject) {
    uv::fs::read(
        file,
        [&resolve, &reject](auto result, auto error) {
          if (error) {
            reject(make_exception_ptr(error));
          } else {
            resolve(result);
          }
        },
        buf, buf_len, offset, native_loop);
  });
}
#endif

#ifdef UVPP_TASK_INCLUDE
task<std::string> readAll(uv_file file, int64_t offset, uv_loop_t* native_loop) {
  std::string result;

  char buf[65536];
  while (true) {
    auto chunk = co_await read(file, buf, sizeof(buf), offset + result.length(), native_loop);

    if (chunk.length() == 0) {
      break;
    }

    result += chunk;
  }

  co_return result;
}
#endif

#ifdef UVPP_TASK_INCLUDE
task<std::string> readAll(std::string_view path, int64_t offset, uv_loop_t* native_loop) {
  uv::file file = co_await uv::fs::open(path, O_RDONLY, S_IRUSR, native_loop);
  // finally f{[file, native_loop]() {
  //   uv::fs::close(file, []() {}, native_loop);
  // }};

  co_return co_await uv::fs::readAll(file, offset, native_loop);
}
#endif
} // namespace fs
} // namespace uv
