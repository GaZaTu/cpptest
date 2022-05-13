#pragma once

#include "./parse.hpp"
#include "./twitch.hpp"
#include <unordered_set>
#include <unordered_map>
#include <random>
#include "uv.hpp"
#include "task.hpp"

namespace irc::twitch {
class bot {
public:
  int message_queue_size = 3;
  int message_timeout = 1600;

  std::unordered_set<char> illegal_prefix_set{'/', '.'};

  bot() {
    handle<irc::ping>([this](const irc::ping& ping) {
      if (_on_send_with_reader) {
        _on_send_with_reader(irc::out::pong(ping.server()));
      }

      if (_on_send_with_writer) {
        _on_send_with_writer(irc::out::pong(ping.server()));
      }
    });
  }

  void onSendWithReader(std::function<void(std::string_view)> on_send) {
    _on_send_with_reader = on_send;
  }

  void onSendWithWriter(std::function<void(std::string_view)> on_send) {
    _on_send_with_writer = on_send;
  }

  void feed(std::string_view chunk) {
    _stream << chunk;

    std::string line;
    while (std::getline(_stream, line)) {
      if (line.ends_with('\r')) {
        line.resize(line.size() - 1);
      }

      irc::message message = irc::parse(line);
      for (auto& [id, handler] : _handlers) {
        handler(message, id);
      }
    }

    if (_stream.eof()) {
      _stream.clear();
      _stream.str({});
    }
  }

  task<void> authAnon() {
    if (_on_send_with_reader) {
      _on_send_with_reader(irc::out::twitch::nickAnon());
    }

    return task<void>::create([&](auto& resolve, auto&) {
      handle([&](const auto&, uint64_t id) {
        remove(id);
        resolve();
      });
    });
  }

  task<void> auth(std::string_view nick, std::string_view pass) {
    auto r = authAnon();

    if (_on_send_with_writer) {
      _on_send_with_writer(irc::out::pass(pass));
      _on_send_with_writer(irc::out::nick(nick));
    }

    return r;
  }

  task<void> join(std::string_view chn) {
    if (_on_send_with_reader) {
      _on_send_with_reader(irc::out::join(chn));
    }

    if (_on_send_with_writer) {
      _on_send_with_writer(irc::out::join(chn));
    }

    return task<void>::create([&](auto& resolve, auto&) {
      handle<irc::join>([&, chn{(std::string)chn}](const irc::join& join, uint64_t id) {
        if (join.channel() != chn) {
          return;
        }

        remove(id);
        resolve();
      });
    });
  }

  void part(std::string_view chn) {
    if (_on_send_with_reader) {
      _on_send_with_reader(irc::out::part(chn));
    }

    if (_on_send_with_writer) {
      _on_send_with_writer(irc::out::part(chn));
    }
  }

  task<void> sendUnchecked(std::string_view _chn, std::string_view _msg) {
    auto chn = (std::string)_chn;
    auto msg = (std::string)_msg;

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

    if (_on_send_with_writer) {
      _on_send_with_writer(irc::out::privmsg(chn, msg));
    }
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

  void capreq(std::string_view cap) {
    if (_on_send_with_reader) {
      _on_send_with_reader(irc::out::capreq(cap));
    } else {
      _on_send_with_writer(irc::out::capreq(cap));
    }
  }

  void remove(uint64_t handler) {
    _handlers.erase(handler);
  }

  uint64_t handle(std::function<void(const irc::message&, uint64_t)> handler) {
    uint64_t id = _random_uint64(_rng);
    _handlers.emplace(id, std::move(handler));
    return id;
  }

  template <typename T>
  uint64_t handle(std::function<void(const T&, uint64_t)> handler) {
    return handle([handler{std::move(handler)}](const irc::message& message, uint64_t id) {
      if (const T* result = irc::get_if<T>(message)) {
        handler(*result, id);
      }
    });
  }

  uint64_t handle(std::function<void(const irc::message&)> handler) {
    return handle([handler{std::move(handler)}](const irc::message& message, uint64_t id) {
      handler(message);
    });
  }

  template <typename T>
  uint64_t handle(std::function<void(const T&)> handler) {
    return handle<T>([handler{std::move(handler)}](const T& message, uint64_t id) {
      handler(message);
    });
  }

  uint64_t once(std::function<void(const irc::message&)> handler) {
    return handle([this, handler{std::move(handler)}](const irc::message& message, uint64_t id) {
      remove(id);
      handler(message);
    });
  }

  template <typename T>
  uint64_t once(std::function<void(const T&)> handler) {
    return handle<T>([this, handler{std::move(handler)}](const T& message, uint64_t id) {
      remove(id);
      handler(message);
    });
  }

  task<irc::message> once() {
    return task<irc::message>::create([this](auto& resolve, auto&) {
      once([&](const irc::message& _message) {
        auto message = _message;
        resolve(message);
      });
    });
  }

  template <typename T>
  task<T> once() {
    return task<T>::create([this](auto& resolve, auto&) {
      once<T>([&](const T& _message) {
        auto message = _message;
        resolve(message);
      });
    });
  }

  void command(std::regex&& regexp, std::function<void(const irc::privmsg&, std::cmatch&&)> handler) {
    handle<irc::privmsg>([regexp{std::move(regexp)}, handler{std::move(handler)}](const irc::privmsg& privmsg) {
      std::string_view msg = privmsg.message();

      std::cmatch match;
      std::regex_search(std::begin(msg), std::end(msg), match, regexp);
      if (match.empty()) {
        return;
      }

      handler(privmsg, std::move(match));
    });
  }

private:
  std::stringstream _stream;

  std::function<void(std::string_view)> _on_send_with_reader;
  std::function<void(std::string_view)> _on_send_with_writer;

  std::unordered_map<uint64_t, std::function<void(const irc::message&, uint64_t)>> _handlers;

  uint64_t _hrtime_of_last_send = 0;
  int _messages_in_queue = 0;

  std::random_device _rd;
  std::mt19937 _rng{_rd()};
  std::uniform_int_distribution<uint64_t> _random_uint64{std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max()};
};
}
