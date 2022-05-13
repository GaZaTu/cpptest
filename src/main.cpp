#define USE_SQLITE 1
// #define USE_PGSQL 1

#include "logging.hpp"
#include "db.hpp"
#ifdef USE_SQLITE
#include "db/sqlite.hpp"
#include "db/sqlite-createftssynctriggers.h"
#include "db/sqlite-createisotimestamptriggers.h"
#include "db/sqlite-sanitizewebsearch.h"
#endif
#ifdef USE_PGSQL
#include "db/pgsql.hpp"
#endif
#include "ulid_uint128.hh"
#include <iostream>
#include <regex>
#include "json.hpp"
#include "finally.hpp"
#include "uv.hpp"
#include "http/base64.hpp"
#include <algorithm>
#include <random>
#include "http.hpp"
#include "http/serve.hpp"
#include "http/websocket.hpp"
#include <set>
#include "irc/twitch-bot.hpp"

#include <cmrc/cmrc.hpp>
CMRC_DECLARE(res);

#define __CPP20 202002L
#define __CPP17 201703L
#define __CPP14 201402L
#define __CPP11 201103L

#define __IS_CLANG false
#define __IS_GCC false
#define __IS_MSVC false

#if defined(__clang__)
#undef __IS_CLANG
#define __IS_CLANG true
#elif defined(__GNUC__)
#undef __IS_GCC
#define __IS_GCC true
#elif defined(_MSC_VER)
#undef __IS_MSVC
#define __IS_MSVC true
#endif

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

std::string sql_expand_macros(std::istream& in, const std::unordered_map<std::string, std::string>& defines) {
  std::string_view IFDEF = "--#ifdef";
  std::string_view ELIF = "--#elif";
  std::string_view ELSE = "--#else";
  std::string_view ENDIF = "--#endif";

  std::string result;

  int ifdef_depth = 0;
  bool ifdef_valid = true;
  bool ifdef_was_valid = false;

  for (std::string line; std::getline(in, line, '\n'); ) {
    if (line.empty()) {
      result += '\n';
    } else if (line.starts_with(IFDEF)) {
      ifdef_depth += 1;
      ifdef_valid = defines.count(line.substr(IFDEF.length() + 1)) != 0;
    } else if (line.starts_with(ELIF)) {
      if (ifdef_depth > 0 && !ifdef_valid && !ifdef_was_valid) {
        ifdef_valid = defines.count(line.substr(ELIF.length() + 1)) != 0;
      } else {
        ifdef_valid = false;
        ifdef_was_valid = true;
      }
    } else if (line.starts_with(ELSE)) {
      if (ifdef_depth > 0 && !ifdef_valid && !ifdef_was_valid) {
        ifdef_valid = true;
      } else {
        ifdef_valid = false;
        ifdef_was_valid = true;
      }
    } else if (line.starts_with(ENDIF)) {
      ifdef_depth -= 1;
      ifdef_valid = true;
      ifdef_was_valid = false;
    } else if (ifdef_valid) {
      result += line + '\n';
    }
  }

  return result;
}

std::string sql_expand_macros(std::string_view in, const std::unordered_map<std::string, std::string>& defines) {
  std::stringstream stream{std::string{in}};
  return sql_expand_macros(stream, defines);
}

auto registerDatabaseUpgradeMacros(db::connection& connection) {
#ifdef USE_SQLITE
  db::sqlite::loadExtension(connection, sqlite3_createisotimestamptriggers_init);
  db::sqlite::loadExtension(connection, sqlite3_createftssynctriggers_init);
#endif

  return finally{[&connection]() {
#ifdef USE_SQLITE
    db::sqlite::loadExtension(connection, sqlite3_createftssynctriggers_deinit);
    db::sqlite::loadExtension(connection, sqlite3_createisotimestamptriggers_deinit);
#endif
  }};
}

