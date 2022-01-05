#pragma once

#include "./connection.hpp"
#include <memory>

namespace db {
class transaction {
public:
  explicit transaction(connection& conn) : _conn(conn) {
    begin();
  }

  transaction(const transaction&) = delete;

  transaction(transaction&&) = delete;

  transaction& operator=(const transaction&) = delete;

  transaction& operator=(transaction&&) = delete;

  ~transaction() {
    if (!_did_commit_or_rollback) {
      if (std::uncaught_exceptions() == 0) {
        commit();
      } else {
        rollback();
      }
    }
  }

  void begin() {
    if (_did_commit_or_rollback) {
      execute(_on_begin_scripts);
      _did_commit_or_rollback = false;
      _conn.get()._datasource_connection->beginTransaction();
    }
  }

  void commit() {
    _conn.get()._datasource_connection->commit();
    _did_commit_or_rollback = true;
    execute(_on_commit_scripts);
  }

  void rollback() {
    _conn.get()._datasource_connection->rollback();
    _did_commit_or_rollback = true;
    execute(_on_rollback_scripts);
  }

  void onBegin(std::string_view script) {
    _on_begin_scripts.push_back((std::string)script);

    if (!_did_commit_or_rollback) {
      rollback();
      begin();
    }
  }

  void onCommit(std::string_view script) {
    _on_commit_scripts.push_back((std::string)script);
  }

  void onRollback(std::string_view script) {
    _on_rollback_scripts.push_back((std::string)script);
  }

  void onClose(std::string_view script) {
    onCommit(script);
    onRollback(script);
  }

private:
  std::reference_wrapper<connection> _conn;
  bool _did_commit_or_rollback = true;

  std::vector<std::string> _on_begin_scripts;
  std::vector<std::string> _on_commit_scripts;
  std::vector<std::string> _on_rollback_scripts;

  void execute(const std::vector<std::string>& scripts) {
    for (const auto& script : scripts) {
      _conn.get().execute(script);
    }
  }
};
} // namespace db
