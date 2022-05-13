#include "uvpp/tty.hpp"

namespace uv {
tty::data::data(uv_tty_t* native_tty) : _native_tty(native_tty) {
}

tty::data::~data() {
  delete _native_tty;
}

tty::tty(file fd, uv_loop_t* native_loop, uv_tty_t* native_tty)
    : stream(native_tty, new data(native_tty)), _native_tty(native_tty) {
  error::test(uv_tty_init(native_loop, native_tty, fd, fd == STDIN));
}

tty::tty(file fd, uv_loop_t* native_loop) : tty(fd, native_loop, new uv_tty_t()) {
}

tty::tty(file fd, uv_tty_t* native_tty) : tty(fd, uv_default_loop(), native_tty) {
}

tty::tty(file fd) : tty(fd, uv_default_loop(), new uv_tty_t()) {
}

tty::tty(tty&& source) noexcept
    : stream(source._native_tty, handle::getData<data>(source._native_tty)),
      _native_tty(std::exchange(source._native_tty, nullptr)) {
}

tty::operator uv_tty_t*() noexcept {
  return _native_tty;
}

tty::operator const uv_tty_t*() const noexcept {
  return _native_tty;
}
} // namespace uv