void registerDatabaseUpgradeScripts(db::datasource& datasource) {
  for (auto entry : cmrc::res::get_filesystem().iterate_directory("src/res/sql/upgrades")) {
    auto filename = entry.filename();
    auto version = std::stoi(filename.substr(1, 5));

    datasource.updates().push_back({version,
        [path{"src/res/sql/upgrades/" + filename}](db::connection& connection) {
          auto file = cmrc::res::get_filesystem().open(path);
          auto data = std::string_view{file.begin(), file.size()};

          auto script = sql_expand_macros(data, {
#ifdef USE_SQLITE
            {"USE_SQLITE", "1"},
#endif
#ifdef USE_PGSQL
            {"USE_PGSQL", "1"},
#endif
          });

          auto macros = registerDatabaseUpgradeMacros(connection);
          connection.execute(script);
        },
        [path{"src/res/sql/downgrades/" + filename}](db::connection& connection) {
          auto file = cmrc::res::get_filesystem().open(path);
          auto data = std::string_view{file.begin(), file.size()};

          auto script = sql_expand_macros(data, {
#ifdef USE_SQLITE
            {"USE_SQLITE", "1"},
#endif
#ifdef USE_PGSQL
            {"USE_PGSQL", "1"},
#endif
          });

          auto macros = registerDatabaseUpgradeMacros(connection);
          connection.execute(script);
        }});
  }
}

using json = nlohmann::json;

namespace nlohmann {
template <typename T>
struct adl_serializer<std::optional<T>> {
  static void from_json(const json& j, std::optional<T>& opt) {
    if (!j.is_null()) {
      opt = j.get<T>();
    } else {
      opt = std::nullopt;
    }
  }

  static void to_json(json& j, const std::optional<T>& opt) {
    if (opt) {
      j = *opt;
    } else {
      j = nullptr;
    }
  }
};
}

// task<std::vector<TriviaQuestionJson>> getGazatuXyzQuestions(std::string_view count = "0", std::string_view config = "") {
//   http::url url{"https://api.gazatu.xyz/trivia/questions"};
//   // url.query({
//   //   {"verified", "true"},
//   //   {"count", (std::string)count},
//   // });
//   // if (!config.empty()) {
//   //   url.query(url.query() + std::string{"&"} + (std::string)config);
//   // }

//   auto response = co_await http::fetch(url);
//   std::cout << (std::string)response << std::endl;

//   auto json = json::parse(response.body);

//   co_return json;
// }

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

  static ulid::ULID fromString(std::string_view string) {
    return ulid::Unmarshal(string);
  }
};
}

namespace nlohmann {
template <>
struct adl_serializer<ulid::ULID> {
  static void from_json(const json& j, ulid::ULID& value) {
    value = ulid::Unmarshal(j.get<std::string>());
  }

  static void to_json(json& j, const ulid::ULID& value) {
    j = ulid::Marshal(value);
  }
};
}

std::ostream& operator<<(std::ostream& os, ulid::ULID v) {
  os << ulid::Marshal(v);

  return os;
}

std::vector<std::string_view> string_split(std::string_view string, const std::regex& regex) {
  std::vector<std::string_view> result;

  std::cregex_token_iterator iter{string.begin(), string.end(), regex, -1};
  for (std::cregex_token_iterator end; iter != end; ++iter) {
    result.push_back({iter->first, (std::string_view::size_type)iter->length()});
  }

  return result;
}

namespace nlohmann {
template <>
struct adl_serializer<db::statement::parameter_access> {
  static void from_json(const json& j, db::statement::parameter_access& params) {
    if (!j.is_object()) {
      return;
    }

    for (const auto& [key, value] : j.items()) {
      switch (value.type()) {
        case json::value_t::null:
          params[key] = std::nullopt;
          break;
        case json::value_t::string:
          params[key] = (std::string)value;
          break;
        case json::value_t::boolean:
          params[key] = (bool)value;
          break;
        case json::value_t::number_integer:
          params[key] = (int64_t)value;
          break;
        case json::value_t::number_unsigned:
          params[key] = (uint64_t)value;
          break;
        case json::value_t::number_float:
          params[key] = (double)value;
          break;
      }
    }
  }
};

template <>
struct adl_serializer<db::resultset> {
  static void to_json(json& j, const db::resultset& resultset) {
    for (auto column : resultset.columns()) {
      switch (resultset.type(column)) {
        case db::orm::field_type::UNKNOWN:
          j[(std::string)column] = nullptr;
          break;
        case db::orm::field_type::BOOLEAN:
          j[(std::string)column] = resultset.get<bool>(column);
          break;
        case db::orm::field_type::INT32:
          j[(std::string)column] = resultset.get<int32_t>(column);
          break;
        case db::orm::field_type::INT64:
          j[(std::string)column] = resultset.get<int64_t>(column);
          break;
        case db::orm::field_type::DOUBLE:
          j[(std::string)column] = resultset.get<double>(column);
          break;
        case db::orm::field_type::STRING:
        case db::orm::field_type::DATE:
        case db::orm::field_type::TIME:
        case db::orm::field_type::DATETIME:
          j[(std::string)column] = resultset.get<std::string>(column);
          break;
        case db::orm::field_type::BLOB:
          j[(std::string)column] = base64::encode(resultset.value<std::vector<uint8_t>>(column));
          break;
      }
    }
  }
};
}

