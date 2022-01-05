// #define UVPP_PERF 1

#include "http.hpp"
#include "twitchircbot.hpp"
#include "uv.hpp"
#include "db.hpp"
#include "db-sqlite.hpp"
#include "ulid_uint128.hh"
#include <iostream>
#include <regex>
#include "json.hpp"
#include "finally.hpp"

#include <cmrc/cmrc.hpp>
CMRC_DECLARE(res);

template<typename ...Args>
std::string string_format(const std::string& format, Args... args) {
  int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
  if (size_s <= 0) { throw std::runtime_error("Error during formatting."); }
  auto size = static_cast<size_t>(size_s);
  auto buf = std::make_unique<char[]>(size);
  std::snprintf(buf.get(), size, format.c_str(), args...);
  return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

std::size_t string_replace_all(std::string& inout, std::string_view what, std::string_view with) {
  std::size_t count{};
  for (std::string::size_type pos{};
      inout.npos != (pos = inout.find(what.data(), pos, what.length()));
      pos += with.length(), ++count) {
    inout.replace(pos, what.length(), with.data(), with.length());
  }
  return count;
}

std::size_t string_remove_all(std::string& inout, std::string_view what) {
  return string_replace_all(inout, what, "");
}

std::string expand_macros(std::istream& in, const std::unordered_map<std::string, std::string>& defines) {
  std::string_view IFDEF = "--#ifdef";
  std::string_view ELIF = "--#elif";
  std::string_view ELSE = "--#else";
  std::string_view ENDIF = "--#endif";

  std::string result;

  int ifdef_depth = 0;
  bool ifdef_valid = true;
  bool ifdef_was_valid = false;

  for (std::string line; std::getline(in, line, '\n'); ) {
    const auto lineStartsWith = [&line](std::string_view keyword) {
      return line.length() >= keyword.length() && line.substr(0, keyword.length()) == keyword;
    };

    if (line.empty()) {
      result += '\n';
    } else if (lineStartsWith(IFDEF)) {
      ifdef_depth += 1;
      ifdef_valid = defines.count(line.substr(IFDEF.length() + 1)) != 0;
    } else if (lineStartsWith(ELIF)) {
      if (ifdef_depth > 0 && !ifdef_valid && !ifdef_was_valid) {
        ifdef_valid = defines.count(line.substr(ELIF.length() + 1)) != 0;
      } else {
        ifdef_valid = false;
        ifdef_was_valid = true;
      }
    } else if (lineStartsWith(ELSE)) {
      if (ifdef_depth > 0 && !ifdef_valid && !ifdef_was_valid) {
        ifdef_valid = true;
      } else {
        ifdef_valid = false;
        ifdef_was_valid = true;
      }
    } else if (lineStartsWith(ENDIF)) {
      ifdef_depth -= 1;
      ifdef_valid = true;
      ifdef_was_valid = false;
    } else if (ifdef_valid) {
      result += line + '\n';
    }
  }

  return result;
}

std::string expand_macros(std::string_view in, const std::unordered_map<std::string, std::string>& defines) {
  std::stringstream stream{std::string{in}};
  return expand_macros(stream, defines);
}

void registerDatabaseUpgradeScripts(db::datasource& datasource) {
  for (auto entry : cmrc::res::get_filesystem().iterate_directory("src/res/sql/upgrades")) {
    auto filename = entry.filename();
    auto version = std::stoi(filename.substr(1, 4));

    datasource.updates().push_back({version,
        [path{"src/res/sql/upgrades/" + filename}](db::connection& connection) {
          auto file = cmrc::res::get_filesystem().open(path);
          auto script = std::string{file.begin(), file.size()};

          connection.execute(script);
        },
        [path{"src/res/sql/downgrades/" + filename}](db::connection& connection) {
          auto file = cmrc::res::get_filesystem().open(path);
          auto script = std::string{file.begin(), file.size()};

          connection.execute(script);
        }});
  }
}

void handleConnectionCreate(db::connection& conn) {
  conn.execute("PRAGMA journal_mode = WAL");
  conn.execute("PRAGMA synchronous = normal");
  conn.execute("PRAGMA temp_store = memory");
  conn.execute("PRAGMA mmap_size = 268435456");
  conn.execute("PRAGMA cache_size = -10000");
  conn.execute("PRAGMA foreign_keys = ON");

  db::sqlite::createScalarFunction(conn, "create_iso_timestamp_triggers", [](sqlite3* _conn, sqlite3_context* context, int argc, sqlite3_value** argv) {
    auto conn = db::sqlite::convertConnection(_conn);
    auto table = db::sqlite::getString(argv[0]);
    auto column = db::sqlite::getString(argv[1]);

    std::string script = R"~(
CREATE TRIGGER "trg_${TABLE}_after_insert_of_${COLUMN}_fix_iso_timestamp" AFTER INSERT ON "${TABLE}"
BEGIN
UPDATE "${TABLE}"
SET "${COLUMN}" = strftime('%Y-%m-%dT%H:%M:%fZ', NEW."${COLUMN}")
WHERE rowid = NEW.rowid;
END;

CREATE TRIGGER "trg_${TABLE}_after_update_of_${COLUMN}_fix_iso_timestamp" AFTER UPDATE OF "${COLUMN}" ON "${TABLE}"
WHEN NEW."${COLUMN}" IS NOT OLD."${COLUMN}"
BEGIN
UPDATE "${TABLE}"
SET "${COLUMN}" = strftime('%Y-%m-%dT%H:%M:%fZ', NEW."${COLUMN}")
WHERE rowid = NEW.rowid;
END;
    )~";

    string_replace_all(script, "${TABLE}", table);
    string_replace_all(script, "${COLUMN}", column);

    conn.execute(script);
  });

  db::transaction transaction{conn};
  conn.execute("PRAGMA defer_foreign_keys = ON");
  conn.upgrade();
}

