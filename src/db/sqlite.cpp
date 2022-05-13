#include "db/sqlite.hpp"
#include "db/orm.hpp"

namespace db::sqlite {
sqlite3_error::sqlite3_error(const std::string& msg) : db::sql_error(msg) {
}

sqlite3_error::sqlite3_error(int code) : sqlite3_error(sqlite3_errstr(code)) {
}

sqlite3_error::sqlite3_error(sqlite3* db) : sqlite3_error(sqlite3_errmsg(db)) {
}

sqlite3_error::sqlite3_error(std::shared_ptr<sqlite3> db) : sqlite3_error(&*db) {
}

void sqlite3_error::_assert(int code) {
  if (code != SQLITE_OK && code != SQLITE_DONE) {
    throw sqlite3_error(code);
  }
}

void sqlite3_error::_assert(int code, sqlite3* db) {
  if (code != SQLITE_OK && code != SQLITE_DONE) {
    throw sqlite3_error(db);
  }
}

void sqlite3_error::_assert(int code, std::shared_ptr<sqlite3> db) {
  _assert(code, &*db);
}

datasource::resultset::resultset(std::shared_ptr<sqlite3> native_connection, std::shared_ptr<sqlite3_stmt> native_statement)
    : _native_connection(native_connection), _native_statement(native_statement) {
  // printf("%s\n", sqlite3_sql(&*_native_statement));
  printf("%s\n", sqlite3_expanded_sql(&*_native_statement));

  if ((_code = sqlite3_step(&*_native_statement)) == SQLITE_ERROR) {
    throw sqlite3_error(_native_connection);
  }

  for (int i = 0, col_count = sqlite3_data_count(&*_native_statement); i < col_count; i++) {
    _cols[sqlite3_column_name(&*_native_statement, i)] = i;
  }
}

datasource::resultset::~resultset() {
  sqlite3_reset(&*_native_statement);
  sqlite3_clear_bindings(&*_native_statement);
}

bool datasource::resultset::next() {
  if (_first_row) {
    _first_row = false;
    return _code == SQLITE_ROW;
  } else {
    return sqlite3_step(&*_native_statement) == SQLITE_ROW;
  }
}

orm::field_type datasource::resultset::getValueType(std::string_view name) {
  auto native_type = sqlite3_column_type(&*_native_statement, _cols[name]);

  switch (native_type) {
    case SQLITE_INTEGER: return orm::INT64;
    case SQLITE_FLOAT: return orm::DOUBLE;
    case SQLITE_TEXT: return orm::STRING;
    case SQLITE_BLOB: return orm::BLOB;
    default: return orm::UNKNOWN;
  }
}

bool datasource::resultset::isValueNull(std::string_view name) {
  return sqlite3_column_type(&*_native_statement, _cols[name]) == SQLITE_NULL;
}

void datasource::resultset::getValue(std::string_view name, int32_t& result) {
  result = sqlite3_column_int(&*_native_statement, _cols[name]);
}

void datasource::resultset::getValue(std::string_view name, int64_t& result) {
  result = sqlite3_column_int64(&*_native_statement, _cols[name]);
}

void datasource::resultset::getValue(std::string_view name, double& result) {
  result = sqlite3_column_double(&*_native_statement, _cols[name]);
}

void datasource::resultset::getValue(std::string_view name, std::string_view& result) {
  auto text_ptr = sqlite3_column_text(&*_native_statement, _cols[name]);
  auto text_len = sqlite3_column_bytes(&*_native_statement, _cols[name]);

  result = {(const char*)text_ptr, (std::string_view::size_type)text_len};
}

void datasource::resultset::getValue(std::string_view name, const uint8_t*& result, size_t& length, bool) {
  auto blob_ptr = sqlite3_column_blob(&*_native_statement, _cols[name]);
  auto blob_len = sqlite3_column_bytes(&*_native_statement, _cols[name]);

  result = (const uint8_t*)blob_ptr;
  length = (size_t)blob_len;
}

int datasource::resultset::columnCount() {
  return sqlite3_data_count(&*_native_statement);
}

std::string datasource::resultset::columnName(int i) {
  return sqlite3_column_name(&*_native_statement, i);
}

datasource::statement::statement(std::shared_ptr<sqlite3> native_connection, std::string_view script)
    : _native_connection(native_connection) {
  sqlite3_stmt* statement = nullptr;
  sqlite3_error::_assert(
      sqlite3_prepare_v2(&*_native_connection, script.data(), script.length(), &statement, nullptr),
      _native_connection);

  _native_statement = {statement, &sqlite3_finalize};
}

datasource::statement::~statement() {
}

std::shared_ptr<db::datasource::resultset> datasource::statement::execute() {
  return std::make_shared<resultset>(_native_connection, _native_statement);
}

int datasource::statement::executeUpdate() {
  sqlite3_error::_assert(sqlite3_step(&*_native_statement), _native_connection);

  sqlite3_reset(&*_native_statement);
  sqlite3_clear_bindings(&*_native_statement);

  return sqlite3_changes(&*_native_connection);
}

void datasource::statement::setParamToNull(std::string_view name) {
  int index = parameterIndex(name);
  if (index == 0) {
    return;
  }

  sqlite3_error::_assert(sqlite3_bind_null(&*_native_statement, index), _native_connection);
}

void datasource::statement::setParam(std::string_view name, int32_t value) {
  int index = parameterIndex(name);
  if (index == 0) {
    return;
  }

  sqlite3_error::_assert(sqlite3_bind_int(&*_native_statement, index, value), _native_connection);
}

void datasource::statement::setParam(std::string_view name, int64_t value) {
  int index = parameterIndex(name);
  if (index == 0) {
    return;
  }

  sqlite3_error::_assert(sqlite3_bind_int64(&*_native_statement, index, value), _native_connection);
}

void datasource::statement::setParam(std::string_view name, double value) {
  int index = parameterIndex(name);
  if (index == 0) {
    return;
  }

  sqlite3_error::_assert(sqlite3_bind_double(&*_native_statement, index, value), _native_connection);
}

void datasource::statement::setParam(std::string_view name, std::string_view value) {
  int index = parameterIndex(name);
  if (index == 0) {
    return;
  }

  sqlite3_error::_assert(sqlite3_bind_text(&*_native_statement, index, value.data(), value.length(), SQLITE_TRANSIENT), _native_connection);
}

void datasource::statement::setParam(std::string_view name, const uint8_t* value, size_t length, bool) {
  int index = parameterIndex(name);
  if (index == 0) {
    return;
  }

  sqlite3_error::_assert(sqlite3_bind_blob(&*_native_statement, index, (const void*)value, (int)length, SQLITE_TRANSIENT), _native_connection);
}

int datasource::statement::parameterIndex(std::string_view name) {
  return sqlite3_bind_parameter_index(&*_native_statement, ((std::string)name).data());
}

bool datasource::statement::readonly() {
  return sqlite3_stmt_readonly(&*_native_statement);
}

datasource::connection::connection(sqlite3 *connection) {
  _native_connection = {connection, [](sqlite3*) {}};
}

datasource::connection::connection(std::string_view _filename, int flags) {
  std::string filename = (std::string)_filename;

  sqlite3* connection = nullptr;
  sqlite3_error::_assert(sqlite3_open_v2(filename.data(), &connection, flags, nullptr), _native_connection);

  _native_connection = {connection, &sqlite3_close};

  sqlite3_extended_result_codes(&*_native_connection, true);

#if SQLITE_VERSION_NUMBER >= 3029000
  sqlite3_db_config(&*_native_connection, SQLITE_DBCONFIG_DQS_DDL, false, nullptr);
  sqlite3_db_config(&*_native_connection, SQLITE_DBCONFIG_DQS_DML, false, nullptr);
#endif

  execute("PRAGMA journal_mode = WAL");
  execute("PRAGMA synchronous = normal");
  execute("PRAGMA temp_store = memory");
  execute("PRAGMA mmap_size = 268435456");
  execute("PRAGMA cache_size = -10000");
  execute("PRAGMA foreign_keys = ON");
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

bool datasource::connection::readonlyTransaction() {
#if SQLITE_VERSION_NUMBER >= 3029000
  return (sqlite3_txn_state(&*_native_connection, "main") != SQLITE_TXN_WRITE);
#else
  return false;
#endif
}

void datasource::connection::execute(std::string_view _script) {
  std::string script = (std::string)_script;

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

int datasource::connection::getVersion() {
  auto statement = prepareStatement("PRAGMA user_version");
  auto resultset = statement->execute();

  int version = 0;
  resultset->getValue(resultset->columnName(0), version);

  return version;
}

void datasource::connection::setVersion(int version) {
  execute(std::string{"PRAGMA user_version = "} + std::to_string(version));
}

bool datasource::connection::supportsORM() {
  return true;
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

bool datasource::connection::readonly() {
  return sqlite3_db_readonly(&*_native_connection, "main");
}

int64_t datasource::connection::changes() {
  return sqlite3_total_changes(&*_native_connection);
}

datasource::datasource() {
}

datasource::datasource(std::string_view filename, int flags) : _filename(filename), _flags(flags) {
}

std::string_view datasource::driver() {
  return DRIVER_NAME;
}

std::shared_ptr<db::datasource::connection> datasource::getConnection() {
  return std::make_shared<connection>(_filename, _flags);
}

db::connection convertConnection(sqlite3* native_connection) {
  auto datasource = db::sqlite::datasource{};
  auto datasource_connection = std::make_shared<db::sqlite::datasource::connection>(native_connection);
  auto connection = db::connection{datasource, datasource_connection};

  return connection;
}

sqlite3* convertConnection(db::connection& connection) {
  auto connection_native = connection.getNativeConnection<datasource::connection>();
  auto connection_sqlite = &*connection_native->_native_connection;

  return connection_sqlite;
}

// db::statement convertStatement(sqlite3_stmt* native_statement) {
// }

sqlite3_stmt* convertStatement(db::statement& statement) {
  auto statement_native = statement.getNativeStatement<datasource::statement>();
  auto statement_sqlite = &*statement_native->_native_statement;

  return statement_sqlite;
}

db::connection getConnection(sqlite3_context* context) {
  auto connection_sqlite = sqlite3_context_db_handle(context);
  auto connection = convertConnection(connection_sqlite);

  return connection;
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

void configure(db::connection& _conn, int op) {
  auto conn = convertConnection(_conn);

  sqlite3_error::_assert(sqlite3_db_config(conn, op), conn);
}

std::string getString(sqlite3_value* value) {
  auto text_ptr = (const std::string::value_type*)sqlite3_value_text(value);
  auto text_size = (std::string::size_type)sqlite3_value_bytes(value);

  return {text_ptr, text_size};
}

void createScalarFunction(
    db::connection& _conn, std::string_view _name, std::function<void(sqlite3*, sqlite3_context*, int, sqlite3_value**)> func) {
  auto conn = convertConnection(_conn);
  auto name = (std::string)_name;

  struct context_t {
    std::function<void(sqlite3*, sqlite3_context*, int, sqlite3_value**)> func;
    sqlite3* conn;
  };

  if (!func) {
    sqlite3_create_function_v2(conn, name.data(), -1, SQLITE_UTF8, nullptr, nullptr, nullptr, nullptr, nullptr);
  }

  auto context = new context_t();
  context->func = std::move(func);
  context->conn = conn;

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

  sqlite3_create_function_v2(conn, name.data(), -1, SQLITE_UTF8, context, xFunc, nullptr, nullptr, xDestroy);
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

void profile(db::connection& _conn, std::function<void(std::string_view, uint64_t)> _callback) {
  auto conn = convertConnection(_conn);

  struct context_t {
    decltype(_callback) callback;
  };

  auto context = new context_t();
  context->callback = std::move(_callback);

  auto xProfile = [](void* context, const char* script, sqlite3_uint64 duration) -> void {
    ((context_t*)context)->callback(script, duration);
  };

  sqlite3_profile(conn, xProfile, context);
}

void profile(db::connection& _conn, std::ostream& out) {
  profile(_conn, [&out](auto script, auto) {
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

void loadExtension(db::connection& _conn, std::string_view path, std::string_view proc) {
  auto conn = convertConnection(_conn);

  int rc = SQLITE_OK;
  char* errstr = nullptr;

  rc = sqlite3_enable_load_extension(conn, true);
  if (rc != SQLITE_OK) {
    throw sqlite3_error{rc};
  }

  rc = sqlite3_load_extension(conn, path.data(), proc.data(), &errstr);
  if (rc != SQLITE_OK && rc != SQLITE_OK_LOAD_PERMANENTLY) {
    if (errstr != nullptr) {
      throw sqlite3_error{errstr};
    } else {
      throw sqlite3_error{rc};
    }
  }
}

void loadExtension(db::connection& _conn, int (*init)(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi)) {
  auto conn = convertConnection(_conn);

  int rc = SQLITE_OK;
  char* errstr = nullptr;

  rc = init(conn, &errstr, nullptr);
  if (rc != SQLITE_OK && rc != SQLITE_OK_LOAD_PERMANENTLY) {
    if (errstr != nullptr) {
      throw sqlite3_error{errstr};
    } else {
      throw sqlite3_error{rc};
    }
  }
}

// void bindZeroBlob(db::statement& _stmt, std::string_view _name, int64_t size) {
//   auto stmt = convertStatement(_stmt);
//   auto conn = sqlite3_db_handle(stmt);
//   auto name = (std::string)name;

//   int index = sqlite3_bind_parameter_index(stmt, name.data());
//   if (index == 0) {
//     return;
//   }

//   sqlite3_error::_assert(sqlite3_bind_zeroblob64(stmt, index, size), conn);
// }

// class blob_buffer : public std::streambuf {
// public:
//   blob_buffer(sqlite3_blob* native_blob) : _native_blob(native_blob) {
//     _size = sqlite3_blob_bytes(&*_native_blob);
//   }

// protected:
//   virtual pos_type seekoff(off_type offset, std::ios_base::seekdir, std::ios_base::openmode) {
//     if (offset > _size) {
//       return pos_type(off_type(-1));
//     }

//     return pos_type(_offset = offset);
//   }

//   virtual pos_type seekpos(pos_type offset, std::ios_base::openmode) {
//     if (offset > _size) {
//       return pos_type(off_type(-1));
//     }

//     return pos_type(_offset = offset);
//   }

//   virtual std::streamsize xsgetn(char_type* buffer, std::streamsize size) {
//     size = std::min(_size - _offset, size);
//     sqlite3_error::_assert(sqlite3_blob_read(&*_native_blob, (void*)&buffer, size, _offset));
//     return size;
//   }

//   virtual std::streamsize xsputn(const char_type* buffer, std::streamsize size) {
//     sqlite3_error::_assert(sqlite3_blob_write(&*_native_blob, (const void*)&buffer, size, _offset));
//     return size;
//   }

// 	virtual int_type underflow() {
//     int_type buffer;
//     sqlite3_error::_assert(sqlite3_blob_read(&*_native_blob, (void*)&buffer, 1, _offset));
//     return buffer;
// 	}

// 	virtual int_type overflow(int_type c) {
//     sqlite3_error::_assert(sqlite3_blob_write(&*_native_blob, (const void*)&c, 1, _offset));
//     return c;
// 	}

// private:
//   std::shared_ptr<sqlite3_blob> _native_blob;

//   std::streamsize _size = 0;
//   off_type _offset = 0;
// };

// std::iostream openBlob(db::statement& _stmt, std::string_view _name) {
//   // sqlite3_last_insert_rowid()
//   // sqlite3_column_table_name()
// }
} // namespace db::sqlite
