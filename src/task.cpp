#include "task.hpp"
#include <queue>
#include <mutex>
#include <shared_mutex>

namespace taskpp {
namespace detail {
std::queue<std::function<void()>> completed_tasks;
std::shared_mutex completed_tasks_rwlock;

void deleteTask(std::function<void()> deleter) {
  std::unique_lock lock{detail::completed_tasks_rwlock};

  while (!detail::completed_tasks.empty()) {
    auto& existing_deleter = detail::completed_tasks.front();

    existing_deleter();
    detail::completed_tasks.pop();
  }

  detail::completed_tasks.push(std::move(deleter));
}
}
}
