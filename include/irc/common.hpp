#pragma once

#include <regex>
#include <string>
#include <unordered_map>

namespace irc {
class parsing_error : public std::exception {
public:
  std::string raw;

  parsing_error() {
  }

  parsing_error(std::string& r) : raw(std::move(r)) {
  }
};

std::tuple<std::string_view, std::unordered_map<std::string_view, std::string_view>> consumeTags(std::string_view raw) {
  std::unordered_map<std::string_view, std::string_view> tags;

  std::string_view key;
  std::string_view value;

  if (raw[0] != '@') {
    return {raw, std::move(tags)};
  }

  size_t offset = 1;
  size_t length = raw.length();
  for (size_t i = offset; i < length; i++) {
    switch (raw[i]) {
    case '=':
      key = raw.substr(offset, i - offset);
      offset = i + 1;
      break;

    case ';':
    case ' ':
      value = raw.substr(offset, i - offset);
      offset = i + 1;
      tags[key] = value;

      if (raw[i] == ' ') {
        goto end;
      }
      break;
    }
  }

end:
  return {raw.substr(offset), std::move(tags)};
}

namespace regex {
std::regex privmsg{"^:(\\w+)!\\w+@\\S+ PRIVMSG #(\\w+) :"};
std::regex ping{"^PING :(.*)"};
std::regex clearchat{"^:\\S+ CLEARCHAT #(\\w+) :(\\w+)"};
std::regex usernotice{"^:\\S+ USERNOTICE #(\\w+) :"};
std::regex userstate{"^:\\S+ USERSTATE #(\\w+)"};
std::regex roomstate{"^:\\S+ ROOMSTATE #(\\w+)"};
std::regex reconnect{"RECONNECT"};
} // namespace regex
} // namespace irc
