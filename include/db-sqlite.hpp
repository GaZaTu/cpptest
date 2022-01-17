#pragma once

#include "./db/connection.hpp"
#include "./db/datasource.hpp"
#include "./db/orm.hpp"
#include "./db/pool.hpp"
#include "sqlite3.h"
#include <sstream>

namespace db::sqlite {
const char DRIVER_NAME[] = "sqlite";

class sqlite3_error : public db::sql_error {
public:
  sqlite3_error(const std::string& msg) : db::sql_error(msg) {
  }

  sqlite3_error(int code) : db::sql_error(sqlite3_errstr(code)) {
  }

  sqlite3_error(std::shared_ptr<sqlite3> db) : db::sql_error(sqlite3_errmsg(&*db)) {
  }

  static void _assert(int code) {
    if (code != SQLITE_OK && code != SQLITE_DONE) {
      throw sqlite3_error(code);
    }
  }

  static void _assert(int code, std::shared_ptr<sqlite3> db) {
    if (code != SQLITE_OK && code != SQLITE_DONE) {
      throw sqlite3_error(db);
    }
  }
};

class datasource : public db::datasource {
public:
  class resultset : public db::datasource::resultset {
  public:
    resultset(std::shared_ptr<sqlite3> native_connection, std::shared_ptr<sqlite3_stmt> native_statement)
        : _native_connection(native_connection), _native_statement(native_statement) {
      if ((_code = sqlite3_step(&*_native_statement)) == SQLITE_ERROR) {
        throw sqlite3_error(_native_connection);
      }

      for (int i = 0, col_count = sqlite3_data_count(&*_native_statement); i < col_count; i++) {
        _cols[sqlite3_column_name(&*_native_statement, i)] = i;
      }
    }

    virtual ~resultset() override {
    }

    bool next() override {
      if (_first_row) {
        _first_row = false;
        return _code == SQLITE_ROW;
      } else {
        return sqlite3_step(&*_native_statement) == SQLITE_ROW;
      }
    }

    orm::field_type getValueType(const std::string& name) override {
      auto native_type = sqlite3_column_type(&*_native_statement, _cols[(std::string)name]);

      switch (native_type) {
        case SQLITE_INTEGER: return orm::INT64;
        case SQLITE_FLOAT: return orm::DOUBLE;
        case SQLITE_TEXT: return orm::STRING;
        case SQLITE_BLOB: return orm::BLOB;
        default: return orm::UNKNOWN;
      }
    }

    bool isValueNull(const std::string& name) override {
      return sqlite3_column_type(&*_native_statement, _cols[(std::string)name]) == SQLITE_NULL;
    }

    void getValue(const std::string& name, bool& result) override {
      result = sqlite3_column_int(&*_native_statement, _cols[(std::string)name]) != 0;
    }

    void getValue(const std::string& name, int& result) override {
      result = sqlite3_column_int(&*_native_statement, _cols[(std::string)name]);
    }

    void getValue(const std::string& name, int64_t& result) override {
      result = sqlite3_column_int64(&*_native_statement, _cols[(std::string)name]);
    }

#ifdef __SIZEOF_INT128__
    void getValue(const std::string& name, __uint128_t& result) override {
      const void* bytes;
      getValue<sizeof(__uint128_t)>(name, &bytes);
      result = *(__uint128_t*)bytes;
    }
#endif

    void getValue(const std::string& name, double& result) override {
      result = sqlite3_column_double(&*_native_statement, _cols[(std::string)name]);
    }

    void getValue(const std::string& name, std::string& result) override {
      auto col = _cols[(std::string)name];
      auto text_ptr = sqlite3_column_text(&*_native_statement, col);
      auto byte_count = sqlite3_column_bytes(&*_native_statement, col);

      result = {reinterpret_cast<const char*>(text_ptr), static_cast<std::string::size_type>(byte_count)};
    }

