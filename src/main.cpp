#include "http.hpp"
#include "irc.hpp"
#include "irc/twitch.hpp"
#include "ssl-openssl.hpp"
#include "uv.hpp"
#include <iostream>

// https://github.com/nghttp2/nghttp2
// https://nghttp2.org/documentation/tutorial-client.html

task<void> noop() {
  return {};
}

task<void> xdxd(int i) {
  auto test = co_await uv::work::queue<int>([]() {
    return 123;
  });

  throw 123;

  std::cout << co_await uv::fs::readAll(".editorconfig") << std::endl << test << i << std::endl;
}

task<void> amain() {
  // auto start = uv::hrtime();

  // // http::request request;
  // // request.url = "http://www.google.com";

  // http::response response = co_await http::fetch("https://api.gazatu.xyz");
  // if (!response) {
  //   // throw http::error{response};
  // }

  // // std::cout << (std::string)request << std::endl;
  // std::cout << (std::string)response << std::endl;

  // auto end = uv::hrtime();

  // std::cout << "request done: " << ((end - start) * 1e-6) << " ms" << std::endl;

  // co_return;

  // xdxd(42).start(uv::deleteTask);

  // co_await noop();

  // std::cout << co_await task<int>::resolve(121212) << std::endl;

  ssl::openssl::driver opensslDriver;
  ssl::context sslContext{opensslDriver};

  while (true) {
    std::cout << "CONNECT" << std::endl;

    std::string nick = co_await uv::fs::readAll(".irc-nick");
    std::string pass = co_await uv::fs::readAll(".irc-pass");

    uv::tcp tcp;
    tcp.useSSL(sslContext);
    co_await tcp.connect("irc.chat.twitch.tv", "6697");

    co_await tcp.write(irc::out::auth(nick, pass));
    // co_await tcp.write(irc::out::twitch::capreqMembership());
    co_await tcp.write(irc::out::twitch::capreqTags());
    co_await tcp.write(irc::out::twitch::capreqCommands());
    co_await tcp.write(irc::out::join("pajlada"));

    co_await tcp.readLinesAsViewsUntilEOF([&tcp](auto line) {
      uv::startAsTask([&]() -> task<void> {
        irc::message message = irc::parse(line);

        switch (message.index()) {
        case irc::index_v<irc::privmsg>: {
          auto& privmsg = irc::get<irc::privmsg>(message);

          std::cout << privmsg[irc::twitch::tags::DISPLAY_NAME] << ": " << privmsg.message() << std::endl;

          if (
            privmsg.channel() == "pajlada" &&
            privmsg.sender() == "pajbot" &&
            privmsg.message().starts_with("pajaS 🚨 ALERT") &&
            privmsg.action()
          ) {
            co_await tcp.write(irc::out::privmsg(privmsg.channel(), "/me FeelsDonkMan 🚨 TERROREM"));
          }
        } break;

        case irc::index_v<irc::ping>: {
          auto& ping = irc::get<irc::ping>(message);

          // std::cout << (std::string_view)ping << std::endl;
          co_await tcp.write(irc::out::pong(ping.server()));
        } break;

        default: {
          // std::cout << "<< " << irc::getAsString(message) << std::endl;
        } break;
        }
      });
    });
  }

  // auto test = co_await uv::work::queue<int>([]() {
  //   return 123;
  // });

  // std::cout << co_await uv::fs::readAll(".editorconfig") << std::endl << test << std::endl;

  // uv::tty ttyin(uv::tty::STDIN);
  // uv::tty ttyout(uv::tty::STDOUT);
  // co_await ttyin.readLinesAsViewsUntilEOF([&](auto line) {
  //   uv::startAsTask([&]() -> task<void> {
  //     auto start = uv::hrtime();

  //     http::response response = co_await http::fetch(line);
  //     if (!response) {
  //       // throw http::error{response};
  //     }

  //     std::cout << (std::string)response << std::endl;

  //     auto end = uv::hrtime();

  //     std::cout << "request done: " << ((end - start) * 1e-6) << " ms" << std::endl;
  //   });
  // });
}

