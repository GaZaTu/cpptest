#pragma once

#include "http_parser.h"
#include <string>
#include <string_view>
#include <unordered_map>
#include <stdexcept>
#ifdef HTTPPP_TASK_INCLUDE
#include HTTPPP_TASK_INCLUDE
#endif

namespace http {
struct method {
public:
  enum _enum {
#define XX(num, name, string) name = HTTP_##name,
    HTTP_METHOD_MAP(XX)
#undef XX
  };

  method() = delete;

  method(_enum value);

  method(http_method value);

  method(std::string_view value);

  operator http_method() const;

  operator std::string_view() const;

private:
  _enum _value;
};

constexpr auto DELETE = method::DELETE;
constexpr auto GET = method::GET;
constexpr auto HEAD = method::HEAD;
constexpr auto POST = method::POST;
constexpr auto PUT = method::PUT;
constexpr auto CONNECT = method::CONNECT;

struct status {
public:
  enum _enum {
    UNKNOWN = 0,
#define XX(num, name, string) name = HTTP_STATUS_##name,
    HTTP_STATUS_MAP(XX)
#undef XX
  };

  status() = delete;

  status(_enum value);

  status(http_status value);

  status(std::string_view value);

  operator http_status() const;

  operator std::string_view() const;

private:
  _enum _value;
};

constexpr auto UNKNOWN = status::UNKNOWN;
constexpr auto OK = status::OK;
constexpr auto CREATED = status::CREATED;
constexpr auto NO_CONTENT = status::NO_CONTENT;
constexpr auto MOVED_PERMANENTLY = status::MOVED_PERMANENTLY;
constexpr auto BAD_REQUEST = status::BAD_REQUEST;
constexpr auto UNAUTHORIZED = status::UNAUTHORIZED;
constexpr auto FORBIDDEN = status::FORBIDDEN;
constexpr auto NOT_FOUND = status::NOT_FOUND;
constexpr auto METHOD_NOT_ALLOWED = status::METHOD_NOT_ALLOWED;
constexpr auto INTERNAL_SERVER_ERROR = status::INTERNAL_SERVER_ERROR;

struct url {
public:
  static constexpr bool IN = true;
  static constexpr bool OUT = false;

  url();

  url(std::string_view str, bool in = OUT);

  url(const char* str);

  url(const url&) = default;

  url(url&&) = default;

  url& operator=(const url&) = default;

  url& operator=(url&&) = default;

  const std::string& schema() const;

  url& schema(std::string_view value);

  const std::string& host() const;

  url& host(std::string_view value);

  uint16_t port() const;

  url& port(uint16_t value);

  const std::string& path() const;

  url& path(std::string_view value);

  const std::unordered_map<std::string, std::string>& query() const;

  url& query(const std::unordered_map<std::string, std::string>& map);

  std::optional<std::string_view> queryvalue(const std::string& name) const;

  template <typename R>
  std::optional<R> queryvalue(const std::string& name) const {
    std::optional<std::string_view> value = queryvalue(name);
    if (!value) {
      return std::nullopt;
    }

    std::string_view valuestr = value.value();
    if (valuestr == "" || valuestr == "null") {
      return std::nullopt;
    }

    if constexpr (std::is_same_v<R, std::string>) {
      return (std::string)valuestr;
    } else if constexpr (std::is_same_v<R, bool>) {
      return (valuestr == "1" || valuestr == "true");
    } else if constexpr (std::is_integral_v<R>) {
      return (R)std::stoi((std::string)valuestr);
    } else if constexpr (std::is_floating_point_v<R>) {
      return (R)std::stof((std::string)valuestr);
    } else {
      return {};
    }
  }

  url& queryvalue(const std::string& name, std::string_view value);

  const std::string& fragment() const;

  url& fragment(std::string_view value);

  std::string fullpath() const;

  explicit operator std::string() const;

private:
  std::string _schema;
  std::string _host;
  uint16_t _port = 0;
  std::string _path = "/";
  std::unordered_map<std::string, std::string> _query;
  std::string _fragment;
};

struct proxyinfo {
public:
  std::string host;
  uint16_t port = 0;
  std::string auth;
};

struct request {
public:
  std::tuple<uint8_t, uint8_t> version = {1, 1};

  http::method method = http::GET;

  http::url url;

  std::unordered_map<std::string, std::string> headers;

  std::string body;

  proxyinfo proxy;

  explicit operator std::string() const;

private:
  static std::string stringify(const request& r, const std::unordered_map<std::string, std::string>& headers = {});
};

struct response {
public:
  std::tuple<uint8_t, uint8_t> version = {1, 1};

  http::status status = http::UNKNOWN;

  std::unordered_map<std::string, std::string> headers;

  std::string body;

  operator bool() const;

  explicit operator std::string() const;

private:
  static std::string stringify(const response& r, const std::unordered_map<std::string, std::string>& headers = {});
};

class error : public std::runtime_error {
public:
  error(const std::string& s);

  error(unsigned int c = 0);

  error(const response& r);

private:
  static std::string errno_string(unsigned int code);
};

namespace serve {
#ifdef HTTPPP_TASK_INCLUDE
using handler = std::function<HTTPPP_TASK_TYPE<void>(http::request&, http::response&)>;
#endif
}
} // namespace http