namespace db::orm {
template <>
struct idmeta<ulid::ULID> {
  static constexpr bool specialized = true;

  static ulid::ULID null() {
    return 0;
  }

  static ulid::ULID generate() {
    return ulid::CreateNowRand();
  }

  static std::string toString(ulid::ULID value) {
    return ulid::Marshal(value);
  }

  static ulid::ULID fromString(const std::string& string) {
    return ulid::Unmarshal(string);
  }
};
}

std::ostream& operator<<(std::ostream& os, ulid::ULID v) {
  os << ulid::Marshal(v);

  return os;
}

// struct TwitchUser {
//   std::string id = "";
//   std::string name = "";

//   operator bool() {
//     return id != "";
//   }
// };

// DB_ORM_SPECIALIZE(TwitchUser, id, name);
// DB_ORM_PRIMARY_KEY(TwitchUser, id);
using json = nlohmann::json;

namespace nlohmann {
template <typename T>
struct adl_serializer<std::optional<T>> {
  static void to_json(json& j, const std::optional<T>& opt) {
    if (opt) {
      j = *opt;
    } else {
      j = nullptr;
    }
  }

  static void from_json(const json& j, std::optional<T>& opt) {
    if (!j.is_null()) {
      opt = j.get<T>();
    } else {
      opt = std::nullopt;
    }
  }
};
}

