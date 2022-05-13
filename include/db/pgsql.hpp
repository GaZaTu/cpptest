#pragma once

#include "./datasource.hpp"
#include "./connection.hpp"
#include "./statement.hpp"
#include "./pool.hpp"
// #include <pgsql/libpq-fe.h>
#include <postgresql/libpq-fe.h>
#include <sstream>

namespace db::pgsql {
constexpr char DRIVER_NAME[] = "PGSQL";

namespace types {
constexpr Oid _bool = 16;
constexpr Oid _bytea = 17;
constexpr Oid _char = 18; //  8-bit
constexpr Oid _name = 19;
constexpr Oid _int8 = 20; // 64-bit
constexpr Oid _int2 = 21; // 16-bit
constexpr Oid _int4 = 23; // 32-bit
constexpr Oid _text = 25;
constexpr Oid _oid = 26;
constexpr Oid _xid = 28;
constexpr Oid _cid = 29;

constexpr Oid _float4 = 700; // 32-bit
constexpr Oid _float8 = 701; // 64-bit
constexpr Oid _unknown = 705;

constexpr Oid _date = 1082;
constexpr Oid _time = 1083;

constexpr Oid _timestamp = 1114;
constexpr Oid _timestamptz = 1184;

constexpr Oid _bit = 1560;

constexpr Oid _numeric = 1700;

constexpr Oid _uuid = 2950;
} // namespace types

class pgsql_error : public db::sql_error {
public:
  pgsql_error(const std::string& msg);

  pgsql_error(std::shared_ptr<PGconn> db);

  pgsql_error(std::shared_ptr<PGresult> res);

  static void _assert(std::shared_ptr<PGconn> db);

  static void _assert(std::shared_ptr<PGresult> res);
};

class datasource : public db::datasource {
public:
  class resultset : public db::datasource::resultset {
  public:
    resultset(std::shared_ptr<PGconn> native_connection, std::shared_ptr<PGresult> native_resultset);

    virtual ~resultset() override;

    bool next() override;

    bool isValueNull(std::string_view name) override;

    void getValue(std::string_view name, std::string_view& result) override;

    void getValue(std::string_view _name, const uint8_t*& result, size_t& length, bool isnumber) override;

    int columnCount() override;

    std::string columnName(int i) override;

  private:
    std::shared_ptr<PGconn> _native_connection;
    std::shared_ptr<PGresult> _native_resultset;

    std::unordered_map<std::string_view, int> _cols;

    std::unordered_map<std::string, std::vector<uint8_t>> _blobs;

    int _row = -1;
  };

  class statement : public db::datasource::statement {
  public:
    statement(std::shared_ptr<PGconn> native_connection, std::string_view script);

    virtual ~statement() override;

    std::shared_ptr<db::datasource::resultset> execute() override;

    int executeUpdate() override;

    void setParamToNull(std::string_view name) override;

    void setParam(std::string_view name, bool value) override;

    void setParam(std::string_view name, int32_t value) override;

    void setParam(std::string_view name, int64_t value) override;

    void setParam(std::string_view name, double value) override;

    void setParam(std::string_view name, std::string_view value) override;

    void setParam(std::string_view name, std::string&& value) override;

    void setParam(std::string_view name, const uint8_t* value, size_t length, bool isnumber) override;

    void setParam(std::string_view name, orm::date value) override;

    void setParam(std::string_view name, orm::time value) override;

    void setParam(std::string_view name, orm::timestamp value) override;

  private:
    std::shared_ptr<PGconn> _native_connection;

    std::string _statement;
    std::string _statement_hash;

    bool _prepared = false;
    bool _failed = false;

    std::unordered_map<std::string, size_t> _params_map;

    std::vector<std::string> _params;
    std::vector<Oid> _param_types;
    std::vector<const char*> _param_pointers;
    std::vector<int> _param_lengths;
    std::vector<int> _param_formats;

    std::shared_ptr<PGresult> runPrepareAndExec();

    void pushParam(std::string_view name, std::string&& value, Oid type, bool binary = false);

    std::string replaceNamedParams(std::string&& script);
  };

  class connection : public db::datasource::connection {
  public:
    connection(std::string_view _conninfo);

    virtual ~connection() override;

    std::shared_ptr<db::datasource::statement> prepareStatement(std::string_view script) override;

    void beginTransaction() override;

    void commit() override;

    void rollback() override;

    void execute(std::string_view _script) override;

    int getVersion() override;

    void setVersion(int version) override;

    void createInsertScript(const orm::query_builder_data& data, std::stringstream& str, int& param);

    std::string createInsertScript(const orm::query_builder_data& data, int& param) override;

    void createUpdateScript(const orm::query_builder_data& data, std::stringstream& str, int& param);

    std::string createUpdateScript(const orm::query_builder_data& data, int& param) override;

    std::string createUpsertScript(const orm::query_builder_data& _data, int& param) override;

    std::string createSelectScript(const orm::query_builder_data& data, int& param) override;

    std::string createDeleteScript(const orm::query_builder_data& data, int& param) override;

  private:
    std::shared_ptr<PGconn> _native_connection;
  };

  datasource(std::string_view conninfo);

  std::shared_ptr<db::datasource::connection> getConnection() override;

  std::string_view driver() override;

private:
  std::string _conninfo;
};

namespace pooled {
using datasource = db::pooled::datasource<db::pgsql::datasource>;
}
} // namespace db::pq
