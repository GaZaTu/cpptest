#pragma once

#include "db.hpp"
#include "sqlite3.h"

namespace db::sqlite {
class sqlite3_error : public db::sql_error {
public:
  sqlite3_error(const std::string& msg) : db::sql_error(msg) {
  }

  sqlite3_error(int code) : db::sql_error(sqlite3_errstr(code)) {
  }

  sqlite3_error(sqlite3* db) : db::sql_error(sqlite3_errmsg(db)) {
  }

  static void assert(int code) {
    if (code != SQLITE_OK) {
      throw sqlite3_error(code);
    }
  }

  static void assert(int code, sqlite3* db) {
    if (code != SQLITE_OK) {
      throw sqlite3_error(db);
    }
  }
};

class datasource : public db::datasource {
public:
  class resultset : public db::datasource::resultset {
  public:
    resultset(sqlite3* native_connection, sqlite3_stmt* native_statement)
        : _native_connection(native_connection), _native_statement(native_statement) {
      if ((_code = sqlite3_step(_native_statement)) == SQLITE_ERROR) {
        throw sqlite3_error(_native_connection);
      }

      for (int i = 0, col_count = sqlite3_data_count(_native_statement); i < col_count; i++) {
        _cols[sqlite3_column_name(_native_statement, i)] = i;
      }
    }

    virtual ~resultset() override {
    }

    virtual bool next() override {
      if (_first_row) {
        _first_row = false;
        return _code == SQLITE_ROW;
      } else {
        return sqlite3_step(_native_statement) == SQLITE_ROW;
      }
    }

    virtual bool isValueNull(const std::string_view name) override {
      return sqlite3_column_type(_native_statement, _cols[(std::string)name]) == SQLITE_NULL;
    }

    virtual void getValue(const std::string_view name, bool& result) override {
      result = sqlite3_column_int(_native_statement, _cols[(std::string)name]) != 0;
    }

    virtual void getValue(const std::string_view name, int& result) override {
      result = sqlite3_column_int(_native_statement, _cols[(std::string)name]);
    }

    virtual void getValue(const std::string_view name, double& result) override {
      result = sqlite3_column_double(_native_statement, _cols[(std::string)name]);
    }

    virtual void getValue(const std::string_view name, std::string& result) override {
      auto col = _cols[(std::string)name];
      auto text_ptr = sqlite3_column_text(_native_statement, col);
      auto byte_count = sqlite3_column_bytes(_native_statement, col);

      result = {reinterpret_cast<const char*>(text_ptr), static_cast<std::string::size_type>(byte_count)};
    }

    virtual void getValue(const std::string_view name, std::vector<std::byte>& result) override {
      auto col = _cols[(std::string)name];
      auto blob_ptr = sqlite3_column_blob(_native_statement, col);
      auto byte_count = sqlite3_column_bytes(_native_statement, col);
      auto bytes_ptr = static_cast<const std::byte*>(blob_ptr);

      result = {bytes_ptr, bytes_ptr + byte_count};
    }

    virtual int columnCount() override {
      return sqlite3_data_count(_native_statement);
    }

    virtual std::string columnName(int i) override {
      return sqlite3_column_name(_native_statement, i);
    }

  private:
    sqlite3* _native_connection;
    sqlite3_stmt* _native_statement;

    int _code;
    std::unordered_map<std::string, int> _cols;
    bool _first_row = true;
  };

  class statement : public db::datasource::statement {
  public:
    statement(sqlite3* native_connection, const std::string_view script) : _native_connection(native_connection) {
      sqlite3_error::assert(
          sqlite3_prepare_v2(_native_connection, script.data(), script.length() + 1, &_native_statement, nullptr),
          _native_connection);
    }

    virtual ~statement() override {
      sqlite3_finalize(_native_statement);
    }

    virtual std::shared_ptr<db::datasource::resultset> execute() override {
      return std::make_shared<resultset>(_native_connection, _native_statement);
    }

    virtual int executeUpdate() override {
      if (sqlite3_step(_native_statement) == SQLITE_ERROR) {
        throw sqlite3_error(_native_connection);
      }

      return sqlite3_changes(_native_connection);
    }

    virtual void setParamToNull(const std::string_view name) override {
      int index = sqlite3_bind_parameter_index(_native_statement, name.data());

      if (index != 0) {
        sqlite3_error::assert(sqlite3_bind_null(_native_statement, index), _native_connection);
      }
    }

    virtual void setParam(const std::string_view name, bool value) override {
      int index = sqlite3_bind_parameter_index(_native_statement, name.data());

      if (index != 0) {
        sqlite3_error::assert(sqlite3_bind_int(_native_statement, index, value), _native_connection);
      }
    }

    virtual void setParam(const std::string_view name, int value) override {
      int index = sqlite3_bind_parameter_index(_native_statement, name.data());

      if (index != 0) {
        sqlite3_error::assert(sqlite3_bind_int(_native_statement, index, value), _native_connection);
      }
    }

    virtual void setParam(const std::string_view name, double value) override {
      int index = sqlite3_bind_parameter_index(_native_statement, name.data());

      if (index != 0) {
        sqlite3_error::assert(sqlite3_bind_double(_native_statement, index, value), _native_connection);
      }
    }

    virtual void setParam(const std::string_view name, const std::string_view value) override {
      int index = sqlite3_bind_parameter_index(_native_statement, name.data());

      if (index != 0) {
        sqlite3_error::assert(
            sqlite3_bind_text(_native_statement, index, value.data(), value.length(), SQLITE_TRANSIENT),
            _native_connection);
      }
    }

    virtual void setParam(const std::string_view name, const std::vector<std::byte>& value) override {
      int index = sqlite3_bind_parameter_index(_native_statement, name.data());

      if (index != 0) {
        sqlite3_error::assert(sqlite3_bind_blob(_native_statement, index, static_cast<const void*>(value.data()),
                                  value.size(), SQLITE_TRANSIENT),
            _native_connection);
      }
    }

  private:
    sqlite3* _native_connection;
    sqlite3_stmt* _native_statement;
  };

  class connection : public db::datasource::connection {
  public:
    connection(const std::string_view filename, int flags) {
      sqlite3_error::assert(sqlite3_open_v2(filename.data(), &_native_connection, flags, nullptr), _native_connection);
    }

    virtual ~connection() override {
      sqlite3_close(_native_connection);
    }

    virtual std::shared_ptr<db::datasource::statement> prepareStatement(const std::string_view script) override {
      return std::make_shared<statement>(_native_connection, script);
    }

    virtual void beginTransaction() override {
      execute("BEGIN TRANSACTION");
    }

    virtual void commit() override {
      execute("COMMIT");
    }

    virtual void rollback() override {
      execute("ROLLBACK");
    }

    virtual void execute(const std::string_view script) override {
      char* errmsg = nullptr;
      int code = sqlite3_exec(_native_connection, script.data(), nullptr, nullptr, &errmsg);

      if (code != SQLITE_OK && errmsg != nullptr) {
        std::string errmsg_str{errmsg};
        sqlite3_free(errmsg);
        throw sqlite3_error(errmsg_str);
      }
    }

  private:
    sqlite3* _native_connection;
  };

  datasource(const std::string_view filename, int flags = default_flags) : _filename(filename), _flags(flags) {
  }

  virtual std::shared_ptr<db::datasource::connection> getConnection() override {
    return std::make_shared<connection>(_filename, _flags);
  }

private:
  std::string _filename;
  int _flags;

  static const int default_flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI;
};
} // namespace db::sqlite
