#pragma once

#include "./connection.hpp"
#include <memory>

namespace db {
class transaction {
public:
  explicit transaction(connection& conn);

  transaction(const transaction&) = delete;

  transaction(transaction&&) = delete;

  transaction& operator=(const transaction&) = delete;

  transaction& operator=(transaction&&) = delete;

  ~transaction();

  void begin();

  void commit();

  void rollback();

  void onBegin(std::string_view script);

  void onCommit(std::string_view script);

  void onRollback(std::string_view script);

  void onClose(std::string_view script);

  bool readonly();

private:
  connection& _conn;
  bool _did_commit_or_rollback = true;

  std::vector<std::string> _on_begin_scripts;
  std::vector<std::string> _on_commit_scripts;
  std::vector<std::string> _on_rollback_scripts;

  void execute(const std::vector<std::string>& scripts);
};
} // namespace db
