#include "db/transaction.hpp"

namespace db {
transaction::transaction(connection& conn) : _conn(conn) {
  begin();
}

transaction::~transaction() {
  if (!_did_commit_or_rollback) {
    if (std::uncaught_exceptions() == 0) {
      commit();
    } else {
      rollback();
    }
  }
}

void transaction::begin() {
  if (_did_commit_or_rollback) {
    execute(_on_begin_scripts);
    _did_commit_or_rollback = false;
    _conn._datasource_connection->beginTransaction();
  }
}

void transaction::commit() {
  _conn._datasource_connection->commit();
  _did_commit_or_rollback = true;
  execute(_on_commit_scripts);
}

void transaction::rollback() {
  _conn._datasource_connection->rollback();
  _did_commit_or_rollback = true;
  execute(_on_rollback_scripts);
}

void transaction::onBegin(std::string_view script) {
  _on_begin_scripts.push_back((std::string)script);

  if (!_did_commit_or_rollback) {
    rollback();
    begin();
  }
}

void transaction::onCommit(std::string_view script) {
  _on_commit_scripts.push_back((std::string)script);
}

void transaction::onRollback(std::string_view script) {
  _on_rollback_scripts.push_back((std::string)script);
}

void transaction::onClose(std::string_view script) {
  onCommit(script);
  onRollback(script);
}

void transaction::execute(const std::vector<std::string>& scripts) {
  for (const auto& script : scripts) {
    _conn.execute(script);
  }
}

bool transaction::readonly() {
  return _conn._datasource_connection->readonlyTransaction();
}
} // namespace db
