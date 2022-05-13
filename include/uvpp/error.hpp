#pragma once

#include "uv.h"
#include <stdexcept>
#include <string>

namespace uv {
class error : public std::runtime_error {
public:
  int code;

  error(const std::string& msg);

  error(int code = 0);

  operator bool();

  bool operator==(int code);

  static void test(int code);

private:
  static const char* strerror(int code);
};
} // namespace uv
