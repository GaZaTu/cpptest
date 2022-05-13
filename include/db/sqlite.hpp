#pragma once

#include "./datasource.hpp"
#include "./connection.hpp"
#include "./statement.hpp"
#include "./pool.hpp"
#include "sqlite3.h"
#include <sstream>

namespace db::sqlite {
constexpr char DRIVER_NAME[] = "SQLITE";

class sqlite3_error : public db::sql_error {
public:
  sqlite3_error(const std::string& msg);

  sqlite3_error(int code);

  sqlite3_error(sqlite3* db);

  sqlite3_error(std::shared_ptr<sqlite3> db);

  static void _assert(int code);

  static void _assert(int code, sqlite3* db);

  static void _assert(int code, std::shared_ptr<sqlite3> db);
};

class datasource : public db::datasource {
public:
  class resultset : public db::datasource::resultset {
  public:
    resultset(std::shared_ptr<sqlite3> native_connection, std::shared_ptr<sqlite3_stmt> native_statement);

    virtual ~resultset() override;

    bool next() override;

    orm::field_type getValueType(std::string_view name) override;

    bool isValueNull(std::string_view name) override;

    void getValue(std::string_view name, int32_t& result) override;

    void getValue(std::string_view name, int64_t& result) override;

    void getValue(std::string_view name, double& result) override;

    void getValue(std::string_view name, std::string_view& result) override;

    void getValue(std::string_view name, const uint8_t*& result, size_t& length, bool) override;

    int columnCount() override;

    std::string columnName(int i) override;

  public:
    std::shared_ptr<sqlite3> _native_connection;
    std::shared_ptr<sqlite3_stmt> _native_statement;

    int _code;
    std::unordered_map<std::string_view, int> _cols;
    bool _first_row = true;
  };

  class statement : public db::datasource::statement {
  public:
    statement(std::shared_ptr<sqlite3> native_connection, std::string_view script);

    virtual ~statement() override;

    std::shared_ptr<db::datasource::resultset> execute() override;

    int executeUpdate() override;

    void setParamToNull(std::string_view name) override;

    void setParam(std::string_view name, int32_t value) override;

    void setParam(std::string_view name, int64_t value) override;

    void setParam(std::string_view name, double value) override;

    void setParam(std::string_view name, std::string_view value) override;

    void setParam(std::string_view name, const uint8_t* value, size_t length, bool) override;

    bool readonly() override;

  public:
    std::shared_ptr<sqlite3> _native_connection;
    std::shared_ptr<sqlite3_stmt> _native_statement;

    int parameterIndex(std::string_view name);
  };

  class connection : public db::datasource::connection {
  public:
    connection(sqlite3 *connection);

    connection(std::string_view filename, int flags);

    virtual ~connection() override;

    std::shared_ptr<db::datasource::statement> prepareStatement(std::string_view script) override;

    void beginTransaction() override;

    void commit() override;

    void rollback() override;

    bool readonlyTransaction() override;

    void execute(std::string_view script) override;

    int getVersion() override;

    void setVersion(int version) override;

    bool supportsORM() override;

    void createInsertScript(const orm::query_builder_data& data, std::stringstream& str, int& param);

    std::string createInsertScript(const orm::query_builder_data& data, int& param) override;

    void createUpdateScript(const orm::query_builder_data& data, std::stringstream& str, int& param);

    std::string createUpdateScript(const orm::query_builder_data& data, int& param) override;

    std::string createUpsertScript(const orm::query_builder_data& _data, int& param) override;

    std::string createSelectScript(const orm::query_builder_data& data, int& param) override;

    std::string createDeleteScript(const orm::query_builder_data& data, int& param) override;

    virtual bool readonly() override;

    virtual int64_t changes() override;

  public:
    std::shared_ptr<sqlite3> _native_connection;
  };

  explicit datasource();

  explicit datasource(std::string_view filename, int flags = default_flags);

  virtual std::string_view driver() override;

  virtual std::shared_ptr<db::datasource::connection> getConnection() override;

private:
  std::string _filename;
  int _flags;

  static const int default_flags = SQLITE_OPEN_URI | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
};

namespace pooled {
using datasource = db::pooled::datasource<db::sqlite::datasource>;
}

db::connection convertConnection(sqlite3* native_connection);

sqlite3* convertConnection(db::connection& connection);

db::connection getConnection(sqlite3_context* context);

void pragma(db::connection& conn, std::string_view name, std::string_view value);

std::string pragma(db::connection& conn, std::string_view name);

// void configure(db::connection& _conn, int op);

std::string getString(sqlite3_value* value);

void createScalarFunction(
    db::connection& _conn, std::string_view name, std::function<void(sqlite3*, sqlite3_context*, int, sqlite3_value**)> func);

// void trace(db::connection& _conn, unsigned int mask, std::function<void(unsigned int, void*, void*)> callback);

void profile(db::connection& _conn, std::function<void(std::string_view, uint64_t)> _callback);

void profile(db::connection& _conn, std::ostream& out);

// void wal(db::connection& _conn, std::function<bool(db::connection&, const char*, int)> _hook);

void loadExtension(db::connection& conn, std::string_view path, std::string_view proc);

void loadExtension(db::connection& conn, int (*init)(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi));
} // namespace db::sqlite