    void getValue(const std::string& name, std::vector<uint8_t>& result) override {
      auto col = _cols[(std::string)name];
      auto blob_ptr = sqlite3_column_blob(&*_native_statement, col);
      auto byte_count = sqlite3_column_bytes(&*_native_statement, col);
      auto bytes_ptr = static_cast<const uint8_t*>(blob_ptr);

      result = {bytes_ptr, bytes_ptr + byte_count};
    }

    template <std::size_t N>
    void getValue(const std::string& name, const void** result) {
      auto col = _cols[(std::string)name];
      auto blob_ptr = sqlite3_column_blob(&*_native_statement, col);

      *result = blob_ptr;
    }

    int columnCount() override {
      return sqlite3_data_count(&*_native_statement);
    }

    std::string columnName(int i) override {
      return sqlite3_column_name(&*_native_statement, i);
    }

  public:
    std::shared_ptr<sqlite3> _native_connection;
    std::shared_ptr<sqlite3_stmt> _native_statement;

    int _code;
    std::unordered_map<std::string, int> _cols;
    bool _first_row = true;
  };

  class statement : public db::datasource::statement {
  public:
    statement(std::shared_ptr<sqlite3> native_connection, const std::string_view script)
        : _native_connection(native_connection) {
      sqlite3_stmt* statement = nullptr;
      sqlite3_error::_assert(
          sqlite3_prepare_v2(&*_native_connection, script.data(), script.length() + 1, &statement, nullptr),
          _native_connection);

      _native_statement = {statement, &sqlite3_finalize};
    }

    virtual ~statement() override {
    }

    std::shared_ptr<db::datasource::resultset> execute() override {
      return std::make_shared<resultset>(_native_connection, _native_statement);
    }

    int executeUpdate() override {
      sqlite3_error::_assert(sqlite3_step(&*_native_statement), _native_connection);

      return sqlite3_changes(&*_native_connection);
    }

    void setParamToNull(const std::string& name) override {
      int index = sqlite3_bind_parameter_index(&*_native_statement, name.data());

      if (index != 0) {
        sqlite3_error::_assert(sqlite3_bind_null(&*_native_statement, index), _native_connection);
      }
    }

    void setParam(const std::string& name, bool value) override {
      int index = sqlite3_bind_parameter_index(&*_native_statement, name.data());

      if (index != 0) {
        sqlite3_error::_assert(sqlite3_bind_int(&*_native_statement, index, value), _native_connection);
      }
    }

    void setParam(const std::string& name, int value) override {
      int index = sqlite3_bind_parameter_index(&*_native_statement, name.data());

      if (index != 0) {
        sqlite3_error::_assert(sqlite3_bind_int(&*_native_statement, index, value), _native_connection);
      }
    }

    void setParam(const std::string& name, int64_t value) override {
      int index = sqlite3_bind_parameter_index(&*_native_statement, name.data());

      if (index != 0) {
        sqlite3_error::_assert(sqlite3_bind_int64(&*_native_statement, index, value), _native_connection);
      }
    }

#ifdef __SIZEOF_INT128__
    void setParam(const std::string& name, __uint128_t value) override {
      setParam<sizeof(__uint128_t)>(name, (const void*)&value);
    }
#endif

    void setParam(const std::string& name, double value) override {
      int index = sqlite3_bind_parameter_index(&*_native_statement, name.data());

      if (index != 0) {
        sqlite3_error::_assert(sqlite3_bind_double(&*_native_statement, index, value), _native_connection);
      }
    }

    void setParam(const std::string& name, const std::string_view value) override {
      int index = sqlite3_bind_parameter_index(&*_native_statement, name.data());

      if (index != 0) {
        sqlite3_error::_assert(
            sqlite3_bind_text(&*_native_statement, index, value.data(), value.length(), SQLITE_TRANSIENT),
            _native_connection);
      }
    }

    void setParam(const std::string& name, const std::vector<uint8_t>& value) override {
      int index = sqlite3_bind_parameter_index(&*_native_statement, name.data());

      if (index != 0) {
        sqlite3_error::_assert(sqlite3_bind_blob(&*_native_statement, index, static_cast<const void*>(value.data()),
                                  value.size(), SQLITE_TRANSIENT),
            _native_connection);
      }
    }

