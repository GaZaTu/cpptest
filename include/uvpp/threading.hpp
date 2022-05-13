#pragma once

#include "./error.hpp"
#include "uv.h"
#include <functional>

namespace uv {
struct thread {
public:
  struct data {
    std::function<void()> entry;

    data(std::function<void()>& e);
  };

  thread(std::function<void()> entry);

  void join();

  bool operator==(thread& other);

private:
  uv_thread_t* _native;
};

struct rwlock {
public:
  struct read {
  public:
    read(rwlock& l);

    ~read();

  private:
    rwlock& _l;
  };

  struct write {
  public:
    write(rwlock& l);

    ~write();

  private:
    rwlock& _l;
  };

  friend read;
  friend write;

  rwlock();

  ~rwlock();

private:
  uv_rwlock_t* _native;
};
} // namespace uv
