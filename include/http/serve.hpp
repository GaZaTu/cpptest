#pragma once

#ifdef HTTPPP_TASK_INCLUDE
#include "../uv.hpp"
#include HTTPPP_TASK_INCLUDE
#ifdef HTTPPP_SSL_DRIVER_INCLUDE
#include HTTPPP_SSL_DRIVER_INCLUDE
#endif
#include "./gzip.hpp"
#include "./http1-serve.hpp"
#include "./http2-serve.hpp"

namespace http::serve {
void listen(uv::tcp& server, http::serve::handler&& _callback);

// struct ssl_config {
// public:
//   std::string private_key_file;
//   std::string certificate_file;
//   std::string certificate_chain_file;
// };

// inline void useSSL(uv::tcp& server, ssl_config config) {
// #ifdef HTTPPP_SSL_DRIVER_INCLUDE
//   HTTPPP_SSL_DRIVER_TYPE ssl_driver;
//   ssl::context ssl_context{ssl_driver, ssl::ACCEPT};
//   if (!config.private_key_file.empty()) {
//     ssl_context.usePrivateKeyFile(config.private_key_file.data());
//   }
//   if (!config.certificate_file.empty()) {
//     ssl_context.useCertificateFile(config.certificate_file.data());
//   }
//   if (!config.certificate_chain_file.empty()) {
//     ssl_context.useCertificateChainFile(config.certificate_chain_file.data());
//   }
// #ifdef HTTPPP_HTTP2
//   ssl_context.useALPNCallback({"h2", "http/1.1"});
// #else
//   ssl_context.useALPNCallback({"http/1.1"});
// #endif
// #endif

// #if defined(UVPP_SSL_INCLUDE) && defined(HTTPPP_SSL_DRIVER_INCLUDE)
//   if (!config.private_key_file.empty()) {
//     server.useSSL(ssl_context);
//   }
// #endif
// }

inline void normalize(http::response& response) {
#ifdef HTTPPP_ZLIB
  if (response.headers.count("content-encoding") == 0) {
    if (request.headers["accept-encoding"].find("gzip") != std::string::npos) {
      response.headers["content-encoding"] = "gzip";
      http::gzip::compress(response.body);
    }
  }
#endif

  response.headers["content-length"] = std::to_string(response.body.length());
}

namespace detail {
template <typename T>
struct query_params_meta {
  static constexpr bool specialized = false;
};

template <typename T>
struct decay_std_optional {
  using type = T;

  static constexpr bool specialized = false;
};

template <typename T>
struct decay_std_optional<std::optional<T>> {
  using type = T;

  static constexpr bool specialized = true;
};
} // namespace detail

template <typename T>
void deserializeQuery(const http::url& url, T& result) {
  http::serve::detail::query_params_meta<T>::deserialize(url, result);
}
} // namespace http::serve
#endif

#define HTTP_QUERY_PARAMS_DESERIALIZE_FIELD(FIELD)                                   \
  {                                                                                  \
    using decayed = http::serve::detail::decay_std_optional<decltype(result.FIELD)>; \
    auto value = url.queryvalue<typename decayed::type>(#FIELD);                     \
    if constexpr (decayed::specialized) {                                            \
      if (value) {                                                                   \
        result.FIELD = value;                                                        \
      }                                                                              \
    } else {                                                                         \
      result.FIELD = value.value();                                                  \
    }                                                                                \
  }

#define HTTP_QUERY_PARAMS_SPECIALIZE(TYPE, FIELDS...)             \
  namespace http::serve::detail {                                 \
  template <>                                                     \
  struct query_params_meta<TYPE> {                                \
    using class_type = TYPE;                                      \
                                                                  \
    static constexpr bool specialized = true;                     \
                                                                  \
    static void deserialize(const http::url& url, TYPE& result) { \
      FOR_EACH(HTTP_QUERY_PARAMS_DESERIALIZE_FIELD, FIELDS);      \
    }                                                             \
  };                                                              \
  }
