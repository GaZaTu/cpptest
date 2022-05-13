#pragma once

#include "./datasource.hpp"
#include <memory>
#include <optional>
#include <string_view>

namespace db {
class connection {
public:
  friend db::statement;
  friend db::transaction;

  friend class db::orm::selector;
  friend class db::orm::inserter;
  friend class db::orm::updater;
  friend class db::orm::deleter;

  explicit connection(db::datasource& dsrc, std::shared_ptr<db::datasource::connection> conn);

  explicit connection(db::datasource& dsrc);

  connection(const db::connection&) = delete;

  connection(db::connection&& src);

  connection& operator=(const db::connection&) = delete;

  connection& operator=(db::connection&&) = delete;

  ~connection();

  void execute(std::string_view script);

  int version();

  void upgrade(int new_version = -1);

  void downgrade(int new_version);

  template <typename T = db::datasource::connection>
  std::shared_ptr<T> getNativeConnection() {
    return std::dynamic_pointer_cast<T>(_datasource_connection);
  }

  void profile(std::function<void(std::string_view)> fn);

  void profile(std::ostream& stream);

  db::datasource& datasource();

  bool readonly();

  int64_t changes();

private:
  db::datasource& _dsrc;
  std::shared_ptr<db::datasource::connection> _datasource_connection;
  bool _datasource_connection_owned = false;
};
} // namespace db
