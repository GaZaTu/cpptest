#include "db/connection.hpp"

namespace db {
connection::connection(db::datasource& dsrc, std::shared_ptr<db::datasource::connection> conn) : _dsrc(dsrc) {
  _datasource_connection = conn;
  _datasource_connection_owned = false;
}

connection::connection(db::datasource& dsrc) : _dsrc(dsrc) {
  _datasource_connection = dsrc.getConnection();
  _datasource_connection_owned = true;

  if (_dsrc.onConnectionOpen()) {
    _dsrc.onConnectionOpen()(*this);
  }
}

connection::connection(connection&& src) : _dsrc(src._dsrc) {
  _datasource_connection = src._datasource_connection;
  _datasource_connection_owned = src._datasource_connection_owned;

  src._datasource_connection_owned = false;
};

connection::~connection() {
  if (_datasource_connection_owned && _dsrc.onConnectionClose()) {
    _dsrc.onConnectionClose()(*this);
  }
}

void connection::execute(std::string_view script) {
  _datasource_connection->execute(script);
}

int connection::version() {
  return _datasource_connection->getVersion();
}

void connection::upgrade(int new_version) {
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
    try {
      _datasource_connection->setVersion(old_version);
    } catch (...) {}

    throw;
  }
}

void connection::downgrade(int new_version) {
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

void connection::profile(std::function<void(std::string_view)> fn) {
  _datasource_connection->onPrepareStatement() = fn;
}

void connection::profile(std::ostream& stream) {
  profile([&stream](std::string_view script) {
    stream << script << std::endl;
  });
}

db::datasource& connection::datasource() {
  return _dsrc;
}

bool connection::readonly() {
  return _datasource_connection->readonly();
}

int64_t connection::changes() {
  return _datasource_connection->changes();
}
} // namespace db