struct TriviaCategory {
  ulid::ULID id = 0;
  std::string name;
  std::optional<std::string> description;
  std::optional<std::string> submitter;
  std::optional<ulid::ULID> submitterUserId;
  bool verified = false;
  bool disabled = false;
  std::string createdAt;
  std::string updatedAt;
  std::optional<ulid::ULID> updatedByUserId;
};

DB_ORM_SPECIALIZE(TriviaCategory, id, name, description, submitter, submitterUserId, verified, disabled, createdAt, updatedAt, updatedByUserId);
DB_ORM_PRIMARY_KEY(TriviaCategory, id);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TriviaCategory, id, name, description, submitter, verified, disabled, createdAt, updatedAt);

struct TriviaQuestion {
  ulid::ULID id = 0;
  ulid::ULID categoryId = 0;
  TriviaCategory category{};
  std::string question;
  std::string answer;
  std::optional<std::string> hint1;
  std::optional<std::string> hint2;
  std::optional<std::string> submitter;
  std::optional<ulid::ULID> submitterUserId;
  bool verified = false;
  bool disabled = false;
  std::string createdAt;
  std::string updatedAt;
  std::optional<ulid::ULID> updatedByUserId;
};

DB_ORM_SPECIALIZE(TriviaQuestion, id, categoryId, question, answer, hint1, hint2, submitter, submitterUserId, verified, disabled, createdAt, updatedAt, updatedByUserId);
DB_ORM_PRIMARY_KEY(TriviaQuestion, id);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TriviaQuestion, id, categoryId, category, answer, hint1, hint2, submitter, verified, disabled, createdAt, updatedAt);

#ifdef USE_SQLITE
struct TriviaQuestionFTS {
  uint64_t rowid;
};

DB_ORM_SPECIALIZE(TriviaQuestionFTS, rowid);
DB_ORM_PRIMARY_KEY(TriviaQuestionFTS, rowid);
#endif

struct TriviaReport {
  ulid::ULID id = 0;
  ulid::ULID questionId = 0;
  TriviaQuestion question{};
  std::string message;
  std::string submitter;
  std::optional<ulid::ULID> submitterUserId;
  std::string createdAt;
};

DB_ORM_SPECIALIZE(TriviaReport, id, questionId, message, submitter, submitterUserId, createdAt);
DB_ORM_PRIMARY_KEY(TriviaReport, id);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TriviaReport, id, questionId, message, submitter, createdAt);

#define SearchSortPageOptionsMixin \
  std::optional<std::string> search;\
  std::optional<std::string> orderBy;\
  std::optional<std::string> orderByDirection;\
  std::optional<uint32_t> offset;\
  std::optional<uint32_t> limit;

struct TriviaQuestionSelectOptions {
  SearchSortPageOptionsMixin;

  std::optional<bool> verified = true;
  std::optional<bool> disabled = false;
  std::optional<bool> dangling = false;
  std::optional<bool> reported;

  std::optional<bool> shuffle = false;
};

HTTP_QUERY_PARAMS_SPECIALIZE(TriviaQuestionSelectOptions, search, orderBy, orderByDirection, offset, limit, verified, disabled, dangling, reported, shuffle);

