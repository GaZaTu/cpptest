#include "db/pgsql.hpp"
#include "db/orm.hpp"
#include <limits>
#include <bitset>
#include <unordered_set>

namespace db::pgsql {
pgsql_error::pgsql_error(const std::string& msg) : db::sql_error(msg) {
}

pgsql_error::pgsql_error(std::shared_ptr<PGconn> db) : db::sql_error(PQerrorMessage(&*db)) {
}

pgsql_error::pgsql_error(std::shared_ptr<PGresult> res) : db::sql_error(PQresultErrorMessage(&*res)) {
}

void pgsql_error::_assert(std::shared_ptr<PGconn> db) {
  ConnStatusType status = PQstatus(&*db);
  if (status != CONNECTION_OK) {
    throw pgsql_error(db);
  }
}

void pgsql_error::_assert(std::shared_ptr<PGresult> res) {
  ExecStatusType status = PQresultStatus(&*res);
  if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK && status != PGRES_EMPTY_QUERY) {
    throw pgsql_error(res);
  }
}

datasource::resultset::resultset(std::shared_ptr<PGconn> native_connection, std::shared_ptr<PGresult> native_resultset)
    : _native_connection(native_connection), _native_resultset(native_resultset) {
  for (int i = 0, col_count = PQnfields(&*_native_resultset); i < col_count; i++) {
    _cols[PQfname(&*_native_resultset, i)] = i;
  }
}

datasource::resultset::~resultset() {
}

bool datasource::resultset::next() {
  return ++_row < PQntuples(&*_native_resultset);
}

bool datasource::resultset::isValueNull(std::string_view name) {
  return PQgetisnull(&*_native_resultset, _row, _cols[name]);
}

void datasource::resultset::getValue(std::string_view name, std::string_view& result) {
  auto text_ptr = PQgetvalue(&*_native_resultset, _row, _cols[name]);
  auto text_len = PQgetlength(&*_native_resultset, _row, _cols[name]);

  result = {(const char*)text_ptr, (std::string_view::size_type)text_len};
}

void datasource::resultset::getValue(std::string_view _name, const uint8_t*& result, size_t& length, bool isnumber) {
  auto name = (std::string)_name;

  auto bits_ptr = PQgetvalue(&*_native_resultset, _row, _cols[name]);
  auto bits_len = PQgetlength(&*_native_resultset, _row, _cols[name]);

  constexpr auto uint8_bitc = std::numeric_limits<uint8_t>::digits;

  auto blob_len = bits_len / uint8_bitc;

  _blobs[name] = std::vector<uint8_t>{};
  auto& blob_ptr = _blobs[name];
  blob_ptr.resize(blob_len);

  for (size_t i = 0; i < bits_len; i += uint8_bitc) {
    blob_ptr[i / uint8_bitc] = (uint8_t)std::bitset<uint8_bitc>{bits_ptr + i, uint8_bitc}.to_ulong();
  }

  result = blob_ptr.data();
  length = blob_len;
}

int datasource::resultset::columnCount() {
  return PQnfields(&*_native_resultset);
}

std::string datasource::resultset::columnName(int i) {
  return PQfname(&*_native_resultset, i);
}

datasource::statement::statement(std::shared_ptr<PGconn> native_connection, std::string_view script)
    : _native_connection(native_connection) {
  _statement = replaceNamedParams((std::string)script);
  _statement_hash = "query" + std::to_string(std::hash<std::string>{}(_statement));
}

std::unordered_map<std::string, int> prepared_statements;

datasource::statement::~statement() {
  if (!_prepared) {
    return;
  }

  prepared_statements.at(_statement_hash) -= 1;

  if (_failed) {
    return;
  }

  for (auto it = prepared_statements.cbegin(); it != prepared_statements.cend();) {
    const auto& statement_hash = it->first;
    const auto& inuse = it->second;

    if (inuse) {
      ++it;
      continue;
    }

    std::string script{"DEALLOCATE " + statement_hash};
    std::shared_ptr<PGresult> exec_result{PQexec(&*_native_connection, script.data()), &PQclear};
    pgsql_error::_assert(exec_result);

    it = prepared_statements.erase(it);
  }
}

std::shared_ptr<db::datasource::resultset> datasource::statement::execute() {
  return std::make_shared<resultset>(_native_connection, runPrepareAndExec());
}

int datasource::statement::executeUpdate() {
  runPrepareAndExec();
  return -1;
}

void datasource::statement::setParamToNull(std::string_view name) {
  pushParam(name, "null", types::_unknown);
}

void datasource::statement::setParam(std::string_view name, bool value) {
  pushParam(name, std::to_string(value), types::_bool);
}

