#pragma once

#include "./datasource.hpp"
#include <memory>
#include <optional>
#include <string_view>

namespace db {
class connection {
public:
  friend statement;
  friend transaction;

  template <typename T>
  friend class orm::selector;

  template <typename T>
  friend class orm::inserter;

  template <typename T>
  friend class orm::updater;

  template <typename T>
  friend class orm::deleter;

  explicit connection(datasource& dsrc, std::shared_ptr<db::datasource::connection> conn) : _dsrc(dsrc) {
    _datasource_connection = conn;
    _datasource_connection_owned = false;
  }

  explicit connection(datasource& dsrc) : _dsrc(dsrc) {
    _datasource_connection = dsrc.getConnection();
    _datasource_connection_owned = true;

    if (_dsrc.onConnectionOpen()) {
      _dsrc.onConnectionOpen()(*this);
    }
  }

  connection(const connection&) = delete;

  connection(connection&& src) : _dsrc(src._dsrc) {
    _datasource_connection = src._datasource_connection;
    _datasource_connection_owned = src._datasource_connection_owned;

    src._datasource_connection_owned = false;
  };

  connection& operator=(const connection&) = delete;

  connection& operator=(connection&&) = delete;

  ~connection() {
    if (_datasource_connection_owned && _dsrc.onConnectionClose()) {
      _dsrc.onConnectionClose()(*this);
    }
  }

  void execute(const std::string_view script) {
    _datasource_connection->execute(script);
  }

  int version() {
    return _datasource_connection->getVersion();
  }

  void upgrade(int new_version = -1) {
    int old_version = version();

    try {
      for (auto& update : _dsrc.updates()) {
        if (update.version <= old_version || (update.version > new_version && new_version != -1)) {
          continue;
        }

        update.up(*this);

        _datasource_connection->setVersion(update.version);
      }
    } catch (...) {
      _datasource_connection->setVersion(old_version);

      throw;
    }
  }

  void downgrade(int new_version) {
    int old_version = version();

    try {
      for (int i = _dsrc.updates().size() - 1; i >= 0; i--) {
        auto& update = _dsrc.updates().at(i);

        if (update.version > old_version || update.version <= new_version) {
          continue;
        }

        update.down(*this);

        _datasource_connection->setVersion(update.version);
      }
    } catch (...) {
      _datasource_connection->setVersion(old_version);

      throw;
    }
  }

  template <typename T = datasource::connection>
  std::shared_ptr<T> getNativeConnection() {
    return std::dynamic_pointer_cast<T>(_datasource_connection);
  }

  void profile(std::function<void(std::string_view)> fn) {
    _datasource_connection->onPrepareStatement() = fn;
  }

  void profile(std::ostream& stream) {
    profile([&stream](std::string_view script) {
      stream << script << std::endl;
    });
  }

  orm::selector<std::nullopt_t> select();

  orm::selector<std::nullopt_t> select(const std::vector<orm::selection>& fields);

  orm::inserter<std::nullopt_t> insert();

  orm::updater<std::nullopt_t> update();

  orm::deleter<std::nullopt_t> remove();

  datasource& source() {
    return _dsrc;
  }

private:
  datasource& _dsrc;
  std::shared_ptr<datasource::connection> _datasource_connection;
  bool _datasource_connection_owned = false;
};
} // namespace db
