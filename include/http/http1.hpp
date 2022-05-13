#pragma once

#include "./common.hpp"
#include <functional>
#ifdef HTTPPP_TASK_INCLUDE
#include HTTPPP_TASK_INCLUDE
#endif

namespace http::_1 {
// either http::request or http::response
template <typename T>
struct parser {
public:
  parser() {
    _result.version = {1, 1};

    if constexpr (type == HTTP_REQUEST) {
      _result.method = (http_method)-1;
      _result.url.schema("http");
    } else {
      _result.status = (http_status)-1;
    }

    http_parser_settings_init(&_settings);
    http_parser_init(&_parser, type);

    _parser.data = (void*)this;

    _settings.on_url = [](http_parser* p, const char* data, size_t len) {
      auto parser = (http::_1::parser<T>*)p->data;

      if constexpr (type == HTTP_REQUEST) {
        parser->_result.url = {std::string_view{data, len}, http::url::IN};
      }

      return 0;
    };

    _settings.on_header_field = [](http_parser* p, const char* data, size_t len) {
      auto parser = (http::_1::parser<T>*)p->data;

      parser->_header = std::string_view{data, len};
      std::transform(std::begin(parser->_header), std::end(parser->_header), std::begin(parser->_header), [](unsigned char c) {
        return std::tolower(c);
      });

      return 0;
    };

    _settings.on_header_value = [](http_parser* p, const char* data, size_t len) {
      auto parser = (http::_1::parser<T>*)p->data;

      parser->_result.headers[parser->_header] = std::string_view{data, len};

      return 0;
    };

    _settings.on_headers_complete = [](http_parser* p) {
      auto parser = (http::_1::parser<T>*)p->data;

      parser->_result.version = {parser->_parser.http_major, parser->_parser.http_minor};

      if constexpr (type == HTTP_REQUEST) {
        parser->_result.method = (http_method)parser->_parser.method;
      } else {
        parser->_result.status = (http_status)parser->_parser.status_code;
      }

      return 0;
    };

    _settings.on_body = [](http_parser* p, const char* data, size_t len) {
      auto parser = (http::_1::parser<T>*)p->data;

      parser->_result.body += std::string_view{data, len};

      return 0;
    };

    _settings.on_message_complete = [](http_parser* p) {
      auto parser = (http::_1::parser<T>*)p->data;

      if constexpr (type == HTTP_REQUEST) {
        parser->_done = parser->_result.method != -1;
      } else {
        parser->_done = parser->_result.status != -1;
      }

      return 0;
    };
  }

  parser(const parser&) = delete;

  parser(parser&&) = delete;

  parser& operator=(const parser&) = delete;

  parser& operator=(parser&&) = delete;

  void execute(std::string_view chunk) {
    try {
      size_t offset = http_parser_execute(&_parser, &_settings, chunk.data(), chunk.length());

      if (_parser.http_errno != 0) {
        if (_parser.http_errno == HPE_INVALID_CONSTANT) {
          return;
        }

        throw http::error{_parser.http_errno};
      }

      if (*this) {
        if (_on_complete) {
          _on_complete(_result);
          _on_complete = nullptr;
          _on_fail = nullptr;
        }
      }
    } catch (...) {
      if (_on_fail) {
        _on_fail(std::current_exception());
        _on_complete = nullptr;
        _on_fail = nullptr;
      } else {
        throw;
      }
    }
  }

  void fail(std::exception_ptr error = nullptr) {
    if (_on_fail) {
      _on_fail(error);
      _on_complete = nullptr;
      _on_fail = nullptr;
    }
  }

  void onComplete(std::function<void(T&)> on_complete, std::function<void(std::exception_ptr)> on_fail = [](auto) {}) {
    _on_complete = std::move(on_complete);
    _on_fail = std::move(on_fail);
  }

#ifdef HTTPPP_TASK_INCLUDE
  HTTPPP_TASK_TYPE<T> onComplete() {
    return HTTPPP_TASK_CREATE<T>([this](auto& resolve, auto& reject) {
      onComplete(resolve, reject);
    });
  }
#endif

  void close() {
    if (_on_complete) {
      _on_complete(_result);
      _on_complete = nullptr;
    }
  }

  auto& result() {
    return _result;
  }

  operator bool() const {
    return _done;
  }

private:
  static constexpr http_parser_type type = std::is_same_v<T, http::request> ? HTTP_REQUEST : HTTP_RESPONSE;

  http_parser_settings _settings;
  http_parser _parser;

  bool _done = false;

  std::function<void(T&)> _on_complete;
  std::function<void(std::exception_ptr)> _on_fail;

  T _result;

  std::string _header;
};
} // namespace http
