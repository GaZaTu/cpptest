#include "http/common.hpp"
#include <sstream>

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

method::method(_enum value) : _value{value} {}

method::method(http_method value) : _value{(_enum)value} {}

method::method(std::string_view value) {
  for (const auto& [key, entry] : detail::method::strings) {
    if (entry.name == value) {
      _value = (_enum)key;
      break;
    }
  }
}

method::operator http_method() const {
  return (http_method)_value;
}

method::operator std::string_view() const {
  return detail::method::strings[(http_method)_value].text;
}

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

status::status(_enum value) : _value{value} {}

status::status(http_status value) : _value{(_enum)value} {}

status::status(std::string_view value) {
  for (const auto& [key, entry] : detail::status::strings) {
    if (entry.name == value) {
      _value = (_enum)key;
      break;
    }
  }
}

status::operator http_status() const {
  return (http_status)_value;
}

status::operator std::string_view() const {
  return detail::status::strings[(http_status)_value].text;
}

namespace detail {
std::unordered_map<std::string_view, uint16_t> known_protocols = {
  {"http", 80},
  {"https", 443},
  {"ws", 80},
  {"wss", 443},
};
}

url::url() {
}

url::url(std::string_view str, bool in) {
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
  std::string querystr;
  move(UF_QUERY, querystr);
  move(UF_FRAGMENT, _fragment);

  if (detail::known_protocols.contains(_schema)) {
    _port = detail::known_protocols[_schema];
  }

  if (parser.port != 0) {
    _port = parser.port;
  }

  if (querystr.length()) {
    std::string name;
    std::string value;
    auto reading = &name;
    char hex[3] = {0};

    for (int i = 0; i <= querystr.length(); i++) {
      char chr = querystr[i];

      switch (chr) {
        case '=':
          reading = &value;
          break;

        case '&':
        case '\0': {
          _query[std::move(name)] = std::move(value);
          reading = &name;
          break;
        }

        case '%':
          hex[0] = querystr[++i];
          hex[1] = querystr[++i];
          hex[2] = '\0';
          *reading += (char)std::stoi(hex, 0, 16);
          break;

        case '+':
          *reading += ' ';
          break;

        default:
          *reading += chr;
      }
    }
  }
}

url::url(const char* str) : url(std::string_view{str}) {
}

const std::string& url::schema() const {
  return _schema;
}

url& url::schema(std::string_view value) {
  _schema = value;
  return *this;
}

const std::string& url::host() const {
  return _host;
}

url& url::host(std::string_view value) {
  _host = value;
  return *this;
}

uint16_t url::port() const {
  return _port;
}

url& url::port(uint16_t value) {
  _port = value;
  return *this;
}

const std::string& url::path() const {
  return _path;
}

url& url::path(std::string_view value) {
  _path = value;
  return *this;
}

const std::unordered_map<std::string, std::string>& url::query() const {
  return _query;
}

url& url::query(const std::unordered_map<std::string, std::string>& value) {
  _query = value;
  return *this;
}

std::optional<std::string_view> url::queryvalue(const std::string& name) const {
  if (_query.count(name)) {
    return _query.at(name);
  } else {
    return std::nullopt;
  }
}

url& url::queryvalue(const std::string& name, std::string_view value) {
  _query[name] = (std::string)value;
  return *this;
}

const std::string& url::fragment() const {
  return _fragment;
}

url& url::fragment(std::string_view value) {
  _fragment = value;
  return *this;
}

std::string url::fullpath() const {
  std::string result = _path;

  if (_query.size()) {
    result += "?";
    for (const auto& [key, value] : _query) {
      if (*(std::end(result) - 1) != '?') {
        result += '&';
      }
      result += key + '=';
      for (auto chr : value) {
        switch (chr) {
          case '=':
            result += "%3d";
            break;
          case '&':
            result += "%26";
            break;
          case '/':
            result += "%2f";
            break;
          case ' ':
            result += "%20";
            break;
          default:
            result += chr;
        }
      }
    }
  }

  if (_fragment != "") {
    result += "#" + _fragment;
  }

  return result;
}

url::operator std::string() const {
  std::stringstream result;

  if (_host != "") {
    if (_schema != "") {
      result << _schema << "://";
    }

    result << _host;

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

request::operator std::string() const {
  return stringify(*this);
}

std::string request::stringify(const request& r, const std::unordered_map<std::string, std::string>& headers) {
  auto url = r.url;
  if (r.method == method::CONNECT) {
    url.schema("").path("").query({}).fragment("");
  } else {
    url.host("");
  }

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

response::operator bool() const {
  return status >= 200 && status < 300;
}

response::operator std::string() const {
  return stringify(*this);
}

std::string response::stringify(const response& r, const std::unordered_map<std::string, std::string>& headers) {
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

error::error(const std::string& s) : std::runtime_error(s) {
}

error::error(unsigned int c) : std::runtime_error(errno_string(c)) {
}

error::error(const response& r) : std::runtime_error(std::to_string(r.status) + " " + (std::string)r.status) {
}

std::string error::errno_string(unsigned int code) {
  return std::string{http_errno_name((http_errno)code)} + ": " +
      std::string{http_errno_description((http_errno)code)};
}
} // namespace http