    template <std::size_t N>
    void setParam(const std::string& name, const void* data) {
      int index = sqlite3_bind_parameter_index(&*_native_statement, name.data());

      if (index != 0) {
        sqlite3_error::_assert(
            sqlite3_bind_blob(&*_native_statement, index, data, N, SQLITE_TRANSIENT), _native_connection);
      }
    }

  public:
    std::shared_ptr<sqlite3> _native_connection;
    std::shared_ptr<sqlite3_stmt> _native_statement;
  };

  class connection : public db::datasource::connection {
  public:
    connection(sqlite3 *connection) {
      _native_connection = {connection, [](sqlite3*) {}};
    }

    connection(const std::string& filename, int flags) {
      sqlite3* connection = nullptr;
      sqlite3_error::_assert(sqlite3_open_v2(filename.data(), &connection, flags, nullptr), _native_connection);

      _native_connection = {connection, &sqlite3_close};
    }

    virtual ~connection() override {
    }

    std::shared_ptr<db::datasource::statement> prepareStatement(const std::string_view script) override {
      if (onPrepareStatement()) {
        onPrepareStatement()(script);
      }

      return std::make_shared<statement>(_native_connection, script);
    }

    void beginTransaction() override {
      execute("BEGIN TRANSACTION");
    }

    void commit() override {
      execute("COMMIT");
    }

    void rollback() override {
      execute("ROLLBACK");
    }

    void execute(const std::string& script) override {
      if (onPrepareStatement()) {
        onPrepareStatement()(script);
      }

      char* errmsg = nullptr;
      int code = sqlite3_exec(&*_native_connection, script.data(), nullptr, nullptr, &errmsg);

      if (code != SQLITE_OK && errmsg != nullptr) {
        std::string errmsg_str{errmsg};
        sqlite3_free(errmsg);
        throw sqlite3_error(errmsg_str);
      }
    }

    int getVersion() override {
      auto statement = prepareStatement("PRAGMA user_version");
      auto resultset = statement->execute();

      int version = 0;
      resultset->getValue(resultset->columnName(0), version);

      return version;
    }

    void setVersion(int version) override {
      execute(std::string{"PRAGMA user_version = "} + std::to_string(version));
    }

    bool supportsORM() override {
      return true;
    }

    void createInsertScript(std::stringstream& str, const char* table, const orm::field_info* fields, size_t fields_len) {
      str << "INSERT INTO " << '"' << table << '"' << " (";

      for (size_t i = 0; i < fields_len; i++) {
        if (i > 0) {
          str << ", ";
        }

        str << fields[i].name;
      }

      str << ") VALUES (";

      for (size_t i = 0; i < fields_len; i++) {
        if (i > 0) {
          str << ", ";
        }

        str << ":" << fields[i].name;
      }

      str << ")";
    }

    std::string createInsertScript(const char* table, const orm::field_info* fields, size_t fields_len) override {
      std::stringstream str;
      createInsertScript(str, table, fields, fields_len);
      return str.str();
    }

    std::string createUpdateScript(const char* table, const orm::field_info* fields, size_t fields_len) override {
      std::stringstream str;

      str << "UPDATE " << '"' << table << '"' << " SET ";

      for (size_t i = 1; i < fields_len; i++) {
        if (i > 1) {
          str << ", ";
        }

        str << fields[i].name << " = :" << fields[i].name;
      }

      str << " WHERE " << fields[0].name << " = :" << fields[0].name;

      return str.str();
    }

    // std::string createUpsertScript(const char* table, const orm::field_info* fields, size_t fields_len, const orm::field_info* idfields, size_t idfields_len) override {
    //   std::stringstream str;

    //   createInsertScript(str, table, fields, fields_len);

    //   str << " ON CONFLICT (";

    //   for (size_t i = 1; i < idfields_len; i++) {
    //     if (i > 0) {
    //       str << ", ";
    //     }

    //     str << idfields[i].name;
    //   }

