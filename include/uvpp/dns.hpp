#pragma once

#include "./error.hpp"
#include "./req.hpp"
#include "uv.h"
#ifdef UVPP_TASK_INCLUDE
#include UVPP_TASK_INCLUDE
#endif
#include <functional>
#include <memory>
#include <variant>

namespace uv {
namespace dns {
using addrinfo = std::shared_ptr<::addrinfo>;

void uv_freeaddrinfo2(::addrinfo* addr);

void getaddrinfo(const std::string& node, const std::string& service, std::function<void(uv::dns::addrinfo, uv::error)> cb,
    uv_loop_t* native_loop = uv_default_loop());

#ifdef UVPP_TASK_INCLUDE
task<uv::dns::addrinfo> getaddrinfo(
    const std::string& node, const std::string& service, uv_loop_t* native_loop = uv_default_loop());
#endif
} // namespace dns
} // namespace uv
