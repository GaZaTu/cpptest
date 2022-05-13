#pragma once

#ifdef HTTPPP_HTTP2
#include "./common.hpp"
#include <cstring>
#include <functional>
#include <nghttp2/nghttp2.h>
#ifdef HTTPPP_TASK_INCLUDE
#include HTTPPP_TASK_INCLUDE
#endif

namespace http::_2 {
// either http::request or http::response
template <typename T>
struct handler {
public:
  handler() {
    nghttp2_session_callbacks_new(&_callbacks);

    nghttp2_session_callbacks_set_send_callback(
        _callbacks, [](nghttp2_session* session, const uint8_t* data, size_t length, int flags, void* user_data) {
          std::string_view input{(const char*)data, length};

          auto handler = (http::_2::handler<T>*)user_data;
          handler->_on_send(input);

          return (ssize_t)length;
        });

    nghttp2_session_callbacks_set_on_begin_headers_callback(_callbacks,
        [](nghttp2_session *session, const nghttp2_frame *frame, void *user_data) {
          auto handler = (http::_2::handler<T>*)user_data;

          nghttp2_session_set_stream_user_data(handler->_session, frame->hd.stream_id, (void*)1);

          if (frame->hd.type != NGHTTP2_HEADERS) {
            return 0;
          }

          T result;
          result.version = {2, 0};

          if constexpr (type == HTTP_REQUEST) {
            result.url.schema("https");
          } else {
            result.status = (http_status)-1;
          }

          handler->_result.emplace(frame->hd.stream_id, std::move(result));

          return 0;
        });

    nghttp2_session_callbacks_set_on_header_callback(_callbacks,
        [](nghttp2_session* session, const nghttp2_frame* frame, const uint8_t* name, size_t namelen,
            const uint8_t* value, size_t valuelen, uint8_t flags, void* user_data) {
          if (frame->hd.type != NGHTTP2_HEADERS) {
            return 0;
          }

          std::string_view header_name{(const char*)name, namelen};
          std::string_view header_value{(const char*)value, valuelen};

          auto handler = (http::_2::handler<T>*)user_data;
          auto& result = handler->_result[frame->hd.stream_id];

          if constexpr (type == HTTP_REQUEST) {
            if (header_name == ":method") {
              result.method = http::method{header_value};
              return 0;
            }
            if (header_name == ":scheme") {
              result.url.schema(header_value);
              return 0;
            }
            if (header_name == ":authority") {
              result.url.host(header_value);
              return 0;
            }
            if (header_name == ":path") {
              http::url url{header_value, http::url::IN};
              url.schema(result.url.schema());
              url.host(result.url.host());

              result.url = std::move(url);
              return 0;
            }

            result.headers[(std::string)header_name] = (std::string)header_value;
          } else {
            if (header_name == ":status") {
              result.status = (http_status)std::stoi((std::string)header_value);
              return 0;
            }

            result.headers[(std::string)header_name] = (std::string)header_value;
          }

          return 0;
        });

    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(_callbacks,
        [](nghttp2_session* session, uint8_t flags, int32_t stream_id, const uint8_t* data, size_t len,
            void* user_data) {
          std::string_view chunk{(const char*)data, len};

          auto handler = (http::_2::handler<T>*)user_data;
          handler->_result[stream_id].body += chunk;

          return 0;
        });

    nghttp2_session_callbacks_set_on_stream_close_callback(
        _callbacks, [](nghttp2_session* session, int32_t stream_id, uint32_t error_code, void* user_data) {
          auto handler = (http::_2::handler<T>*)user_data;

          nghttp2_session_set_stream_user_data(handler->_session, stream_id, (void*)0);

          if constexpr (type == HTTP_RESPONSE) {
            nghttp2_session_terminate_session(session, NGHTTP2_NO_ERROR);
            handler->onStreamClose(stream_id);
          }

          return 0;
        });

    nghttp2_session_callbacks_set_on_frame_recv_callback(
        _callbacks, [](nghttp2_session* session, const nghttp2_frame* frame, void* user_data) {
          auto handler = (http::_2::handler<T>*)user_data;

          switch (frame->hd.type) {
          case NGHTTP2_DATA:
          case NGHTTP2_HEADERS:
            if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
              if (nghttp2_session_get_stream_user_data(session, frame->hd.stream_id) == 0) {
                return 0;
              }

              return handler->onStreamClose(frame->hd.stream_id);
            }
            break;
          }

          return 0;
        });

