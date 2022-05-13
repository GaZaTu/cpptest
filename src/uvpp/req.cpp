#include "uvpp/req.hpp"

namespace uv {
namespace detail {
req::data::~data() noexcept {
}

req::req(uv_req_t* native_req, data* data_ptr) : _native_req(native_req) {
#if (UV_VERSION_MAJOR >= 1) && (UV_VERSION_MINOR >= 34)
  uv_req_set_data(native_req, data_ptr);
#else
  native_req->data = (void*)data_ptr;
#endif
}

req::~req() noexcept {
}

req::operator uv_req_t*() noexcept {
  return _native_req;
}

req::operator const uv_req_t*() const noexcept {
  return _native_req;
}

void req::cancel() {
  error::test(uv_cancel(*this));
}
} // namespace detail
} // namespace uv