void datasource::statement::setParam(std::string_view name, int32_t value) {
  pushParam(name, std::to_string(value), types::_int4);
}

void datasource::statement::setParam(std::string_view name, int64_t value) {
  pushParam(name, std::to_string(value), types::_int8);
}

void datasource::statement::setParam(std::string_view name, double value) {
  pushParam(name, std::to_string(value), types::_float8);
}

void datasource::statement::setParam(std::string_view name, std::string_view value) {
  pushParam(name, (std::string)value, types::_text);
}

void datasource::statement::setParam(std::string_view name, std::string&& value) {
  pushParam(name, std::move(value), types::_text);
}

void datasource::statement::setParam(std::string_view name, const uint8_t* value, size_t length, bool isnumber) {
  constexpr auto uint8_bitc = std::numeric_limits<uint8_t>::digits;

  std::string bitstr;
  for (size_t i = 0; i < length; i++) {
    bitstr += std::bitset<uint8_bitc>{value[i]}.to_string();
  }

  pushParam(name, std::move(bitstr), isnumber ? types::_bit : types::_bytea, false);
}

void datasource::statement::setParam(std::string_view name, orm::date value) {
  pushParam(name, (std::string)value, types::_date);
}

void datasource::statement::setParam(std::string_view name, orm::time value) {
  pushParam(name, (std::string)value, types::_time);
}

void datasource::statement::setParam(std::string_view name, orm::timestamp value) {
  pushParam(name, (std::string)value, types::_timestamptz);
}

std::shared_ptr<PGresult> datasource::statement::runPrepareAndExec() {
  try {
    if (!_prepared) {
      _prepared = prepared_statements.count(_statement_hash);
      if (_prepared) {
        prepared_statements.at(_statement_hash) += 1;
      }
    }

    if (!_prepared) {
      std::shared_ptr<PGresult> prep_result{PQprepare(&*_native_connection, _statement_hash.data(), _statement.data(),
                                                _params.size(), _param_types.data()),
          &PQclear};
      pgsql_error::_assert(prep_result);

      _prepared = true;

      prepared_statements.emplace(_statement_hash, 1);
    }

    std::shared_ptr<PGresult> exec_result{
        PQexecParams(&*_native_connection, _statement.data(), _params.size(), _param_types.data(), _param_pointers.data(),
            _param_lengths.data(), _param_formats.data(), 0),
        &PQclear};
    pgsql_error::_assert(exec_result);

    return exec_result;
  } catch (...) {
    _failed = true;
    throw;
  }
}

void datasource::statement::pushParam(std::string_view name, std::string&& value, Oid type, bool binary) {
  size_t i = _params_map[(std::string)name] - 1;
  if (i == -1) {
    // throw sql_error{"parameter " + (std::string)name + " does not exist"};
    return;
  }

  _params[i] = std::move(value);
  _param_types[i] = type;
  _param_pointers[i] = _params[i].data();
  _param_lengths[i] = _params[i].length();
  _param_formats[i] = (int)binary;
}

std::string datasource::statement::replaceNamedParams(std::string&& script) {
  char in_string = 0;
  size_t param_idx = -1;
  size_t param_len = -1;

  for (size_t i = 0; i <= script.length(); i++) {
    switch (script[i]) {
    case '\'':
    case '\"':
      if (in_string && in_string != script[i]) {
        break;
      }

      in_string = in_string ? 0 : script[i];

      break;
    case ':':
    case '$':
    case '@':
      if (in_string) {
        break;
      }

      param_idx = i;
      while (std::isalnum(script[++i])) {}
      param_len = i - param_idx;
      if (param_len == 1) {
        break;
      }

      size_t idx = 0;
      std::string name = script.substr(param_idx, param_len);

      auto search = _params_map.find(name);
      if (search != _params_map.end()) {
        idx = search->second;
      } else {
        idx = _params_map.size() + 1;
        _params_map[std::move(name)] = idx;
      }

      std::string key = std::string{'$'} + std::to_string(idx);

      script.replace(param_idx, param_len, key);
      i = param_idx + key.length();

      break;
    }
  }

  for (int i = 0; i < _params_map.size(); i++) {
    _params.emplace_back();
  }

  _param_types.resize(_params_map.size());
  _param_pointers.resize(_params_map.size());
  _param_lengths.resize(_params_map.size());
  _param_formats.resize(_params_map.size());

  return script;
}

datasource::connection::connection(std::string_view _conninfo) {
  std::string conninfo = (std::string)_conninfo;
  _native_connection = std::shared_ptr<PGconn>{PQconnectdb(conninfo.data()), &PQfinish};
  pgsql_error::_assert(_native_connection);
}