namespace trivia {
struct Question {
  std::string category;
  std::string question;
  std::string answer;
  std::optional<std::string> hint1;
  std::optional<std::string> hint2;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Question, category, question, answer, hint1, hint2);

task<std::vector<Question>> getGazatuXyzQuestions(std::string_view count = "0", std::string_view config = "") {
  http::url url{"https://api.gazatu.xyz/trivia/questions"};
  url.query({
    {"verified", "true"},
    {"count", (std::string)count},
  });
  if (!config.empty()) {
    url.query(url.query() + std::string{"&"} + (std::string)config);
  }

  auto response = co_await http::fetch(url);
  std::cout << (std::string)response << std::endl;

  auto json = json::parse(response.body);

  co_return json;
}

struct {
  bool stopped = true;
  bool running = false;
} state;

task<void> run(irc::twitch::bot& bot, const irc::privmsg& privmsg, std::cmatch&& match) {
  if (match.str(1) == "stop") {
    state.stopped = true;
    co_return;
  }

  if (state.running) {
    co_return;
  }

  state.running = true;
  state.stopped = false;

  finally cleanup{[&]() {
    state.running = false;
    bot.send(privmsg, "trivia ended nam").start();
  }};

  auto questions = co_await trivia::getGazatuXyzQuestions("3");

  for (int i = 0; i < questions.size(); i++) {
    auto& question = questions[i];

    if (!question.hint1) {
      question.hint1 = "";

      for (int i = 0; i < question.answer.length(); i++) {
        if (question.answer[i] == ' ') {
          *question.hint1 += question.answer[i];
        } else {
          *question.hint1 += "_";
        }
      }
    }

    if (!question.hint2) {
      question.hint2 = "";

      for (int i = 0; i < question.answer.length(); i++) {
        if (question.answer[i] == ' ' || i < (question.answer.length() * 0.5)) {
          *question.hint2 += question.answer[i];
        } else {
          *question.hint2 += "_";
        }
      }
    }

    if (!state.running || state.stopped) {
      break;
    }

    co_await bot.send(privmsg, string_format("%d/%d category: %s 🤔 question: %s", i + 1, questions.size(), question.category.data(), question.question.data()));

    uv::timer timer1;
    uv::timer timer2;
    uv::timer timer3;

    timer1.startOnce(15000, [&]() {
      bot.send(privmsg, string_format("trivia hint %s", question.hint1->data()));
    });

    timer2.startOnce(30000, [&]() {
      bot.send(privmsg, string_format("trivia hint %s", question.hint2->data()));
    });

    std::function<void(const irc::privmsg&)> onmsg;

    auto done = task<>::create([&](auto& resolve, auto& reject) {
      timer3.startOnce(45000, [&]() {
        bot.send(privmsg, string_format("you guys suck 🤣 the answer was: `%s`", question.answer.data()));
        resolve();
      });

      onmsg = [&](auto& answer) {
        if (answer.channel() != privmsg.channel()) {
          return;
        }

        auto similarity = 0;
        if (similarity < 0.9) {
          return;
        }

        timer1.stop();
        timer2.stop();
        timer3.stop();

        bot.send(privmsg, string_format("%s got it right miniDank the answer was \"%s\"", answer.sender().data(), question.answer.data())).start();

        resolve();
      };
    });

    bot.handle<irc::privmsg>(onmsg);

    co_await done;
    // TODO
  }
}
}

// task<void> amain() {
//   using namespace db::orm::literals;
//   using db::orm::fields;

//   ssl::openssl::driver openssl_driver;
//   ssl::context ssl_context{openssl_driver};

//   db::sqlite::pooled::datasource datasource{".db"};
//   registerDatabaseUpgradeScripts(datasource);

//   datasource.onConnectionCreate() = [](db::connection& conn) {
//     std::cout << "onConnectionCreate" << std::endl;
//     conn.profile(std::cout);

//     handleConnectionCreate(conn);
//   };

//   datasource.onConnectionOpen() = [](db::connection& conn) {
//     std::cout << "onConnectionOpen" << std::endl;
//   };

//   datasource.onConnectionClose() = [](db::connection& conn) {
//     std::cout << "onConnectionClose" << std::endl;

//     conn.execute("PRAGMA optimize");
//   };

//   while (true) {
//     std::cout << "CONNECT" << std::endl;

//     std::string nick = co_await uv::fs::readAll(".irc-nick");
//     std::string pass = co_await uv::fs::readAll(".irc-pass");

