#ifdef HTTPPP_HTTP2
#include "http/http2.hpp"

namespace http::_2 {
using request_parser = http::_2::handler<http::request>;
using response_parser = http::_2::handler<http::response>;
} // namespace http
#endif
