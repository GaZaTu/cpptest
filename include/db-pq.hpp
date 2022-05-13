#pragma once

#include "./db/datasource.hpp"
#include <pgsql/libpq-fe.h>
#include <sstream>

namespace db::pq {
namespace types {
constexpr int _bool = 16;
constexpr int _bytea = 17;
constexpr int _char = 18;
constexpr int _name = 19;
constexpr int _int8 = 20; // 64-bit
constexpr int _int2 = 21; // 16-bit
constexpr int _int4 = 23; // 32-bit
constexpr int _text = 25;
constexpr int _oid = 26;
constexpr int _xid = 28;
constexpr int _cid = 29;

constexpr int _float4 = 700; // 32-bit
constexpr int _float8 = 701; // 64-bit
constexpr int _unknown = 705;

constexpr int _date = 1082;
constexpr int _time = 1083;

constexpr int _timestamp = 1114;
constexpr int _timestamptz = 1184;

constexpr int _bit = 1560;

constexpr int _numeric = 1700;

constexpr int _uuid = 2950;
} // namespace types

class pq_error : public db::sql_error {
public:
  pq_error(const std::string& msg) : db::sql_error(msg) {
  }

  pq_error(std::shared_ptr<PGconn> db) : db::sql_error(PQerrorMessage(&*db)) {
  }

  static void assert(std::shared_ptr<PGconn> db) {
    if (PQstatus(&*db) != CONNECTION_OK) {
      throw pq_error(db);
    }
  }

  static void assert(std::shared_ptr<PGresult> res, std::shared_ptr<PGconn> db) {
    ExecStatusType status = PQresultStatus(&*res);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
      // PQresultErrorMessage
      throw pq_error(db);
    }
  }
};

class datasource : public db::datasource {
public:
  class resultset : public db::datasource::resultset {
  public:
    resultset(std::shared_ptr<PGconn> native_connection, std::shared_ptr<PGresult> native_resultset)
        : _native_connection(native_connection), _native_resultset(native_resultset) {
      for (int i = 0, col_count = PQnfields(&*_native_resultset); i < col_count; i++) {
        _cols[PQfname(&*_native_resultset, i)] = i;
      }
    }

    virtual ~resultset() override {
    }

    bool next() override {
      _row += 1;
      return true;
    }

    bool isValueNull(const std::string_view name) override {
      return PQgetisnull(&*_native_resultset, _row, _cols[name]);
    }

    void getValue(const std::string_view name, bool& result) override {
      result = *PQgetvalue(&*_native_resultset, _row, _cols[name]) != '0';
    }

    bool getValue(int col, int& result) {
      result = std::atoi(PQgetvalue(&*_native_resultset, _row, col));
    }

    void getValue(const std::string_view name, int& result) override {
      getValue(_cols[name], result);
    }

    void getValue(const std::string_view name, int64_t& result) override {
      result = std::atoll(PQgetvalue(&*_native_resultset, _row, _cols[name]));
    }

#ifdef __SIZEOF_INT128__
    void getValue(const std::string_view name, __uint128_t& result) override {
      const void* bytes;
      getValue(name, &bytes);
      result = *(__uint128_t*)bytes;
    }
#endif

    void getValue(const std::string_view name, double& result) override {
      result = std::atof(PQgetvalue(&*_native_resultset, _row, _cols[name]));
    }

    void getValue(const std::string_view name, std::string& result) override {
      auto col = _cols[name];
      auto text_ptr = PQgetvalue(&*_native_resultset, _row, col);
      auto byte_count = PQgetlength(&*_native_resultset, _row, col);

      result = {reinterpret_cast<const char*>(text_ptr), static_cast<std::string::size_type>(byte_count)};
    }

    void getValue(const std::string_view name, std::vector<uint8_t>& result) override {
      auto col = _cols[name];
      auto blob_ptr = PQgetvalue(&*_native_resultset, _row, col);
      auto byte_count = PQgetlength(&*_native_resultset, _row, col);
      auto bytes_ptr = reinterpret_cast<const uint8_t*>(blob_ptr);

      result = {bytes_ptr, bytes_ptr + byte_count};
    }