auto selectTriviaQuestions(db::orm::repository& repo, const TriviaQuestionSelectOptions& opts = {}) {
  auto query = repo
    .select()
    .from(_TriviaQuestion{"src"})
    .join(_TriviaCategory{"cat"}).on(_TriviaCategory{"cat"}.id == _TriviaQuestion{"src"}.categoryId);

  if (opts.verified) {
    query
      .where(
        _TriviaQuestion{"src"}.verified == *opts.verified
      );
  }

  if (opts.disabled) {
    query
      .where(
        _TriviaQuestion{"src"}.disabled == *opts.disabled
      );
  }

  if (opts.dangling) {
    query
      .where(
        (
          _TriviaCategory{"cat"}.verified == false ||
          _TriviaCategory{"cat"}.disabled == true
        ) == *opts.dangling
      );
  }

  if (opts.reported) {
    query
      .where(repo
        .select()
        .from(_TriviaReport{"rep"})
        .where(
          _TriviaReport{"rep"}.questionId == _TriviaQuestion{"src"}.id
        )
        .exists(":reported", *opts.reported)
      );
  }

  if (opts.search && *opts.search != "") {
    query
#ifdef USE_SQLITE
      .join(_TriviaQuestionFTS{"fts"}).on("fts.rowid = src.rowid")
      .where("TriviaQuestionFTS MATCH sanitize_web_search(:search)")
      .orderBy("fts.rank", db::orm::ASCENDING)
#endif
#ifdef USE_PGSQL
      .select({{"ts_rank_cd(src.__tsvector, search)", "rank"}})
      .join("plainto_tsquery(:search)", "search").always()
      .where("search @@ src.__tsvector")
      .orderBy("rank", db::orm::DESCENDING)
#endif
      .bind(":search", *opts.search);
  }

  uint32_t offset = opts.offset.value_or(0u);
  uint32_t limit = std::min(opts.limit.value_or(100u), 100u);

  if (opts.shuffle.value_or(false)) {
    auto result = query
      .findMany<TriviaQuestion>()
      .toVector();

    std::random_device rd{};
    std::default_random_engine rng{rd()};
    std::shuffle(std::begin(result), std::end(result), rng);

    if (result.size() > limit) {
      result.resize(limit);
    }

    return result;
  } else {
    auto orderBy = opts.orderBy;
    // TODO: category.name
    if (!orderBy || !_TriviaQuestion::has(*orderBy)) {
      orderBy = _TriviaQuestion{}.updatedAt.name;
    }

    auto orderByDirection = opts.orderByDirection;
    if (!orderByDirection) {
      orderByDirection = "DESC";
    }

    query
      .orderBy("src." + *orderBy, *orderByDirection == "ASC" ? db::orm::ASCENDING : db::orm::DESCENDING)
      .offset(offset)
      .limit(limit);

    return query
      .findMany<TriviaQuestion>()
      .toVector();
  }
}

task<int> amain2() {
  while (true) {
    uv::signalof<SIGINT> sigint;

    irc::twitch::bot bot;

    websocket::handler ws{websocket::IS_CLIENT};

    uv::tcp tcp;

    http::request request = websocket::http::upgrade({
        .url = "wss://irc-ws.chat.twitch.tv",
        // .url = "wss://ws.postman-echo.com/raw",
        // .proxy = {
        //   .host = "proxy-iuk.ofd-h.de",
        //   .port = 8080,
        // },
    });
    http::response response = co_await http::fetch(request, tcp);
    std::cout << (std::string)response << std::endl;

    tcp.readStart([&](auto chunk, auto error) {
      if (error) {
        sigint.cancel();
        return;
      }

      ws.feed(chunk);
    });

    ws.onSend([&](auto chunk) {
      tcp.write(chunk).start();
    });

    ws.onRecv([&](const auto& msg) {
      if (msg.kind == websocket::message::TEXT) {
        bot.feed(msg.payload);
      }
    });

    bot.onSendWithWriter([&](auto line) {
      ws.send(line);
    });

    bot.handle<irc::unknown>([&](const auto& unknown) {
      std::cout << (std::string)unknown << std::endl;
    });

    bot.handle<irc::reconnect>([&](const auto&) {
      sigint.cancel();
    });

    bot.handle<irc::privmsg>([&](const auto& privmsg) {
      std::cout << "#" << privmsg.channel() << " " << privmsg.sender() << ": " << privmsg.message() << std::endl;
    });

    uv::tty ttyin{uv::tty::STDIN};
    ttyin.readLinesAsViews([&](auto line, auto error) {
      if (error) {
        sigint.cancel();
        return;
      }

      if (line.starts_with(".close")) {
        ws.close();
        return;
      }

      if (line.starts_with(".auth")) {
        std::cmatch match;
        std::regex_search(std::begin(line), std::end(line), match, std::regex{"^\\.auth (\\S+) (\\S+)"});
        bot.auth(match[1].str(), match[2].str()).start();
        return;
      }

      if (line.starts_with(".join")) {
        std::cmatch match;
        std::regex_search(std::begin(line), std::end(line), match, std::regex{"^\\.join #(\\w+)"});
        bot.join(match[1].str()).start();
        return;
      }

      if (line.starts_with(".part")) {
        std::cmatch match;
        std::regex_search(std::begin(line), std::end(line), match, std::regex{"^\\.part #(\\w+)"});
        bot.part(match[1].str());
        return;
      }

      std::cmatch match;
      std::regex_search(std::begin(line), std::end(line), match, std::regex{"^#(\\w+) "});
      bot.send(match[1].str(), match.suffix().str()).start();
    });

    if (co_await sigint.once()) {
      break;
    }
  }

  co_return 0;
}

