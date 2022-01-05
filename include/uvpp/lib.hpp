#pragma once

#include "./error.hpp"
#include "uv.h"
#include <memory>
#include <string>
#include <functional>

namespace uv {
struct lib {
public:
  lib(const std::string& name) {
    _native = {new uv_lib_t(), [](uv_lib_t* ptr) {
      uv_dlclose(ptr);
      delete ptr;
    }};

    if (uv_dlopen(name.data(), &*_native) != 0) {
      throw error{uv_dlerror(&*_native)};
    }
  }

  lib() = delete;

  lib(const lib&) = delete;

  // template <typename S>
  // std::function<S> symbol(const std::string& name) {
  //   throw error{"unsupported"};
  // }

  // template <typename R, typename ...A>
  // std::function<R(A...)> symbol<R(A...)>(const std::string& name) {
  //   return [_native, ptr{_symbol<R (*)(A...)>(name)}](A... args) {
  //     return ptr(args...);
  //   };
  // }

private:
  std::shared_ptr<uv_lib_t> _native;

  // template <typename F>
  // F _symbol(const std::string& name) {
  //   F ptr;
  //   if (uv_dlsym(&*_native, name.data(), (void**)&ptr) != 0) {
  //     throw error{uv_dlerror(&*_native)};
  //   }

  //   return ptr;
  // }
};
}
