#ifdef HTTPPP_TASK_INCLUDE
#include "http/http1-serve.hpp"

namespace http::_1 {
task<void> accept(uv::tcp& client, const http::serve::handler& callback) {
  auto sockname = client.sockname();
  auto peername = client.peername();

  while (true) {
    http::_1::parser<http::request> parser;

    client.readStart([&](auto chunk, auto error) {
      if (error) {
        parser.close();
      } else {
        parser.execute(chunk);
      }
    });

    http::request request = co_await parser.onComplete();
    if (!parser) {
      break;
    }

    request.headers[":peername"] = peername;
    if (request.headers[":peername"] == sockname) {
      if (request.headers.count("x-forwarded-for") != 0) {
        request.headers[":peername"] = request.headers["x-forwarded-for"];
      }
    }

    http::response response;
    co_await callback(request, response);

    // request.headers["connection"] = "close";
    // response.headers["connection"] = "close";

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
}
#endif
