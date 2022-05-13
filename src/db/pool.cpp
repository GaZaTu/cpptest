#include "db/pool.hpp"

namespace db::pooled {
namespace dynamic {
wrapper::wrapper(db::datasource& dsrc) : _dsrc(dsrc) {
}

std::shared_ptr<db::datasource::connection> wrapper::getConnection() {
  return _dsrc.getConnection();
}

std::function<void(db::connection&)>& wrapper::onConnectionOpen() {
  return _dsrc.onConnectionOpen();
}

std::function<void(db::connection&)>& wrapper::onConnectionClose() {
  return _dsrc.onConnectionClose();
}

std::vector<db::orm::update>& wrapper::updates() {
  return _dsrc.updates();
}
}
} // namespace db::pooled
