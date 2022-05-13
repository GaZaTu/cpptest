#include "uvpp/stream.hpp"

namespace uv {
stream::stream(uv_stream_t* native_stream, data* data_ptr) : handle(native_stream, data_ptr), _native_stream(native_stream) {
}

stream::~stream() noexcept {
  readStop();
}

stream::operator uv_stream_t*() noexcept {
  return _native_stream;
}

stream::operator const uv_stream_t*() const noexcept {
  return _native_stream;
}

void stream::close(std::function<void()> close_cb) noexcept {
  readStop();
  handle::close(close_cb);
}

void stream::shutdown(std::function<void(uv::error)> cb) {
  struct data_t : public uv::detail::req::data {
    std::function<void(uv::error)> cb;
  };
  using req_t = uv::req<uv_shutdown_t, data_t>;

  auto req = new req_t();
  auto data = req->dataPtr();
  data->cb = std::move(cb);

  error::test(uv_shutdown(*req, *this, [](uv_shutdown_t* req, int status) {
    auto data = req_t::dataPtr(req);
    auto cb = std::move(data->cb);
    delete data->req;

    cb(uv::error{status});
  }));
}

#ifdef UVPP_TASK_INCLUDE
task<void> stream::shutdown() {
  return task<void>::create([this](auto& resolve, auto& reject) {
    shutdown([&resolve, &reject](auto error) {
      if (error) {
        reject(std::make_exception_ptr(error));
      } else {
        resolve();
      }
    });
  });
}
#endif

void stream::listen(std::function<void(uv::error)> cb, int backlog) {
  auto data_ptr = getData<data>();
  data_ptr->connection_cb = std::move(cb);

  error::test(uv_listen(*this, backlog, [](uv_stream_t* native_stream, int status) {
    auto data_ptr = handle::getData<data>(native_stream);

    data_ptr->connection_cb(uv::error{status});
  }));
}

void stream::accept(stream& client) {
  error::test(uv_accept(*this, client));
}

#ifdef UVPP_SSL_INCLUDE
void stream::readStart(std::function<void(std::string_view, uv::error)> cb, bool encrypted) {
#else
void stream::readStart(std::function<void(std::string_view, uv::error)> cb) {
#endif
  auto data_ptr = getData<data>();
  data_ptr->sent_eof = false;
#ifdef UVPP_SSL_INCLUDE
  if (_ssl_state && encrypted) {
    data_ptr->read_decrypted_cb = std::move(cb);
    return;
  }
#endif
  data_ptr->read_cb = std::move(cb);

  error::test(uv_read_start(
      *this,
      [](uv_handle_t* native_handle, size_t suggested_size, uv_buf_t* buf) {
        buf->base = new char[suggested_size];
        buf->len = suggested_size;
      },
      [](uv_stream_t* native_stream, ssize_t nread, const uv_buf_t* buf) {
        auto data_ptr = handle::getData<data>(native_stream);

        if (nread < 0) {
          if (nread == UV_EOF) {
            data_ptr->sent_eof = true;
          }

          data_ptr->read_cb(std::string_view{nullptr, 0}, uv::error{(int)nread});
        } else {
          data_ptr->read_cb(std::string_view{buf->base, (std::string_view::size_type)nread}, uv::error{0});
        }

        delete buf->base;
      }));
}

#ifdef UVPP_TASK_INCLUDE
task<void> stream::readStartUntilEOF(std::function<void(std::string_view)> cb) {
  return task<void>::create([this, cb{std::move(cb)}](auto& resolve, auto& reject) {
    readStart([&resolve, &reject, cb{std::move(cb)}](auto chunk, auto error) {
      if (error) {
        if (error == UV_EOF) {
          resolve();
        } else {
          reject(std::make_exception_ptr(error));
        }
      } else {
        try {
          cb(chunk);
        } catch (const std::exception& e) {
          // reject(std::make_exception_ptr(e));
          throw;
        }
      }
    });
  });
}
#endif

void stream::readStop() {
  uv_read_stop(*this);

  auto data_ptr = getData<data>();
  if (!data_ptr->sent_eof) {
    data_ptr->sent_eof = true;

    if (data_ptr->read_cb) {
      data_ptr->read_cb({}, uv::error{UV_EOF});
    }
  }
}

void stream::readPause() {
  auto data_ptr = getData<data>();
  if (!data_ptr->sent_eof) {
    data_ptr->sent_eof = true;

    if (data_ptr->read_cb) {
      data_ptr->read_cb({}, uv::error{UV_EOF});
    }

    struct read_cb_buffer_t {
    public:
      uv::stream::data* data_ptr;
      std::string chunk_buffer;
      uv::error error_buffer;

      ~read_cb_buffer_t() {
        if (!chunk_buffer.empty() || error_buffer) {
          data_ptr->read_cb(chunk_buffer, error_buffer);
        }
      }

      void operator()(std::string_view chunk, uv::error error) {
        if (error) {
          error_buffer = error;
        } else {
          chunk_buffer += chunk;
        }
      }
    };

    data_ptr->read_cb = read_cb_buffer_t{data_ptr};
  }
}

void stream::readLines(std::function<void(std::string&&, uv::error)> cb) {
  struct state_t {
    std::stringstream stream;
  };
  auto state = new state_t();

  readStart([cb{std::move(cb)}, state](auto chunk, auto error) {
    if (error) {
      if (error == UV_EOF) {
        // std::string line;
        // std::getline(state->stream, line);
        // cb(std::move(line), uv::error{0});
      }

      delete state;
      cb({}, error);
    } else {
      state->stream << chunk;

      std::string line;
      while (std::getline(state->stream, line)) {
        if (line.ends_with('\r')) {
          line.resize(line.size() - 1);
        }

        cb(std::move(line), uv::error{0});
      }

      if (state->stream.eof()) {
        state->stream.clear();
        state->stream.str({});
      }
    }
  });
}

#ifdef UVPP_TASK_INCLUDE
task<void> stream::readLinesUntilEOF(std::function<void(std::string&&)> cb) {
  return task<void>::create([this, cb{std::move(cb)}](auto& resolve, auto& reject) {
    readLines([&resolve, &reject, cb{std::move(cb)}](auto&& line, auto error) {
      if (error) {
        if (error == UV_EOF) {
          resolve();
        } else {
          reject(std::make_exception_ptr(error));
        }
      } else {
        try {
          cb(std::move(line));
        } catch (const std::exception& e) {
          // reject(std::make_exception_ptr(e));
          throw;
        }
      }
    });
  });
}

// task<void> readLinesUntilEOF(std::function<task<void>(std::string&&)> cb) {
//   return readLinesUntilEOF([this, cb{std::move(cb)}](auto&& line) {
//     cb(std::move(line)).start();
//   });
// }
#endif

void stream::readLinesAsViews(std::function<void(std::string_view, uv::error)> cb) {
  readStart([cb{std::move(cb)}](auto chunk, auto error) {
    if (error) {
      cb({}, error);
    } else {
      size_t offset = 0;
      size_t length = chunk.length();
      for (size_t i = offset; i < length; i++) {
        if (chunk[i] == '\n') {
          short rm = 0;
          if (chunk[i - 1] == '\r') {
            rm = 1;
          }

          cb(chunk.substr(offset, (i - rm) - offset), uv::error{0});
          offset = i + rm;
        }
      }
    }
  });
}

#ifdef UVPP_TASK_INCLUDE
task<void> stream::readLinesAsViewsUntilEOF(std::function<void(std::string_view)> cb) {
  return task<void>::create([this, cb{std::move(cb)}](auto& resolve, auto& reject) {
    readLinesAsViews([&resolve, &reject, cb{std::move(cb)}](auto line, auto error) {
      if (error) {
        if (error == UV_EOF) {
          resolve();
        } else {
          reject(std::make_exception_ptr(error));
        }
      } else {
        try {
          cb(line);
        } catch (const std::exception& e) {
          // reject(std::make_exception_ptr(e));
          throw;
        }
      }
    });
  });
}
#endif

void stream::readAll(std::function<void(std::string&&, uv::error)> cb) {
  struct state_t {
    std::string result;
  };
  auto state = new state_t();

  readStart([cb{std::move(cb)}, state](auto chunk, auto error) {
    if (error) {
      std::string result = std::move(state->result);
      delete state;

      if (error == UV_EOF) {
        cb(std::move(result), uv::error{0});
      } else {
        cb({}, error);
      }
    } else {
      state->result += chunk;
    }
  });
}

#ifdef UVPP_TASK_INCLUDE
task<std::string> stream::readAll() {
  return task<std::string>::create([this](auto& resolve, auto& reject) {
    readAll([&resolve, &reject](auto&& result, auto error) {
      if (error) {
        reject(std::make_exception_ptr(error));
      } else {
        resolve(result);
      }
    });
  });
}
#endif

#ifdef UVPP_SSL_INCLUDE
void stream::write(std::string&& input, std::function<void(uv::error)> cb, bool encrypted) {
#else
void stream::write(std::string&& input, std::function<void(uv::error)> cb) {
#endif
  auto _write = [this](std::string&& input, std::function<void(uv::error)>& cb) {
    struct data_t : public uv::detail::req::data {
      std::string input;
      std::function<void(uv::error)> cb;
    };
    using req_t = uv::req<uv_write_t, data_t>;

    auto req = new req_t();
    auto data = req->dataPtr();
    data->input = std::move(input);
    data->cb = cb;

    uv_buf_t buf = uv_buf_init(data->input.data(), data->input.length());

    error::test(uv_write(*req, *this, &buf, 1, [](uv_write_t* req, int status) {
      auto data = req_t::dataPtr(req);
      auto cb = std::move(data->cb);
      delete data->req;

      cb(uv::error{status});
    }));
  };

#ifdef UVPP_SSL_INCLUDE
  if (_ssl_state && encrypted) {
    _ssl_state.encrypt(std::move(input), [cb{std::move(cb)}](auto error) {
      if (error) {
        try {
          std::rethrow_exception(error);
        } catch (const uv::error& e) {
          cb(e);
        }
      } else {
        cb(uv::error{0});
      }
    });
  } else {
    _write(std::move(input), cb);
  }
#else
  _write(std::move(input), cb);
#endif
}

#ifdef UVPP_SSL_INCLUDE
void stream::write(std::string_view input, std::function<void(uv::error)> cb, bool encrypted) {
#else
void stream::write(std::string_view input, std::function<void(uv::error)> cb) {
#endif
#ifdef UVPP_SSL_INCLUDE
  write((std::string)input, std::move(cb), encrypted);
#else
  write((std::string)input, std::move(cb));
#endif
}

#ifdef UVPP_TASK_INCLUDE
task<void> stream::write(std::string&& input) {
  return task<void>::create([this, input{std::move(input)}](auto& resolve, auto& reject) mutable {
    write(std::move(input), [&resolve, &reject](auto error) {
      if (error) {
        reject(std::make_exception_ptr(error));
      } else {
        resolve();
      }
    });
  });
}

task<void> stream::write(std::string_view input) {
  return write((std::string)input);
}
#endif

bool stream::isReadable() const noexcept {
  return uv_is_readable(*this) != 0;
}

bool stream::isWritable() const noexcept {
  return uv_is_writable(*this) != 0;
}

#ifdef UVPP_SSL_INCLUDE
void stream::useSSL(ssl::context& ssl_context) {
  _ssl_context = &ssl_context;
  _ssl_state = ssl::state{*_ssl_context};

  _ssl_state.onReadDecrypted([this](auto data) {
    auto data_ptr = getData<uv::stream::data>();

    if (data_ptr->read_decrypted_cb) {
      data_ptr->read_decrypted_cb(data, uv::error{0});
    }
  });

  _ssl_state.onWriteEncrypted([this](auto&& input, auto cb) {
    write(
        std::move(input),
        [cb{std::move(cb)}](auto error) {
          if (error) {
            cb(std::make_exception_ptr(error));
          } else {
            cb(nullptr);
          }
        },
        false);
  });
}

void stream::handshake(std::function<void(uv::error)> cb) {
  _ssl_state.handshake([this, cb]() {
    cb(uv::error{0});
  });

  readStart(
      [this, cb](auto data, auto error) {
        if (error) {
          auto data_ptr = getData<uv::stream::data>();

          if (data_ptr->read_decrypted_cb) {
            data_ptr->read_decrypted_cb(std::string_view{nullptr, 0}, error);
          } else if (cb) {
            cb(error);
          }
        } else {
          _ssl_state.decrypt(data);
        }
      },
      false);
}

#ifdef UVPP_TASK_INCLUDE
task<void> stream::handshake() {
  return task<void>::create([this](auto& resolve, auto& reject) {
    handshake([&resolve, &reject](auto error) {
      if (error) {
        reject(std::make_exception_ptr(error));
      } else {
        resolve();
      }
    });
  });
}
#endif

ssl::state& stream::sslState() {
  return _ssl_state;
}
#endif
} // namespace uv
