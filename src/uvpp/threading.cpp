#include "uvpp/threading.hpp"

namespace uv {
thread::data::data(std::function<void()>& e) : entry(std::move(e)) {
}

thread::thread(std::function<void()> entry) : _native(new uv_thread_t()) {
  error::test(uv_thread_create(
      _native,
      [](void* ptr) {
        auto data_ptr = reinterpret_cast<data*>(ptr);
        auto entry = std::move(data_ptr->entry);
        delete data_ptr;

        entry();
      },
      new data(entry)));
}

void thread::join() {
  error::test(uv_thread_join(_native));
}

bool thread::operator==(thread& other) {
  return uv_thread_equal(_native, other._native) != 0;
}

rwlock::read::read(rwlock& l) : _l(l) {
  uv_rwlock_rdlock(_l._native);
}

rwlock::read::~read() {
  uv_rwlock_rdunlock(_l._native);
}

rwlock::write::write(rwlock& l) : _l(l) {
  uv_rwlock_wrlock(_l._native);
}

rwlock::write::~write() {
  uv_rwlock_wrunlock(_l._native);
}

rwlock::rwlock() : _native(new uv_rwlock_t()) {
  error::test(uv_rwlock_init(_native));
}

rwlock::~rwlock() {
  uv_rwlock_destroy(_native);
  delete _native;
}
} // namespace uv
