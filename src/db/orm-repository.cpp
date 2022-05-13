#include "db/orm-repository.hpp"

namespace db::orm {
repository::repository(db::connection& conn) : _conn{&conn, [](auto) {}} {
}

repository::repository(db::datasource& dsrc) : _conn(std::make_shared<db::connection>(dsrc)) {
}

orm::selector repository::select() {
  orm::selector builder{*_conn};
  builder.select();
  return builder;
}

orm::selector repository::select(const std::vector<orm::selection>& fields) {
  orm::selector builder{*_conn};
  builder.select(fields);
  return builder;
}

orm::inserter repository::insert() {
  orm::inserter builder{*_conn};
  return builder;
}

orm::updater repository::update() {
  orm::updater builder{*_conn};
  return builder;
}

orm::deleter repository::remove() {
  orm::deleter builder{*_conn};
  return builder;
}
} // namespace db::orm
