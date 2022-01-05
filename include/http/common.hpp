#pragma once

#include "http_parser.h"
#include <sstream>
#include <string>
#include <unordered_map>

namespace http {
namespace detail {
namespace method {
struct entry {
  std::string_view name;
  std::string_view text;
};
std::unordered_map<http_method, entry> strings = {
#define XX(num, name, string) {HTTP_##name, {#name, #string}},
    HTTP_METHOD_MAP(XX)
#undef XX
};
}
}

struct method {
public:
  enum _enum {
#define XX(num, name, string) name = HTTP_##name,
    HTTP_METHOD_MAP(XX)
#undef XX
  };

  method() = delete;

  method(_enum value) : _value{value} {}

  method(http_method value) : _value{(_enum)value} {}

  method(std::string_view value) {
    for (const auto& [key, entry] : detail::method::strings) {
      if (entry.name == value) {
        _value = (_enum)key;
        break;
      }
    }
  }

  operator http_method() const {
    return (http_method)_value;
  }

  operator std::string_view() const {
    return detail::method::strings[(http_method)_value].text;
  }

private:
  _enum _value;
};

namespace detail {
namespace status {
struct entry {
  std::string_view name;
  std::string_view text;
};
std::unordered_map<http_status, entry> strings = {
#define XX(num, name, string) {HTTP_STATUS_##name, {#name, #string}},
    HTTP_STATUS_MAP(XX)
#undef XX
};
}
}

struct status {
public:
  enum _enum {
#define XX(num, name, string) name = HTTP_STATUS_##name,
    HTTP_STATUS_MAP(XX)
#undef XX
  };

  status() = delete;

  status(_enum value) : _value{value} {}

  status(http_status value) : _value{(_enum)value} {}

  status(std::string_view value) {
    for (const auto& [key, entry] : detail::status::strings) {
      if (entry.name == value) {
        _value = (_enum)key;
        break;
      }
    }
  }

  operator http_status() const {
    return (http_status)_value;
  }

  operator std::string_view() const {
    return detail::status::strings[(http_status)_value].text;
  }

private:
  _enum _value;
};

namespace detail {
std::unordered_map<std::string_view, uint16_t> known_protocols = {
  {"http", 80},
  {"https", 443},
};
}

struct url {
public:
  static constexpr bool IN = true;
  static constexpr bool OUT = false;

  url() {
  }

  url(std::string_view str, bool in = OUT) {
    http_parser_url parser;
    http_parser_url_init(&parser);
    http_parser_parse_url(str.data(), str.length(), in, &parser);

    auto move = [&](http_parser_url_fields field, std::string& member) {
      auto& data = parser.field_data[field];

      if (data.len != 0) {
        member = std::string_view{str.data() + data.off, data.len};
      }
    };

    move(UF_SCHEMA, _schema);
    move(UF_HOST, _host);
    // move(UF_PORT, _port);
    move(UF_PATH, _path);
    move(UF_QUERY, _query);
    move(UF_FRAGMENT, _fragment);

    if (detail::known_protocols.contains(_schema)) {
      _port = detail::known_protocols[_schema];
    }

    if (parser.port != 0) {
      _port = parser.port;
    }
  }

  url(const char* str) : url(std::string_view{str}) {
  }

  url(const url&) = default;

  url(url&&) = default;

  url& operator=(const url&) = default;

  url& operator=(url&&) = default;

  const std::string& schema() const {
    return _schema;
  }

  url& schema(std::string_view value) {
    _schema = value;
    return *this;
  }

  const std::string& host() const {
    return _host;
  }

  url& host(std::string_view value) {
    _host = value;
    return *this;
  }

  uint16_t port() const {
    return _port;
  }

  url& port(uint16_t value) {
    _port = value;
    return *this;
  }

  const std::string& path() const {
    return _path;
  }

  url& path(std::string_view value) {
    _path = value;
    return *this;
  }

  const std::string& query() const {
    return _query;
  }

  url& query(std::string_view value) {
    _query = value;
    return *this;
  }