task<int> amain() {
#ifdef USE_SQLITE
  db::sqlite::pooled::datasource datasource{".db"};
#endif
#ifdef USE_PGSQL
  db::pgsql::pooled::datasource datasource{"postgresql://localhost/gazatu_api"};
#endif
  registerDatabaseUpgradeScripts(datasource);

  datasource.onConnectionCreate() = [](db::connection& conn) {
    // conn.profile(std::cerr);
    // db::sqlite::profile(conn, std::cerr);

#ifdef USE_SQLITE
    db::sqlite::loadExtension(conn, sqlite3_sanitizewebsearch_init);
#endif

    db::transaction transaction{conn};
#ifdef USE_SQLITE
    conn.execute("PRAGMA defer_foreign_keys = ON");
#endif
    conn.upgrade();
  };

  datasource.onConnectionClose() = [](db::connection& conn) {
#ifdef USE_SQLITE
    conn.execute("PRAGMA optimize");
#endif
  };

  // db::connection conn{datasource};
  // db::orm::repository repo{conn};

  // conn.execute("DELETE FROM \"TriviaQuestion\"");
  // conn.execute("DELETE FROM \"TriviaCategory\"");

  auto count = db::orm::repository{datasource}
    .count<TriviaQuestion>();

  if (count == 0) {
    auto data = co_await uv::fs::readAll("./questions.json");
    auto json = json::parse(data);

    db::orm::repository repo{datasource};
    db::transaction transaction{repo};
    for (const auto& source : json) {
      auto category = repo
        .findOne<TriviaCategory>(
          _TriviaCategory{}.name == source["category"]
        )
        .value_or(TriviaCategory{
          .name = source["category"],
          .verified = true,
        });

      if (!category.id) {
        repo.save(category);
      }

      TriviaQuestion question{
        .categoryId = category.id,
        .question = source["question"],
        .answer = source["answer"],
        .hint1 = source["hint1"].get<std::optional<std::string>>(),
        .hint2 = source["hint2"].get<std::optional<std::string>>(),
        .submitter = source["submitter"].get<std::optional<std::string>>(),
        .verified = true,
      };

      repo.save(question);
    }
  }

  std::cout << "ready" << std::endl;

  uv::tcp server;
  server.bind4("127.0.0.1", 8001);

  http::serve::listen(server, [&](http::request& request, http::response& response) -> task<void> {
    db::orm::repository repo{datasource};

    TriviaQuestionSelectOptions options;
    http::serve::deserializeQuery(request.url, options);

    auto questions = selectTriviaQuestions(repo, options);

    std::set<ulid::ULID> categoryids;
    for (auto& question : questions) {
      categoryids.emplace(question.categoryId);
    }
    auto categories = repo.findManyById<TriviaCategory>(categoryids).toVector();

    for (auto& question : questions) {
      for (auto& category : categories) {
        if (category.id == question.categoryId) {
          question.category = category;
          break;
        }
      }
    }

    response.headers["content-type"] = "application/json";

    if (request.url.queryvalue<bool>("simplified").value_or(false)) {
      json body;
      for (auto& question : questions) {
        body.push_back({
          {"id", question.id},
          {"category", question.category.name},
          {"question", question.question},
          {"answer", question.answer},
          {"hint1", question.hint1},
          {"hint2", question.hint2},
          {"submitter", question.submitter},
        });
      }

      response.body = body.dump(2, ' ');
    } else {
      json body = questions;
      response.body = body.dump(2, ' ');
    }

    http::serve::normalize(response);
    co_return;
  });

  std::cout << "listening" << std::endl;

  co_await uv::signal::sonce(SIGINT);
  co_return 0;
}

int main() {
  return amain2().start_blocking([]() {
    uv::run();
  });
}
