#include "db/orm.hpp"

namespace db::orm {
selector::joiner::joiner(selector& s, query_builder_data::join_on_clause& d) : _selector(s), _data(d) {}

selector::joiner& selector::joiner::inner() {
  _data.mode = db::orm::JOIN_INNER;

  return *this;
}

selector::joiner& selector::joiner::left() {
  _data.mode = db::orm::JOIN_LEFT;

  return *this;
}

selector::joiner& selector::joiner::cross() {
  _data.mode = db::orm::JOIN_CROSS;

  return *this;
}

selector& selector::joiner::on(db::orm::query&& query) {
  return on(query.asCondition());
}

selector& selector::joiner::always() {
  return on("1 = 1");
}

selector::selector(db::connection& connection) : _connection(connection) {
}

selector& selector::select() {
  return *this;
}

selector& selector::select(const std::vector<selection>& fields) {
  for (const auto& field : fields) {
    _data.fields.push_back(field);
  }

  return *this;
}

selector& selector::distinct(bool distinct) {
  _data.distinct = distinct;

  return *this;
}

selector& selector::from(std::string_view table, std::string_view alias) {
  _data.table.name = table;
  _data.table.alias = alias;

  return *this;
}

selector::joiner& selector::join(std::string_view table, std::string_view alias) {
  auto& join = _data.joins.emplace_back();
  join.table.name = table;
  join.table.alias = alias;

  _joiner = std::make_shared<joiner>(*this, join);
  return *_joiner;
}

selector& selector::where(db::orm::query&& query) {
  return where(query.asCondition());
}

selector& selector::groupBy(std::string_view field) {
  _data.grouping.emplace_back((std::string)field);

  return *this;
}

selector& selector::having(db::orm::query&& query) {
  return having(query.asCondition());
}

selector& selector::orderBy(std::string_view field, order_by_direction direction,
    order_by_nulls nulls) {
  _data.ordering.emplace_back((std::string)field, direction, nulls);

  return *this;
}

selector& selector::offset(uint32_t offset) {
  _data.offset = offset;

  return *this;
}

selector& selector::limit(uint32_t limit) {
  _data.limit = limit;

  return *this;
}

void selector::prepare(db::statement& statement) {
  if (_data.fields.empty()) {
    if (_data.table.alias.empty()) {
      _data.fields.push_back("*");
    } else {
      _data.fields.push_back(_data.table.alias + '.' + '*');
    }
  }

  int param0 = 666;
  std::string script = _connection._datasource_connection->createSelectScript(_data, param0);
  statement.prepare(script);

  int param1 = 666;
  for (size_t i = 0; i < _data.joins.size(); i++) {
    if (_data.joins.at(i).condition != nullptr) {
      _data.joins.at(i).condition->assignToParams(statement, param1);
    }
  }
  for (auto condition : _data.conditions) {
    condition->assignToParams(statement, param1);
  }
  for (auto param : _params) {
    param->assignToParams(statement, param1);
  }
}

db::resultset selector::findMany() {
  db::statement statement{_connection};
  prepare(statement);
  return db::resultset{statement};
}

db::resultset selector::findOne() {
  _data.limit = 1;

  return findMany();
}

// selector& selector::pushNextIf(bool push_next) {
//   _push_next = push_next;

//   return *this;
// }

condition<std::function<void(std::ostream&, int&)>, condition_operator::CUSTOM, std::function<void(db::statement&, int&)>> selector::exists(const char* name, bool value) {
  _data.fields.clear();
  _data.fields.push_back("1");

  auto connection = std::reference_wrapper{_connection};
  auto conditions = _data.conditions;

  return {[connection, _data{std::move(_data)}, name](std::ostream& os, int& i) {
    std::string script = connection.get()._datasource_connection->createSelectScript(_data, i);
    os << "(EXISTS (" << script << ")) = " << name;
  }, [conditions{std::move(conditions)}, name, value](db::statement& statement, int& i) {
    statement.params[name] = value;

    for (auto condition : conditions) {
      condition->assignToParams(statement, i);
    }
  }};
}

updater::updater(db::connection& connection) : _connection(connection) {
}

updater& updater::in(std::string_view table, std::string_view alias) {
  _data.table.name = table;
  _data.table.alias = alias;

  return *this;
}

updater& updater::set(db::orm::query&& query) {
  return set(query.asCondition());
}

updater& updater::where(db::orm::query&& query) {
  return where(query.asCondition());
}

void updater::prepare(db::statement& statement) {
  if (!_prepare) {
    int param0 = 666;
    std::string script = _connection._datasource_connection->createUpdateScript(_data, param0);
    statement.prepare(script);
  }

  int param1 = 666;
  if (_prepare) {
    param1 = _prepare(statement);
  }

  for (auto assignment : _data.assignments) {
    assignment->assignToParams(statement, param1);
  }

  for (auto condition : _data.conditions) {
    condition->assignToParams(statement, param1);
  }
}

int updater::executeUpdate() {
  db::statement statement(_connection);
  prepare(statement);

  return statement.executeUpdate();
}

inserter::on_conflict::on_conflict(inserter& i) : _inserter(i) {}

inserter& inserter::on_conflict::doNothing() {
  _inserter._data.upsert = db::orm::query_builder_data::UP_NOTHING;

  return _inserter;
}

inserter& inserter::on_conflict::doReplace() {
  _inserter._data.upsert = db::orm::query_builder_data::UP_REPLACE;

  return _inserter;
}

// updater<T>& inserter::on_conflict::doUpdate() {
//   _inserter._data.upsert = db::orm::query_builder_data::UP_UPDATE;

//   _updater._prepare = std::bind(inserter::prepare, _inserter);
//   return _updater;
// }

inserter::inserter(db::connection& connection) : _connection(connection) {
}

inserter& inserter::into(std::string_view table) {
  _data.table = table;

  return *this;
}

inserter& inserter::set(db::orm::query&& query) {
  return set(query.asCondition());
}

inserter::on_conflict& inserter::onConflict(const std::vector<std::string>& fields) {
  _data.fields.clear();
  for (const auto& field : fields) {
    _data.fields.push_back({field});
  }

  return _on_conflict;
}

int inserter::prepare(db::statement& statement) {
  int param0 = 666;
  std::string script;
  if (_data.upsert == db::orm::query_builder_data::UP_DISABLED) {
    script = _connection._datasource_connection->createInsertScript(_data, param0);
  } else {
    script = _connection._datasource_connection->createUpsertScript(_data, param0);
  }

  statement.prepare(script);

  int param1 = 666;
  for (auto assignment : _data.assignments) {
    assignment->assignToParams(statement, param1);
  }

  return param1;
}

int inserter::executeUpdate() {
  db::statement statement(_connection);
  prepare(statement);

  return statement.executeUpdate();
}

deleter::deleter(db::connection& connection) : _connection(connection) {
}

deleter& deleter::from(std::string_view table, std::string_view alias) {
  _data.table.name = table;
  _data.table.alias = alias;

  return *this;
}

deleter& deleter::where(db::orm::query&& query) {
  return where(query.asCondition());
}

void deleter::prepare(db::statement& statement) {
  int param0 = 666;
  std::string script = _connection._datasource_connection->createDeleteScript(_data, param0);
  statement.prepare(script);

  int param1 = 666;
  for (auto condition : _data.conditions) {
    condition->assignToParams(statement, param1);
  }
}

int deleter::executeUpdate() {
  db::statement statement(_connection);
  prepare(statement);

  return statement.executeUpdate();
}
}