    void getValue(const std::string_view name, const void** result) {
      *result = (const void*)PQgetvalue(&*_native_resultset, _row, _cols[name]);
    }

    int columnCount() override {
      return PQnfields(&*_native_resultset);
    }

    std::string columnName(int i) override {
      return PQfname(&*_native_resultset, i);
    }

  private:
    std::shared_ptr<PGconn> _native_connection;
    std::shared_ptr<PGresult> _native_resultset;

    std::unordered_map<std::string_view, int> _cols;

    int _row = -1;
  };

  class statement : public db::datasource::statement {
  public:
    statement(std::shared_ptr<PGconn> native_connection, const std::string_view script)
        : _native_connection(native_connection) {
      _statement = replaceNamedParams((std::string)script);
      _statement_hash = std::to_string(std::hash<std::string>{}(_statement));

      resizeParams(_params_map.size());
    }

    virtual ~statement() override {
    }

    std::shared_ptr<db::datasource::resultset> execute() override {
      return std::make_shared<resultset>(_native_connection, pgPrepareAndExec());
    }

    std::shared_ptr<PGresult> pgPrepareAndExec() {
      if (!_prepared) {
        _prepared = true;

        std::shared_ptr<PGresult> prep_result{PQprepare(&*_native_connection, _statement_hash.data(), _statement.data(),
                                                  _params.size(), _param_types.data()),
            &PQclear};
        pq_error::assert(prep_result, _native_connection);
      }

      std::shared_ptr<PGresult> exec_result{
          PQexecPrepared(&*_native_connection, _statement_hash.data(), _params.size(), _param_pointers.data(),
              _param_lengths.data(), _param_formats.data(), 0),
          &PQclear};
      pq_error::assert(exec_result, _native_connection);

      return exec_result;
    }

    int executeUpdate() override {
      pgPrepareAndExec();

      return -1;
    }

    void setParamToNull(const std::string_view name) override {
      pushParam(name, "null", types::_unknown);
    }

    void setParam(const std::string_view name, bool value) override {
      pushParam(name, std::to_string(value), types::_bool);
    }

    void setParam(const std::string_view name, int value) override {
      pushParam(name, std::to_string(value), types::_int4);
    }

    void setParam(const std::string_view name, int64_t value) override {
      pushParam(name, std::to_string(value), types::_int8);
    }

#ifdef __SIZEOF_INT128__
    void setParam(const std::string_view name, __uint128_t value) override {
      setParam(name, (const void*)&value);
    }
#endif

    void setParam(const std::string_view name, double value) override {
      pushParam(name, std::to_string(value), types::_float8);
    }

    void setParam(const std::string_view name, const std::string_view value) override {
      pushParam(name, (std::string)value, types::_text);
    }

    void setParam(const std::string_view name, const std::vector<uint8_t>& value) override {
      pushParam(name, (const char*)value.data(), types::_bytea, true);
    }

    void setParam(const std::string_view name, orm::date value) override {
      pushParam(name, (std::string)value, types::_date);
    }

    void setParam(const std::string_view name, orm::time value) override {
      pushParam(name, (std::string)value, types::_time);
    }

    void setParam(const std::string_view name, orm::timestamp value) override {
      pushParam(name, (std::string)value, types::_timestamp);
    }

    void setParam(const std::string_view name, const void* data) {
      pushParam(name, (const char*)data, types::_bit, true);
    }

  private:
    std::shared_ptr<PGconn> _native_connection;

    std::string _statement;
    std::string _statement_hash;

    bool _prepared = false;

    std::unordered_map<std::string, size_t> _params_map;

    std::vector<std::string> _params;
    std::vector<Oid> _param_types;
    std::vector<const char*> _param_pointers;
    std::vector<int> _param_lengths;
    std::vector<int> _param_formats;

    void resizeParams(size_t n) {
      _params.resize(n);

      _param_types.resize(n);
      _param_pointers.resize(n);
      _param_lengths.resize(n);
      _param_formats.resize(n);
    }

