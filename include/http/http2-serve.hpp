#pragma once

#ifdef HTTPPP_TASK_INCLUDE
#ifdef HTTPPP_HTTP2
#include "./http2.hpp"
#include "../uv.hpp"
#include HTTPPP_TASK_INCLUDE

namespace http::_2 {
task<void> accept(uv::tcp& client, const http::serve::handler& callback);
}
#endif
#endif