    nghttp2_session_callbacks_set_on_frame_send_callback(
        _callbacks, [](nghttp2_session* session, const nghttp2_frame* frame, void* user_data) {
          auto handler = (http::_2::handler<T>*)user_data;

          switch (frame->hd.type) {
          case NGHTTP2_DATA:
          case NGHTTP2_HEADERS:
            if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
              return 0;
            }
            break;
          }

          return 0;
        });

    if constexpr (type == HTTP_REQUEST) {
      nghttp2_session_server_new(&_session, _callbacks, this);
    } else {
      nghttp2_session_client_new(&_session, _callbacks, this);
    }
  }

  ~handler() {
    nghttp2_session_del(_session);
    nghttp2_session_callbacks_del(_callbacks);
  }

  handler(const handler&) = delete;

  handler(handler&&) = delete;

  handler& operator=(const handler&) = delete;

  handler& operator=(handler&&) = delete;

  void execute(std::string_view chunk) {
    int rv = nghttp2_session_mem_recv(_session, (const uint8_t*)chunk.data(), chunk.length());
    if (rv < 0) {
      throw http::error{nghttp2_strerror(rv)};
    }

    if (rv < chunk.length()) {
      throw http::error{"unexpected"};
    }

    sendSession();
  }

  void onStreamEnd(std::function<void(int32_t, T&&)> on_stream_end) {
    _on_stream_end = std::move(on_stream_end);
  }

  void onComplete(std::function<void()> on_complete) {
    _on_complete = std::move(on_complete);
  }

#ifdef HTTPPP_TASK_INCLUDE
  HTTPPP_TASK_TYPE<void> onComplete() {
    return HTTPPP_TASK_CREATE<void>([this](auto& resolve, auto&) {
      onComplete(resolve);
    });
  }
#endif

  void close() {
    if (_on_complete) {
      _on_complete();
      _on_complete = nullptr;
    }
  }

  auto& result() {
    return _result;
  }

  operator bool() const {
    return _done;
  }

  void onSend(std::function<void(std::string_view)> on_send) {
    _on_send = std::move(on_send);
  }

  void submitSettings() {
    int rv = nghttp2_submit_settings(_session, NGHTTP2_FLAG_NONE, nullptr, 0);
    if (rv != 0) {
      throw http::error{nghttp2_strerror(rv)};
    }
  }

  void submitRequest(const http::request& request, std::function<void(int32_t)> on_send) {
    std::string method = (std::string)request.method;
    std::string scheme = request.url.schema();
    std::string authority = request.url.host();
    std::string path = request.url.fullpath();

    std::vector<nghttp2_nv> headers;
    headers.push_back(makeNV(":method", method));
    headers.push_back(makeNV(":scheme", scheme));
    headers.push_back(makeNV(":authority", authority));
    headers.push_back(makeNV(":path", path));
    for (const auto& [name, value] : request.headers) {
      headers.push_back(makeNV(name, value));
    }

    nghttp2_data_provider data_provider;
    nghttp2_data_provider* data_provider_ptr = nullptr;
    if (!request.body.empty()) {
      data_provider_ptr = &data_provider;

      struct read_context {
        const http::request* request;
        uint64_t offset;
        std::function<void(int32_t)> on_send;
      };
      auto context = new read_context();
      context->request = &request;
      context->offset = 0;
      context->on_send = std::move(on_send);

      data_provider.source.ptr = (void*)context;
      data_provider.read_callback = [](nghttp2_session* session, int32_t stream_id, uint8_t* buf, size_t length,
                                        uint32_t* data_flags, nghttp2_data_source* source, void* user_data) -> ssize_t {
        auto context = (read_context*)source->ptr;
        const auto& request = *context->request;

        auto& offset = context->offset;
        size_t copy_length = std::min(request.body.length(), length);
        memcpy(buf, request.body.data() + offset, copy_length);
        offset += copy_length;

        if (request.body.length() == offset) {
          auto on_send = std::move(context->on_send);
          *data_flags |= NGHTTP2_DATA_FLAG_EOF;
          delete context;
          on_send(stream_id);
        }

        return copy_length;
      };
    }

    int id = nghttp2_submit_request(_session, 0, headers.data(), headers.size(), data_provider_ptr, this);
    if (id <= 0) {
      throw http::error{nghttp2_strerror(id)};
    }

    if (!data_provider_ptr) {
      on_send(id);
    }

    // return id;
  }