datasource::connection::~connection() {
}

std::shared_ptr<db::datasource::statement> datasource::connection::prepareStatement(std::string_view script) {
  if (onPrepareStatement()) {
    onPrepareStatement()(script);
  }

  return std::make_shared<statement>(_native_connection, script);
}

void datasource::connection::beginTransaction() {
  execute("BEGIN TRANSACTION");
}

void datasource::connection::commit() {
  execute("COMMIT");
}

void datasource::connection::rollback() {
  execute("ROLLBACK");
}

void datasource::connection::execute(std::string_view _script) {
  std::string script = (std::string)_script;
  std::shared_ptr<PGresult> result{PQexec(&*_native_connection, script.data()), &PQclear};
  pgsql_error::_assert(result);
}

int datasource::connection::getVersion() {
  execute("CREATE TABLE IF NOT EXISTS __user_version (version INTEGER)");
  execute("INSERT INTO __user_version SELECT 0 WHERE NOT EXISTS (SELECT * FROM __user_version)");

  auto statement = prepareStatement("SELECT version FROM __user_version");
  auto resultset = statement->execute();

  int version = 0;
  resultset->next();
  resultset->getValue(resultset->columnName(0), version);

  return version;
}

void datasource::connection::setVersion(int version) {
  auto statement = prepareStatement("UPDATE __user_version SET version = :version");
  statement->setParam(":version", version);

  statement->executeUpdate();
}

void datasource::connection::createInsertScript(const orm::query_builder_data& data, std::stringstream& str, int& param) {
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

std::string datasource::connection::createInsertScript(const orm::query_builder_data& data, int& param) {
  std::stringstream str;
  createInsertScript(data, str, param);
  return str.str();
}

void datasource::connection::createUpdateScript(const orm::query_builder_data& data, std::stringstream& str, int& param) {
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

std::string datasource::connection::createUpdateScript(const orm::query_builder_data& data, int& param) {
  std::stringstream str;
  createUpdateScript(data, str, param);
  return str.str();
}

std::string datasource::connection::createUpsertScript(const orm::query_builder_data& _data, int& param) {
  auto data = _data;

  std::stringstream str;

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

  if (data.upsert == orm::query_builder_data::UP_NOTHING) {
    str << "NOTHING";
  } else if (data.upsert == orm::query_builder_data::UP_REPLACE) {
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
  } else if (data.upsert == orm::query_builder_data::UP_UPDATE) {
    // TODO
  } else {
    str << " your_mom?";
  }

  return str.str();
}

std::string datasource::connection::createSelectScript(const orm::query_builder_data& data, int& param) {
  std::stringstream str;

  str << "SELECT ";

  if (data.distinct) {
    str << "DISTINCT ";
  }

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
    switch (data.joins.at(i).mode) {
    case db::orm::JOIN_INNER:
      str << " JOIN ";
      break;
    case db::orm::JOIN_LEFT:
      str << " LEFT JOIN ";
      break;
    case db::orm::JOIN_CROSS:
      str << " CROSS JOIN ";
      break;
    }

    str << (std::string)data.joins.at(i).table;

    if (data.joins.at(i).condition != nullptr) {
      str << " ON ";

      data.joins.at(i).condition->appendToQuery(str, param);
    }
  }

  for (size_t i = 0; i < data.conditions.size(); i++) {
    if (i > 0) {
      str << " AND ";
    } else {
      str << " WHERE ";
    }

    data.conditions.at(i)->appendToQuery(str, param);
  }

  for (size_t i = 0; i < data.grouping.size(); i++) {
    if (i > 0) {
      str << ", ";
    } else {
      str << " GROUP BY ";
    }

    str << data.grouping.at(i);
  }

  for (size_t i = 0; i < data.having_conditions.size(); i++) {
    if (i > 0) {
      str << " AND ";
    } else {
      str << " HAVING ";
    }

    data.having_conditions.at(i)->appendToQuery(str, param);
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

  if (data.offset > 0) {
    str << " OFFSET " << data.offset;
  }

  return str.str();
}

std::string datasource::connection::createDeleteScript(const orm::query_builder_data& data, int& param) {
  std::stringstream str;

  str << "DELETE FROM " << (std::string)data.table;

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

datasource::datasource(std::string_view conninfo) : _conninfo(conninfo) {
}

std::shared_ptr<db::datasource::connection> datasource::getConnection() {
  return std::make_shared<connection>(_conninfo);
}

std::string_view datasource::driver() {
  return DRIVER_NAME;
}
}