//     irc::twitch::bot bot;
//     bot.useSSL(ssl_context);

//     co_await bot.connect();
//     co_await bot.auth(nick, pass);
//     co_await bot.capreq(irc::out::twitch::cap::TAGS);
//     co_await bot.capreq(irc::out::twitch::cap::COMMANDS);
//     co_await bot.join("pajlada");

//     bot.handle<irc::privmsg>([&](const irc::privmsg& privmsg) {
//       if (privmsg.sender() != "pajbot" || !privmsg.message().starts_with("pajaS 🚨 ALERT")) {
//         return;
//       }

//       bot.send(privmsg, "/me FeelsDonkMan 🚨 TERROREM").start();
//     });

//     // bot.command(std::regex{R"~(^pajaS 🚨 ALERT)~"}, [&](const irc::privmsg& privmsg, std::cmatch&& match) {
//     //   if (privmsg.sender() != "pajbot") {
//     //     return;
//     //   }

//     //   bot.send(privmsg, match.str(1)).start();
//     // });

//     bot.command(std::regex{R"~(^!vhns\s+is\s+(.*))~"}, [&](const irc::privmsg& privmsg, std::cmatch&& match) {
//       bot.send(privmsg, match.str(1)).start();
//     });

//     bot.command(std::regex{R"~(^!i\+\+)~"}, [&](const irc::privmsg& privmsg, std::cmatch&& match) {
//       db::orm::repository repo{datasource};

//       auto twitch_user_id = (std::string)privmsg[irc::twitch::tags::USER_ID];

//       auto counter = repo
//         .select<Counter>()
//         .where(
//           fields<Counter>::twitch_user_id == twitch_user_id
//         )
//         .findOne()
//         .value_or(Counter{
//           .twitch_user_id = twitch_user_id,
//         });

//       auto i = counter.i++;

//       repo.save(counter);
//       std::cout << "id: " << counter.id << std::endl;

//       // auto twitch_user = counter.twitch_user(repo);
//       // if (!twitch_user) {
//       //   twitch_user.name = privmsg.sender();
//       //   repo.save(twitch_user);
//       // }

//       bot.reply(privmsg, std::to_string(i)).start();
//     });

//     bot.command(std::regex{R"~(^!idel)~"}, [&](const irc::privmsg& privmsg, std::cmatch&& match) {
//       db::orm::repository repo{datasource};

//       auto twitch_user_id = (std::string)privmsg[irc::twitch::tags::USER_ID];

//       auto counter = repo.select<Counter>()
//         .where(
//           fields<Counter>::twitch_user_id == twitch_user_id
//         )
//         .findOne()
//         .value_or(Counter{});

//       std::cout << "id: " << counter.id << std::endl;
//       repo.remove(counter);

//       auto i = counter.i;

//       bot.reply(privmsg, std::to_string(i)).start();
//     });

//     bot.command(std::regex{R"~(^!i\=(\d+))~"}, [&](const irc::privmsg& privmsg, std::cmatch&& match) {
//       db::orm::repository repo{datasource};

//       auto twitch_user_id = (std::string)privmsg[irc::twitch::tags::USER_ID];

//       auto counter = repo.select<Counter>()
//         .where(
//           fields<Counter>::twitch_user_id == twitch_user_id
//         )
//         .findOne()
//         .value_or(Counter{
//           .twitch_user_id = twitch_user_id,
//         });

//       try {
//         counter.i = std::stoll(match.str(1));
//       } catch (...) {
//         bot.reply(privmsg, "fdm").start();
//         return;
//       }

//       auto i = counter.i;

//       repo.save(counter);
//       std::cout << "id: " << counter.id << std::endl;

//       bot.reply(privmsg, std::to_string(i)).start();
//     });

//     co_await bot.readMessagesUntilEOF();
//   }

