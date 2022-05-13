#pragma once

#include "./common.hpp"
#include <variant>

namespace irc {
struct join {
public:
  join(std::string& raw) : _raw(std::move(raw)) {
    parse();
  }

  join(std::string_view raw) : _raw(raw) {
    parse();
  }

  join(const join& other) {
    *this = other;
  }

  join(join&&) = default;

  join& operator=(const join& other) {
    _raw = (std::string)other;
    parse();

    return *this;
  }

  join& operator=(join&&) = default;

  std::string_view channel() const {
    return _views._channel;
  }

  std::string_view user() const {
    return _views._user;
  }

  // std::unordered_map<std::string_view, std::string_view>& tags() {
  //   return _tags;
  // }

  const std::unordered_map<std::string_view, std::string_view>& tags() const {
    return _tags;
  }

  std::string_view operator[](std::string_view key) const {
    return _tags.at(key);
  }

  explicit operator std::string_view() const {
    if (_raw.index() == VIEW) {
      return std::get<VIEW>(_raw);
    } else {
      return std::get<STRING>(_raw);
    }
  }

  explicit operator std::string() const {
    return (std::string)(std::string_view)(*this);
  }

private:
  static constexpr int VIEW = 0;
  static constexpr int STRING = 1;

  std::variant<std::string_view, std::string> _raw;
  std::unordered_map<std::string_view, std::string_view> _tags;

  struct {
    std::string_view _channel;
    std::string_view _user;
  } _views;

  void parse() {
    std::string_view data;
    std::tie(data, _tags) = consumeTags((std::string_view)(*this));

    std::cmatch match;
    std::regex_search(std::begin(data), std::end(data), match, regex::join);

    if (match.empty()) {
      if (_raw.index() == STRING) {
        throw parsing_error{std::get<STRING>(_raw)};
      } else {
        throw parsing_error{};
      }
    }

    _views._channel = {match[2].first, (size_t)match[2].length()};
    _views._user = {match[1].first, (size_t)match[1].length()};
  }
};
} // namespace irc
