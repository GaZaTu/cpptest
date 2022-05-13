#include "uvpp/error.hpp"

namespace uv {
error::error(const std::string& msg) : std::runtime_error(msg) {
  this->code = -1;
}

error::error(int code) : error(strerror(code)) {
  this->code = code;
}

error::operator bool() {
  return this->code != 0;
}

bool error::operator==(int code) {
  return this->code == code;
}

void error::test(int code) {
  if (code != 0) {
    throw error(code);
  }
}

const char* error::strerror(int code) {
  if (code == -1) {
    return "unknown";
  }

  if (code == 0) {
    return "nothing";
  }

  return uv_strerror(code);
}
} // namespace uv
