#ifdef HTTPPP_TASK_INCLUDE
#ifdef HTTPPP_HTTP2
#include "http/http2-serve.hpp"

namespace http::_2 {
task<void> accept(uv::tcp& client, const http::serve::handler& callback) {
  http::_2::handler<http::request> handler;

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

      auto on_sent = handler.submitResponse(id, _response);
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
}
}
#endif
#endif