    void pushParam(std::string_view name, std::string&& value, Oid type, bool binary = false) {
      size_t i = _params_map[(std::string)name];

      _params[i] = std::move(value);
      value = _params[i];

      _param_types[i] = type;
      _param_pointers[i] = value.data();
      _param_lengths[i] = value.length();
      _param_formats[i] = (int)binary;
    }

    std::string replaceNamedParams(std::string&& script) {
      bool in_string = false;
      size_t idx_param = -1;

      for (size_t i = 0; i <= script.length(); i++) {
        switch (script[i]) {
        case '\'':
          if (script[i - 1] != '\\') {
            in_string = !in_string;
          }
          break;
        case ':':
          if (!in_string) {
            if (idx_param != -1) {
            } else if (script[i - 1] != ':') {
              char nc = script[i + 1];
              bool is_digit = (nc >= '0' && nc <= '9');
              bool is_ascii = (nc >= 'a' && nc <= 'z') || (nc >= 'A' && nc <= 'Z');

              if (is_digit || is_ascii) {
                idx_param = i;
              }

              break;
            }
          }
        case ' ':
        case '\0':
          if (idx_param != -1) {
            size_t len = i - idx_param;
            size_t idx = 0;

            std::string name = script.substr(idx_param, len);

            auto search = _params_map.find(name);
            if (search != _params_map.end()) {
              idx = search->second;
            } else {
              idx = _params_map.size() + 1;
              _params_map[std::move(name)] = idx;
            }

            std::string key = std::string{'$'} + std::to_string(idx);

            script.replace(idx_param, len, key);

            i = idx_param + key.length();
            idx_param = -1;
          }
        }
      }

      return script;
    }
  };

  class connection : public db::datasource::connection {
  public:
    connection(const std::string_view conninfo) {
      _native_connection = std::shared_ptr<PGconn>{PQconnectdb(conninfo.data()), &PQfinish};
      pq_error::assert(_native_connection);
    }

    virtual ~connection() override {
    }

    std::shared_ptr<db::datasource::statement> prepareStatement(const std::string_view script) override {
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

    void execute(const std::string_view script) override {
      std::shared_ptr<PGresult> result{PQexec(&*_native_connection, script.data()), &PQclear};
      pq_error::assert(result, _native_connection);
    }

    int getVersion() override {
      auto statement = prepareStatement("SELECT version FROM user_version");
      auto resultset = statement->execute();

      int version = 0;
      resultset->getValue(0, version);

      return version;
    }

    void setVersion(int version) override {
      auto statement = prepareStatement("UPDATE user_version SET version = :version");
      statement->setParam("version", version);

      statement->executeUpdate();
    }

    bool supportsORM() override {
      return true;
    }

    std::string createInsertScript(const char* table, const orm::field_info* fields, size_t fields_len) override {
      std::stringstream str;

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

    std::string createUpdateScript(const orm::query_builder_data& data) override {
      std::stringstream str;
      int param = 666;

      str << "UPDATE " << '"' << data.table << '"' << " SET ";

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

      return str.str();
    }

    std::string createSelectScript(const orm::query_builder_data& data) override {
      std::stringstream str;
      int param = 666;

      str << "SELECT ";

      for (size_t i = 0; i < data.fields.size(); i++) {
        if (i > 0) {
          str << ", ";
        }

        str << data.fields.at(i);
      }

      str << " FROM " << '"' << data.table << '"';

      for (size_t i = 0; i < data.conditions.size(); i++) {
        if (i > 0) {
          str << " AND ";
        } else {
          str << " WHERE ";
        }

        data.conditions.at(i)->appendToQuery(str, param);
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
      int param = 666;

      str << "DELETE FROM " << '"' << data.table << '"';

      for (size_t i = 0; i < data.conditions.size(); i++) {
        if (i > 0) {
          str << " AND ";
        } else {
          str << " WHERE ";
        }

        data.conditions.at(i)->appendToQuery(str, param);
      }

      return str.str();
    }

  private:
    std::shared_ptr<PGconn> _native_connection;
  };

  datasource(const std::string_view conninfo) : _conninfo(conninfo) {
  }

  std::shared_ptr<db::datasource::connection> getConnection() override {
    return std::make_shared<connection>(_conninfo);
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
  std::string _conninfo;
};
} // namespace db::pq