    //   str << ")" << " DO UPDATE SET ";

    //   for (size_t i = 1; i < fields_len; i++) {
    //     if (i > 0) {
    //       str << ", ";
    //     }

    //     str << fields[i].name << " = " << "excluded." << fields[i].name;
    //   }

    //   return str.str();
    // }

    void createInsertScript(const orm::query_builder_data& data, std::stringstream& str, int& param) {
      str << "INSERT INTO " << (std::string)data.table << " (";

      for (size_t i = 0; i < data.assignments.size(); i++) {
        if (i > 0) {
          str << ", ";
        }

        data.assignments.at(i)->appendLeftToQuery(str, param);
      }

      str << ") VALUES (";

      for (size_t i = 0; i < data.assignments.size(); i++) {
        if (i > 0) {
          str << ", ";
        }

        data.assignments.at(i)->appendRightToQuery(str, param);
      }

      str << ")";
    }

    std::string createInsertScript(const orm::query_builder_data& data) override {
      std::stringstream str;
      int param = 666;

      createInsertScript(data, str, param);

      return str.str();
    }

    void createUpdateScript(const orm::query_builder_data& data, std::stringstream& str, int& param) {
      str << "UPDATE ";

      if (data.table) {
        str << (std::string)data.table;
      }

      str << " SET ";

      for (size_t i = 0; i < data.assignments.size(); i++) {
        if (i > 0) {
          str << ", ";
        }

        data.assignments.at(i)->appendToQuery(str, param);
      }

      for (size_t i = 0; i < data.conditions.size(); i++) {
        if (i > 0) {
          str << " AND ";
        } else {
          str << " WHERE ";
        }

        data.conditions.at(i)->appendToQuery(str, param);
      }
    }

    std::string createUpdateScript(const orm::query_builder_data& data) override {
      std::stringstream str;
      int param = 666;

      createUpdateScript(data, str, param);

      return str.str();
    }

    std::string createUpsertScript(const orm::query_builder_data& _data) override {
      auto data = _data;

      std::stringstream str;
      int param = 666;

      createInsertScript(data, str, param);

      str << " ON CONFLICT";

      if (data.fields.size() > 0) {
        str << " (";

        for (size_t i = 0; i < data.fields.size(); i++) {
          if (i > 0) {
            str << ", ";
          }

          str << data.fields.at(i).name;
        }

        str << ")";
      }

      str << " DO ";

      if (data.upsert == orm::query_builder_data::NOTHING) {
        str << "NOTHING";
      } else if (data.upsert == orm::query_builder_data::REPLACE) {
        data.table.name = "";

        auto assignments = std::move(data.assignments);
        for (auto assignment : assignments) {
          if (assignment->getOperator() == orm::condition_operator::ASSIGNMENT) {
            std::stringstream code;
            assignment->appendLeftToQuery(code, param);
            code << " = EXCLUDED.";
            assignment->appendLeftToQuery(code, param);

            auto assignment_on_conflict = std::make_shared<orm::condition<std::string, orm::condition_operator::CUSTOM & orm::condition_operator::ASSIGNMENT, std::nullopt_t>>(code.str(), std::nullopt);
            data.assignments.push_back(assignment_on_conflict);
          } else {
            data.assignments.push_back(assignment);
          }
        }

        createUpdateScript(data, str, param);
      } else if (data.upsert == orm::query_builder_data::UPDATE) {
        // TODO
      } else {
        str << " your mom?";
      }

      return str.str();
    }

