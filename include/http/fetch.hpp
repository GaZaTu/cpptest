#pragma once

#include "../uvpp/async.hpp"
#include "../uvpp/tcp.hpp"
#include "./http1.hpp"
#include "./http2.hpp"
#ifndef UVPP_NO_SSL
#include "../ssl-openssl.hpp"
#endif

namespace http {
task<http::response> fetch(http::request& request) {
#ifndef UVPP_NO_SSL
  ssl::openssl::driver openssl_driver;
  ssl::context ssl_context{openssl_driver};
  ssl_context.useALPNProtocols({"h2", "http/1.1"});
#endif

  uv::tcp tcp;

  if (request.url.schema() == "https") {
#ifndef UVPP_NO_SSL
    tcp.useSSL(ssl_context);
#else
    throw std::runtime_error{"https support disabled"};
#endif
  }

  co_await tcp.connect(request.url.host(), request.url.port());

  request.headers["connection"] = "close";
  request.headers["accept-encoding"] = "gzip";

  if (tcp.sslState().protocol() == "h2") {
    http2::handler<http::response> handler;

    handler.onSend([&](auto input) {
      tcp.write((std::string)input).start();
    });
    handler.onStreamEnd([&](auto, auto&&) {
      uv::async::queue([&]() {
        tcp.readStop();
      });
    });

    handler.submitSettings();
    auto id = handler.submitRequest(request);
    handler.sendSession();

    co_await tcp.readStartUntilEOF([&](auto chunk) {
      handler.execute(chunk);
    });

    if (!handler) {
      // throw http::error{"unexpected EOF"};
    }

    auto response = std::move(handler.result()[id]);
    co_return response;
  } else {
    http::parser<http::response> parser;

    request.headers["host"] = request.url.host();

    co_await tcp.write((std::string)request);
    co_await tcp.shutdown();

    co_await tcp.readStartUntilEOF([&](auto chunk) {
      parser.execute(chunk);
    });

    if (!parser) {
      throw http::error{"unexpected EOF"};
    }

    auto response = std::move(parser.result());
    co_return response;
  }
}

task<http::response> fetch(http_method m, http::url u, std::string b = {}) {
  http::request request{m, u, b};
  co_return co_await fetch(request);
}

task<http::response> fetch(http::url u) {
  http::request request{u};
  co_return co_await fetch(request);
}
} // namespace http
