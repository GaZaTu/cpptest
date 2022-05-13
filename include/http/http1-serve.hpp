#pragma once

#ifdef HTTPPP_TASK_INCLUDE
#include "./http1.hpp"
#include "../uv.hpp"
#include HTTPPP_TASK_INCLUDE

#include <iostream>

namespace http::_1 {
task<void> accept(uv::tcp& client, const http::serve::handler& callback);
}
#endif