    std::string createSelectScript(const orm::query_builder_data& data) override {
      std::stringstream str;

      str << "SELECT ";

      for (size_t i = 0; i < data.fields.size(); i++) {
        if (i > 0) {
          str << ", ";
        }

        str << (std::string)data.fields.at(i);
      }

      if (data.table) {
        str << " FROM " << (std::string)data.table;
      }

      for (size_t i = 0; i < data.joins.size(); i++) {
        str << " JOIN " << (std::string)data.joins.at(i).table;
        str << " ON " << data.joins.at(i).condition;
      }

      for (size_t i = 0; i < data.conditions.size(); i++) {
        if (i > 0) {
          str << " AND ";
        } else {
          str << " WHERE ";
        }

        data.conditions.at(i)->appendToQuery(str);
      }

      for (size_t i = 0; i < data.ordering.size(); i++) {
        if (i > 0) {
          str << ", ";
        } else {
          str << " ORDER BY ";
        }

        str << data.ordering.at(i).field;

        switch (data.ordering.at(i).direction) {
        case db::orm::ASCENDING:
          str << " ASC";
          break;
        case db::orm::DESCENDING:
          str << " DESC";
          break;
        }

        switch (data.ordering.at(i).nulls) {
        case db::orm::NULLS_FIRST:
          str << " NULLS FIRST";
          break;
        case db::orm::NULLS_LAST:
          str << " NULLS LAST";
          break;
        }
      }

      if (data.limit > 0) {
        str << " LIMIT " << data.limit;
      }

      return str.str();
    }

    std::string createDeleteScript(const orm::query_builder_data& data) override {
      std::stringstream str;

      str << "DELETE FROM " << (std::string)data.table;

      for (size_t i = 0; i < data.conditions.size(); i++) {
        if (i > 0) {
          str << " AND ";
        } else {
          str << " WHERE ";
        }

        data.conditions.at(i)->appendToQuery(str);
      }

      return str.str();
    }

  public:
    std::shared_ptr<sqlite3> _native_connection;
  };

  explicit datasource() {
  }

  explicit datasource(const std::string_view filename, int flags = default_flags) : _filename(filename), _flags(flags) {
  }

  virtual std::string driver() override {
    return DRIVER_NAME;
  }

  virtual std::shared_ptr<db::datasource::connection> getConnection() override {
    return std::make_shared<connection>(_filename, _flags);
  }

  // template <typename T>
  // int createTable() {
  //   try {
  //     db::connection connection(*this);
  //     return connection.createTable<T>();
  //   } catch (...) {
  //     throw;
  //   }
  // }

private:
  std::string _filename;
  int _flags;