//   // uv::tty ttyin(uv::tty::STDIN);
//   // uv::tty ttyout(uv::tty::STDOUT);
//   // co_await ttyin.readLinesAsViewsUntilEOF([&](auto line) {
//   //   task<>::run([&]() -> task<void> {
//   //     auto start = uv::hrtime();

//   //     http::response response = co_await http::fetch(line);
//   //     if (!response) {
//   //       // throw http::error{response};
//   //     }

//   //     std::cout << (std::string)response << std::endl;

//   //     auto end = uv::hrtime();

//   //     std::cout << "request done: " << ((end - start) * 1e-6) << " ms" << std::endl;
//   //   });
//   // });
// }

int main() {
  ssl::openssl::driver openssl_driver;
  ssl::context ssl_context{openssl_driver, ssl::ACCEPT};
  ssl_context.usePrivateKeyFile("server-key.pem");
  ssl_context.useCertificateFile("server-cert.pem");
  ssl_context.useALPNCallback({"h2", "http/1.1"});

  auto callback = [&](http::request& request, http::response& response) -> task<void> {
    // std::cout << (std::string)request << std::endl;

    // auto test = co_await http::fetch("https://api.gazatu.xyz/trivia/questions?count=5");

    response.headers["content-type"] = "text/plain";
    response.body = "Hello World!";

    // std::cout << (std::string)response << std::endl;

    if (request.headers["accept-encoding"].find("gzip") != std::string::npos) {
      response.headers["content-encoding"] = "gzip";
      http::compress(response.body);
    }

    response.headers["content-length"] = std::to_string(response.body.length());

    co_return;
  };

  uv::tcp server;
  server.useSSL(ssl_context);
  server.bind4("127.0.0.1", 8001);
  // server.bind6("::1", 8001);
  server.listen([&](auto error) {
    task<>::run([&]() -> task<void> {
      uv::tcp client;
      co_await server.accept(client);

      if (client.sslState().protocol() == "h2") {
        http2::handler<http::request> handler;

        handler.onSend([&](auto input) {
          client.write((std::string)input).start();
        });

        std::unordered_map<int32_t, std::function<void()>> on_close;

        client.readStart([&](auto chunk, auto error) {
          if (error) {
            for (auto& [key, fn] : on_close) {
              fn();
            }

            handler.close();
          } else {
            handler.execute(chunk);
          }
        });

        handler.submitSettings();
        handler.sendSession();

        handler.onStreamEnd([&](int32_t id, http::request&& request) {
          task<>::run([&handler, &callback, id, &request, &on_close]() -> task<void> {
            bool closed = false;
            on_close.emplace(id, [&closed]() {
              closed = true;
            });

            http::request _request = std::move(request);
            http::response _response;
            co_await callback(_request, _response);

            if (closed) {
              co_return;
            } else {
              on_close.erase(id);
            }

            auto on_sent = handler.submitResponse(_response, id);
            handler.sendSession();
            co_await on_sent;
          });
        });

        co_await handler.onComplete();
        if (!handler) {
          // throw http::error{"unexpected EOF"};
        }

        if (client.isActive()) {
          co_await client.shutdown();
        }
      } else {
        while (true) {
          http::parser<http::request> parser;

          client.readStart([&](auto chunk, auto error) {
            if (error) {
              parser.close();
            } else {
              parser.execute(chunk);
            }
          });

          http::request request = co_await parser.onComplete();
          if (!parser) {
            throw http::error{"unexpected EOF"};
          }

          http::response response;
          co_await callback(request, response);

          request.headers["connection"] = "close"; // TODO: fix
          response.headers["connection"] = "close"; // TODO: fix

          if (client.isActive()) {
            co_await client.write((std::string)response);

            if (request.headers["connection"] == "close") {
              co_await client.shutdown();
              break;
            }
          } else {
            break;
          }
        }
      }
    });
  });

  // auto amain_task = amain();
  // amain_task.start_owned();

  uv::run();

  // if (amain_task.done()) {
  //   amain_task.unpack();
  // }

  return 0;
}
