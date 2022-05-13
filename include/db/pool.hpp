#pragma once

#include "./connection.hpp"
#ifdef CMAKE_ENABLE_THREADING
#include <shared_mutex>
#endif

namespace db::pooled {
template <typename Datasource>
class datasource : public Datasource {
public:
  using Datasource::Datasource;

  std::shared_ptr<db::datasource::connection> getConnection() override {
#ifdef CMAKE_ENABLE_THREADING
    std::shared_lock rlock(_connections_mutex);
#endif

    for (auto& connection : _connections) {
      if (connection.use_count() > 1) {
        continue;
      }

      return connection;
    }

#ifdef CMAKE_ENABLE_THREADING
    rlock.unlock();
    std::unique_lock wlock(_connections_mutex);
#endif

    auto connection = Datasource::getConnection();
    _connections.push_back(connection);

    if (_onConnectionCreate) {
      db::connection conn{*this, connection};
      _onConnectionCreate(conn);
    }

    return connection;
  }

  std::function<void(db::connection&)>& onConnectionCreate() {
    return _onConnectionCreate;
  }

private:
  std::vector<std::shared_ptr<db::datasource::connection>> _connections;
#ifdef CMAKE_ENABLE_THREADING
  std::shared_mutex _connections_mutex;
#endif

  std::function<void(db::connection&)> _onConnectionCreate;
};

namespace dynamic {
class wrapper : public db::datasource {
public:
  wrapper(db::datasource& dsrc);

  std::shared_ptr<db::datasource::connection> getConnection() override;

  std::function<void(db::connection&)>& onConnectionOpen() override;

  std::function<void(db::connection&)>& onConnectionClose() override;

  std::vector<db::orm::update>& updates() override;

private:
  db::datasource& _dsrc;
};

using datasource = db::pooled::datasource<wrapper>;
}
} // namespace db::pooled
