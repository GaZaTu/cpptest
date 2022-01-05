#pragma once

#include "irc.hpp"
#include "irc/twitch.hpp"
#include "ssl-openssl.hpp"
#include "uv.hpp"
// #include "rx.hpp"
#include <unordered_set>

namespace irc::twitch {
class bot {
public:
  int message_queue_size = 3;
  int message_timeout = 1600;

  std::unordered_set<char> illegal_prefix_set{'/', '.'};

  bot() {
    handle<irc::ping>([this](const irc::ping& ping) {
      _reader.write(irc::out::pong(ping.server())).start();
      _writer.write(irc::out::pong(ping.server())).start();
    });
  }

  void useSSL(ssl::context& ssl_context) {
    _reader.useSSL(ssl_context);
    _writer.useSSL(ssl_context);
  }

  task<void> connect() {
    std::string host = "irc.chat.twitch.tv";
    std::string port = "";

    if (_reader.sslState() && _writer.sslState()) {
      port = "6697";
    }

    co_await _reader.connect(host, port);
    co_await _writer.connect(host, port);
  }

  task<void> auth(std::string_view nick, std::string_view pass) {
    co_await _reader.write(irc::out::twitch::authAnon());
    co_await _writer.write(irc::out::auth(nick, pass));
  }

  task<void> join(std::string_view chn) {
    co_await _reader.write(irc::out::join(chn));
    co_await _writer.write(irc::out::join(chn));
  }

  task<void> sendUnchecked(std::string_view chn, std::string_view msg) {
    if (_messages_in_queue > message_queue_size) {
      co_return;
    }

    _messages_in_queue += 1;

    while (true) {
      auto now = uv::hrtime();
      auto diff = now - _hrtime_of_last_send;

      if (diff > message_timeout) {
        _messages_in_queue -= 1;
        _hrtime_of_last_send = now;
        break;
      } else {
        co_await uv::timeout(message_timeout + 1 - diff);
      }
    }

    co_await _writer.write(irc::out::privmsg(chn, msg));
  }

  task<void> send(std::string_view chn, std::string_view msg) {
    if (msg.empty() || illegal_prefix_set.contains(msg[0])) {
      co_return;
    }

    co_await sendUnchecked(chn, msg);
  }

  task<void> send(const irc::privmsg& privmsg, std::string_view msg) {
    co_await send(privmsg.channel(), msg);
  }

  task<void> reply(const irc::privmsg& privmsg, std::string_view msg) {
    co_await send(privmsg, std::string{"@"} + std::string{privmsg.sender()} + std::string{" "} + std::string{msg});
  }

  task<void> capreq(std::string_view cap) {
    co_await _reader.write(irc::out::capreq(cap));
  }

  task<void> readMessagesUntilEOF() {
    co_await _reader.readLinesAsViewsUntilEOF([this](auto line) {
      irc::message message = irc::parse(line);

      for (auto& handler : _handlers) {
        handler(message);
      }
    });
  }

  void handle(std::function<void(const irc::message&)> handler) {
    _handlers.push_back(std::move(handler));
  }

  template <typename T>
  void handle(std::function<void(const T&)> handler) {
    handle([handler{std::move(handler)}](const irc::message& message) {
      if (const T* result = irc::get_if<T>(message)) {
        handler(*result);
      }
    });
  }

  void command(std::regex&& regexp, std::function<void(const irc::privmsg&, std::cmatch&&)> handler) {
    handle<irc::privmsg>([regexp{std::move(regexp)}, handler{std::move(handler)}](const irc::privmsg& privmsg) {
      std::cmatch match;
      std::regex_search(privmsg.message().begin(), privmsg.message().end(), match, regexp);

      if (!match.empty()) {
        handler(privmsg, std::move(match));
      }
    });
  }

  // struct privmsg : public irc::privmsg {
  // public:
  //   friend bot;

  //   using irc::privmsg::privmsg;

  //   const std::cmatch& match() const {
  //     return _match;
  //   }

  // private:
  //   std::cmatch _match;
  // };

  // rxcpp::observable<bot::privmsg> command(std::regex&& regexp) {
  //   return rxcpp::observable<>::create([regexp{std::move(regexp)}](rxcpp::subscriber<int> s) {
  //     command(regexp, [&s](const irc::privmsg& privmsg, std::cmatch&& match) {
  //       bot::privmsg r{privmsg};
  //       r._match = match;
  //       return r;
  //     });
  //   });
  // }

private:
  uv::tcp _reader;
  uv::tcp _writer;

  std::vector<std::function<void(const irc::message&)>> _handlers;

  uint64_t _hrtime_of_last_send = 0;
  int _messages_in_queue = 0;
};
}