#ifdef HTTPPP_TASK_INCLUDE
  HTTPPP_TASK_TYPE<int32_t> submitRequest(const http::request& request) {
    return HTTPPP_TASK_CREATE<int32_t>([this, &request](auto& resolve, auto&) {
      submitRequest(request, std::move(resolve));
    });
  }
#endif

  void submitResponse(int32_t stream_id, const http::response& response, std::function<void()> on_send) {
    std::string status = std::to_string(response.status);

    std::vector<nghttp2_nv> headers;
    headers.push_back(makeNV(":status", status));
    for (const auto& [name, value] : response.headers) {
      headers.push_back(makeNV(name, value));
    }

    nghttp2_data_provider data_provider;
    nghttp2_data_provider* data_provider_ptr = nullptr;
    if (!response.body.empty()) {
      data_provider_ptr = &data_provider;

      struct read_context {
        const http::response* response;
        uint64_t offset;
        std::function<void()> on_send;
      };
      auto context = new read_context();
      context->response = &response;
      context->offset = 0;
      context->on_send = std::move(on_send);

      data_provider.source.ptr = (void*)context;
      data_provider.read_callback = [](nghttp2_session* session, int32_t stream_id, uint8_t* buf, size_t length,
                                        uint32_t* data_flags, nghttp2_data_source* source, void* user_data) -> ssize_t {
        auto context = (read_context*)source->ptr;
        const auto& response = *context->response;

        auto& offset = context->offset;
        size_t copy_length = std::min(response.body.length(), length);
        memcpy(buf, response.body.data() + offset, copy_length);
        offset += copy_length;

        if (response.body.length() == offset) {
          auto on_send = std::move(context->on_send);
          *data_flags |= NGHTTP2_DATA_FLAG_EOF;
          delete context;
          on_send();
        }

        return copy_length;
      };
    }

    int rv = nghttp2_submit_response(_session, stream_id, headers.data(), headers.size(), data_provider_ptr);
    if (rv != 0) {
      throw http::error{nghttp2_strerror(rv)};
    }

    if (!data_provider_ptr) {
      on_send();
    }
  }

#ifdef HTTPPP_TASK_INCLUDE
  HTTPPP_TASK_TYPE<void> submitResponse(int32_t stream_id, const http::response& response) {
    return HTTPPP_TASK_CREATE<void>([this, stream_id, &response](auto& resolve, auto&) {
      submitResponse(stream_id, response, std::move(resolve));
    });
  }
#endif

  void sendSession() {
    int rv = nghttp2_session_send(_session);
    if (rv != 0) {
      throw http::error{nghttp2_strerror(rv)};
    }
  }

private:
  static constexpr http_parser_type type = std::is_same_v<T, http::request> ? HTTP_REQUEST : HTTP_RESPONSE;

  struct stream_data {
    T result;
  };

  nghttp2_session_callbacks* _callbacks;
  nghttp2_session* _session;

  bool _done = false;
  std::function<void()> _on_complete;

  std::unordered_map<int32_t, T> _result;

  std::function<void(std::string_view)> _on_send;

  std::function<void(int32_t, T&&)> _on_stream_end;

  int onStreamClose(int32_t stream_id) {
    auto& result = _result[stream_id];

    if (_on_stream_end) {
      _on_stream_end(stream_id, std::move(result));

      if constexpr (type == HTTP_REQUEST) {
        _result.erase(stream_id);
      }
    }

    if (nghttp2_session_want_read(_session) == 0 &&
        nghttp2_session_want_write(_session) == 0) {
      _on_complete();
    }

    return 0;
  }

  nghttp2_nv makeNV(std::string_view name, std::string_view value) {
    return {(uint8_t*)name.data(), (uint8_t*)value.data(), name.length(), value.length(), NGHTTP2_NV_FLAG_NONE};
  }
};
} // namespace http2
#endif