  url& query(const std::unordered_map<std::string, std::string>& map) {
    _query = "";
    for (const auto& [key, value] : map) {
      if (!_query.empty()) {
        _query += '&';
      }
      _query += key + '=' + value;
    }
    return *this;
  }

  const std::string& fragment() const {
    return _fragment;
  }

  url& fragment(std::string_view value) {
    _fragment = value;
    return *this;
  }

  std::string fullpath() const {
    std::string result = _path;

    if (_query != "") {
      result += "?" + _query;
    }

    if (_fragment != "") {
      result += "#" + _fragment;
    }

    return result;
  }

  explicit operator std::string() const {
    std::stringstream result;

    if (_host != "") {
      result << _schema << "://" << _host;

      if (_port != 0) {
        auto protocol_port = detail::known_protocols.find(_schema);
        if (protocol_port == detail::known_protocols.end() || _port != protocol_port->second) {
          result << ":" << _port;
        }
      }
    }

    result << fullpath();

    return result.str();
  }

private:
  std::string _schema;
  std::string _host;
  uint16_t _port = 0;
  std::string _path = "/";
  std::string _query;
  std::string _fragment;
};

struct request {
public:
  std::tuple<uint8_t, uint8_t> version = {1, 1};

  http::url url;

  http::method method = http::method::GET;

  std::unordered_map<std::string, std::string> headers;

  std::string body;

  request() {
  }

  request(http::method m, http::url u, std::string b = {}) : method(m), url(u), body(b) {
  }

  request(http::url u) : url(u) {
  }

  explicit operator std::string() const {
    return stringify(*this);
  }

private:
  static std::string stringify(const request& r, const std::unordered_map<std::string, std::string>& headers = {}) {
    auto url = r.url;
    url.host("");

    std::stringstream result;

    result << (std::string)r.method << " ";
    result << (std::string)url << " ";
    result << "HTTP/" << (int)std::get<0>(r.version) << "." << (int)std::get<1>(r.version) << "\r\n";

    for (const auto& [key, value] : headers) {
      result << key << ": " << value << "\r\n";
    }

    for (const auto& [key, value] : r.headers) {
      result << key << ": " << value << "\r\n";
    }

    result << "\r\n";

    if (r.body.length() > 0) {
      result << r.body << "\r\n\r\n";
    }

    return result.str();
  }
};

struct response {
public:
  std::tuple<uint8_t, uint8_t> version = {1, 1};

  http::status status = http::status::OK;

  std::unordered_map<std::string, std::string> headers;

  std::string body;

  bool upgrade = false;

  response() {
  }

  response(http::status s, std::string b = {}) : status(s), body(b) {
  }

  operator bool() const {
    return status >= 200 && status < 300;
  }

  explicit operator std::string() const {
    return stringify(*this);
  }

private:
  static std::string stringify(const response& r, const std::unordered_map<std::string, std::string>& headers = {}) {
    std::stringstream result;

    result << "HTTP/" << (int)std::get<0>(r.version) << "." << (int)std::get<1>(r.version) << " ";
    result << (int)r.status << " ";
    result << (std::string)r.status << "\r\n";

    for (const auto& [key, value] : headers) {
      result << key << ": " << value << "\r\n";
    }

    for (const auto& [key, value] : r.headers) {
      result << key << ": " << value << "\r\n";
    }

    result << "\r\n";

    if (r.body.length() > 0) {
      result << r.body << "\r\n\r\n";
    }

    return result.str();
  }
};

class error : public std::runtime_error {
public:
  error(const std::string& s) : std::runtime_error(s) {
  }

  error(unsigned int c = 0) : std::runtime_error(errno_string(c)) {
  }

  error(const response& r) : std::runtime_error(std::to_string(r.status) + " " + (std::string)r.status) {
  }

private:
  static std::string errno_string(unsigned int code) {
    return std::string{http_errno_name((http_errno)code)} + ": " +
        std::string{http_errno_description((http_errno)code)};
  }
};
} // namespace http
