#pragma once

#include "./common.hpp"
#include "uvpp/tcp.hpp"
#ifdef HTTPPP_TASK_INCLUDE
#include HTTPPP_TASK_INCLUDE
#endif

namespace http {
#ifdef HTTPPP_TASK_INCLUDE
HTTPPP_TASK_TYPE<http::response> fetch(http::request& request, uv::tcp& tcp);

HTTPPP_TASK_TYPE<http::response> fetch(http::request& request);

// HTTPPP_TASK_TYPE<http::response> fetch(const http::request& request);

HTTPPP_TASK_TYPE<http::response> fetch(http_method m, http::url&& u, std::string&& b = {});

HTTPPP_TASK_TYPE<http::response> fetch(http_method m, http::url&& u, const std::string& b);

HTTPPP_TASK_TYPE<http::response> fetch(http::url&& u);
#endif
} // namespace http