int main() {
  // ssl::openssl::driver opensslDriver;
  // ssl::context sslContext{opensslDriver, ssl::ACCEPT};
  // sslContext.usePrivateKeyFile("server.key");
  // sslContext.useCertificateFile("server.crt");
  // sslContext.useALPNCallback({"http/1.1"});

  // uv::tcp server;
  // server.useSSL(sslContext);
  // server.bind4("127.0.0.1", 8001);
  // server.listen([&](auto error) {
  //   uv::startAsTask([&]() -> task<void> {
  //     uv::tcp client;
  //     co_await server.accept(client);

  //     if (client.sslState().protocol() == "h2") {
  //       http2::handler<http::request> handler;

  //       client.readStart([&](auto chunk, auto error) {
  //         if (error) {
  //           handler.close();
  //         } else {
  //           handler.execute(chunk);
  //         }
  //       });

  //       http::request request = co_await handler.complete();
  //       if (!handler) {
  //         throw http::error{"unexpected EOF"};
  //       }

  //       std::cout << (std::string)request << std::endl;

  //       co_await client.shutdown();
  //     } else {
  //       http::parser<http::request> parser;

  //       client.readStart([&](auto chunk, auto error) {
  //         if (error) {
  //           parser.close();
  //         } else {
  //           parser.execute(chunk);
  //         }
  //       });

  //       http::request request = co_await parser.complete();
  //       if (!parser) {
  //         throw http::error{"unexpected EOF"};
  //       }

  //       // std::cout << (std::string)request << std::endl;

  //       http::response response;
  //       response.headers["connection"] = "close";
  //       response.headers["content-type"] = "text/plain";
  //       response.body = "Hello World!";

  //       if (request.headers["accept-encoding"].find("gzip") != std::string::npos) {
  //         response.headers["content-encoding"] = "gzip";
  //         http::compress(response.body);
  //       }

  //       co_await client.write((std::string)response);
  //       co_await client.shutdown();
  //     }
  //   });
  // });

  auto amain_task = amain();
  amain_task.start();

  uv::run();

  if (amain_task.done()) {
    amain_task.unpack();
  }

  return 0;
}

// int main() {
//   db::sqlite::datasource datasource(":memory:");

//   datasource.onConnectionOpen([](db::connection& connection) {
//     try {
//       db::transaction transaction(connection);

//       connection.execute("CREATE TABLE Test (firstname, lastname, age)");
//       connection.execute("INSERT INTO Test (firstname, lastname, age) VALUES ('firstname1', 'lastname1', 12)");
//     } catch (...) {
//       throw;
//     }
//   });

//   datasource.onConnectionClose([](db::connection& connection) {
//     connection.execute("PRAGMA optimize");
//   });

//   try {
//     db::connection connection(&datasource);
//     db::statement statement(connection);

//     statement.prepare(
//         "SELECT age, lastname FROM Test WHERE age >= :minAge AND lastname = :lastname AND :ignore IS NULL");
//     statement.params[":minAge"] = 12;
//     statement.params[":lastname"] = "lastname1";
//     statement.params[":ignore"] = std::nullopt;

//     for (db::resultset& result : statement.execute()) {
//       std::optional<int> age = result["age"];
//       std::optional<std::string> lastname = result["lastname"];

//       std::cout << "age = " << age.value_or(-1) << std::endl;
//       std::cout << "lastname = " << lastname.value_or("null") << std::endl;

//       for (const std::string& col : result.columns()) {
//         std::cout << "col -> " << col << std::endl;
//       }
//     }

//     statement.prepare("SELECT 3");
//     std::cout << "main::executeAndGetSingle = " << statement.executeAndGetSingle<int>().value() << std::endl;
//   } catch (...) {
//     throw;
//   }

//   return 0;
// }
