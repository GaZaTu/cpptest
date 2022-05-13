#include "http/fetch.hpp"
#include "http/http1.hpp"
#include "http/http2.hpp"
#include "http/gzip.hpp"
#include "uvpp/async.hpp"
#include "uvpp/tcp.hpp"
#ifdef HTTPPP_SSL_DRIVER_INCLUDE
#include HTTPPP_SSL_DRIVER_INCLUDE
#endif

namespace http {
#ifdef HTTPPP_TASK_INCLUDE
HTTPPP_TASK_TYPE<http::response> fetch(http::request& request, uv::tcp& tcp) {
  std::optional<http::request> proxy_request;
  if (!request.proxy.host.empty()) {
    proxy_request = http::request{
      .method = http::method::CONNECT,
      .url = request.url,
    };
    proxy_request->headers["host"] = request.url.host();
    if (!request.headers.count("upgrade")) {
#ifdef HTTPPP_HTTP2
      proxy_request->headers["alpn"] = "h2, http/1.1";
#else
      proxy_request->headers["alpn"] = "http/1.1";
#endif
    }
    if (!request.proxy.auth.empty()) {
      proxy_request->headers["proxy-authorization"] = request.proxy.auth;
    }
  }

#ifdef HTTPPP_SSL_DRIVER_TYPE
  HTTPPP_SSL_DRIVER_TYPE openssl_driver;
  ssl::context ssl_context{openssl_driver};
  if (!request.headers.count("upgrade")) {
#ifdef HTTPPP_HTTP2
    ssl_context.useALPNProtocols({"h2", "http/1.1"});
#else
    ssl_context.useALPNProtocols({"http/1.1"});
#endif
  }
#endif

  auto sslHandshake = [&]() -> task<void> {
    if (request.url.schema() == "https" || request.url.schema() == "wss") {
#if defined(UVPP_SSL_INCLUDE) && defined(HTTPPP_SSL_DRIVER_INCLUDE)
      tcp.useSSL(ssl_context);
      co_await tcp.handshake();
      co_return;
#else
      throw http::error{"https support not enabled"};
#endif
    }
  };

  if (proxy_request) {
    co_await tcp.connect(request.proxy.host, request.proxy.port);
  } else {
    co_await tcp.connect(request.url.host(), request.url.port());
    co_await sslHandshake();
  }

  if (!request.headers.count("upgrade")) {
    request.headers["connection"] = "close";

#ifdef HTTPPP_ZLIB
    request.headers["accept-encoding"] = "gzip";
#endif
  }

  if (!request.body.empty()) {
#ifdef HTTPPP_ZLIB
    request.headers["content-encoding"] = "gzip";
    http::gzip::compress(request.body);
#endif

    request.headers["content-length"] = std::to_string(request.body.length());
  }

  if (proxy_request) {
    std::cout << (std::string)*proxy_request << std::endl;
    co_await tcp.write((std::string)*proxy_request);

    http::_1::parser<http::response> proxy_parser;

    co_await tcp.readStartUntilEOF([&](auto chunk) {
      proxy_parser.execute(chunk);
      tcp.readStop();
    });

    auto proxy_response = std::move(proxy_parser.result());
    if (!proxy_response) {
      co_return proxy_response;
    }

    co_await sslHandshake();
  }

#ifdef UVPP_SSL_INCLUDE
  std::string_view protocol = tcp.sslState().protocol();
#else
  std::string_view protocol = "";
#endif

  http::response response;

  if (protocol == "h2") {
#ifdef HTTPPP_HTTP2
    http::_2::handler<http::response> handler;

    handler.onSend([&](auto input) {
      tcp.write((std::string)input).start();
    });
    handler.onStreamEnd([&](auto, auto&&) {
      tcp.readStop();
      // uv::async::ssend([&]() {
      //   tcp.readStop();
      // });
    });

    handler.submitSettings();
    auto on_sent = handler.submitRequest(request);
    handler.sendSession();
    auto id = co_await on_sent;

    co_await tcp.readStartUntilEOF([&](auto chunk) {
      handler.execute(chunk);
    });

    response = std::move(handler.result()[id]);
#endif
  } else {
    request.headers["host"] = request.url.host();

    std::cout << (std::string)request << std::endl;
    co_await tcp.write((std::string)request);

    http::_1::parser<http::response> parser;

    tcp.readStart([&](auto chunk, auto error) {
      if (error) {
        parser.fail(std::make_exception_ptr(error));
      } else {
        parser.execute(chunk);
      }
    });

    response = co_await parser.onComplete();
  }

#ifdef HTTPPP_ZLIB
  if (response.headers["content-encoding"] == "gzip") {
    http::gzip::uncompress(response.body);
  }
#endif

  co_return response;
}

HTTPPP_TASK_TYPE<http::response> fetch(http::request& request) {
  uv::tcp tcp;
  co_return co_await fetch(request, tcp);
}

// HTTPPP_TASK_TYPE<http::response> fetch(const http::request& request) {
//   auto _request = request;
//   co_return co_await fetch(_request);
// }

HTTPPP_TASK_TYPE<http::response> fetch(http_method m, http::url&& u, std::string&& b) {
  http::request request{
    .method = m,
    .url = std::move(u),
    .body = std::move(b),
  };
  co_return co_await fetch(request);
}

HTTPPP_TASK_TYPE<http::response> fetch(http_method m, http::url&& u, const std::string& b) {
  http::request request{
    .method = m,
    .url = std::move(u),
    .body = b,
  };
  co_return co_await fetch(request);
}

HTTPPP_TASK_TYPE<http::response> fetch(http::url&& u) {
  http::request request{
    .url = std::move(u),
  };
  co_return co_await fetch(request);
}
#endif
} // namespace http
