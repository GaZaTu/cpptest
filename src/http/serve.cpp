#ifdef HTTPPP_TASK_INCLUDE
#include "http/serve.hpp"

namespace http::serve {
void listen(uv::tcp& server, http::serve::handler&& callback) {
  server.listen([&server, callback{std::move(callback)}](auto error) {
    task<>::run([&]() -> task<void> {
      uv::tcp client;
      co_await server.accept(client);

#ifdef UVPP_SSL_INCLUDE
      std::string_view protocol = client.sslState().protocol();
#else
      std::string_view protocol = "";
#endif

      if (protocol == "h2") {
#ifdef HTTPPP_HTTP2
        co_await http::_2::accept(client, callback);
#endif
      } else {
        co_await http::_1::accept(client, callback);
      }
    });
  });
}
}
#endif