  static const int default_flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI;
};

namespace pooled {
using datasource = db::pooled::datasource<db::sqlite::datasource>;
}

void pragma(db::connection& conn, std::string_view name, std::string_view value) {
  conn.execute(std::string{"PRAGMA "} + (std::string)name + std::string{" = "} + (std::string)value);
}

std::string pragma(db::connection& conn, std::string_view name) {
  db::statement stmt{conn};
  stmt.prepare(std::string{"PRAGMA "} + (std::string)name);
  db::resultset rslt{stmt};

  return rslt.firstValue<std::string>().value_or("");
}

// void configure(db::connection& _conn, int op) {
//   auto conn = _conn.getNativeConnection<datasource::connection>();

//   sqlite3_error::assert(sqlite3_db_config(&*conn->_native_connection, op), conn->_native_connection);
// }

// void loadExtension(db::connection& _conn, std::string_view name) {
//   auto conn = _conn.getNativeConnection<datasource::connection>();

//   configure(_conn, SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION);

//   sqlite3_error::assert(
//       sqlite3_load_extension(&*conn->_native_connection, name.data(), nullptr, nullptr), conn->_native_connection);
// }

db::connection convertConnection(sqlite3* native_connection) {
  auto datasource = db::sqlite::datasource{};
  auto datasource_connection = std::make_shared<db::sqlite::datasource::connection>(native_connection);
  auto connection = db::connection{datasource, datasource_connection};

  return connection;
}

// db::connection getConnection(sqlite3_context* context) {
//   auto native_connection = sqlite3_context_db_handle(context);
//   auto datasource = db::sqlite::datasource{};
//   auto datasource_connection = std::make_shared<db::sqlite::datasource::connection>(native_connection);
//   auto connection = db::connection{datasource, datasource_connection};

//   return connection;
// }

std::string getString(sqlite3_value* value) {
  auto text_ptr = sqlite3_value_text(value);
  auto byte_count = sqlite3_value_bytes(value);

  return {reinterpret_cast<const char*>(text_ptr), static_cast<std::string::size_type>(byte_count)};
}

void createScalarFunction(
    db::connection& _conn, const std::string& name, std::function<void(sqlite3*, sqlite3_context*, int, sqlite3_value**)> func) {
  auto conn = _conn.getNativeConnection<datasource::connection>();

  struct context_t {
    std::function<void(sqlite3*, sqlite3_context*, int, sqlite3_value**)> func;
    sqlite3* conn;
  };

  auto context = new context_t();
  context->func = std::move(func);
  context->conn = &*conn->_native_connection;

  auto xFunc = [](sqlite3_context* _context, int argc, sqlite3_value** argv) {
    try {
      auto context = (context_t*)sqlite3_user_data(_context);

      context->func(context->conn, _context, argc, argv);
    } catch (const std::exception& error) {
      sqlite3_result_error(_context, error.what(), -1);
    } catch (...) {
      sqlite3_result_error(_context, nullptr, -1);
    }
  };

  auto xDestroy = [](void* context) {
    delete (context_t*)context;
  };

  // sqlite3_create_function_v2(
  //     &*conn->_native_connection, name.data(), -1, SQLITE_UTF8, context, xFunc, nullptr, nullptr, xDestroy);
  sqlite3_create_function(
    &*conn->_native_connection, name.data(), -1, SQLITE_UTF8, context, xFunc, nullptr, nullptr);
}

// void trace(db::connection& _conn, unsigned int mask, std::function<void(unsigned int, void*, void*)> callback) {
//   auto conn = _conn.getNativeConnection<datasource::connection>();

//   struct context_t {
//     std::function<void(unsigned int, void*, void*)> callback;
//   };

//   auto context = new context_t();
//   context->callback = std::move(callback);

//   auto xCallback = [](unsigned int mask, void* context, void* P, void* X) {
//     try {
//       ((context_t*)context)->callback(mask, P, X);

//       return 0;
//     } catch (...) {
//       return 0;
//     }
//   };

//   sqlite3_error::assert(
//       sqlite3_trace_v2(&*conn->_native_connection, mask, xCallback, context), conn->_native_connection);
// }

void profile(db::connection& _conn, std::function<void(const char*, uint64_t)> _callback) {
  auto conn = _conn.getNativeConnection<datasource::connection>();

  struct context_t {
    decltype(_callback) callback;
  };

  auto context = new context_t();
  context->callback = std::move(_callback);

  auto xProfile = [](void* context, const char* script, sqlite3_uint64 duration) -> void {
    ((context_t*)context)->callback(script, duration);
  };

  sqlite3_profile(&*conn->_native_connection, xProfile, context);
}

void profile(db::connection& _conn, std::ostream& out) {
  profile(_conn, [&out](const char* script, uint64_t) {
    out << script << std::endl;
  });
}

// void wal(db::connection& _conn, std::function<bool(db::connection&, const char*, int)> _hook) {
//   _conn.execute("PRAGMA journal_mode = WAL");

//   auto conn = _conn.getNativeConnection<datasource::connection>();

//   struct context_t {
//     decltype(_hook) hook;
//     db::connection* conn;
//   };

//   auto context = new context_t();
//   context->hook = std::move(_hook);
//   context->conn = &_conn;

//   auto xHook = [](void* context, sqlite3* conn, const char* dbname, int pages) -> int {
//     auto ctx = (context_t*)context;
//     auto res = ctx->hook(*ctx->conn, dbname, pages);

//     return res ? SQLITE_OK : SQLITE_ERROR;
//   };

//   sqlite3_wal_hook(&*conn->_native_connection, xHook, context);
// }

// void wal(db::connection& _conn) {
//   wal(_conn, [](db::connection& conn, const char* dbname, int pages) {
//     if (pages > 500) {
//       conn.execute("PRAGMA wal_checkpoint(PASSIVE)");
//     }

//     return true;
//   });
// }
} // namespace db::sqlite
