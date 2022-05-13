#include "db/sqlite-createftssynctriggers.h"

#ifdef _WIN32
#define _EXPORT __declspec(dllexport)
#else
#define _EXPORT __attribute__((__visibility__("default")))
#endif

namespace {
SQLITE_EXTENSION_INIT1

std::string sqlite3_value_cppstring(sqlite3_value* value) {
  auto text_ptr = (const std::string::value_type*)sqlite3_value_text(value);
  auto text_size = (std::string::size_type)sqlite3_value_bytes(value);

  return {text_ptr, text_size};
}

std::size_t string_replace_all(std::string& inout, const std::string& what, const std::string& with) {
  std::size_t count{};
  for (std::string::size_type pos{};
      inout.npos != (pos = inout.find(what.data(), pos, what.length()));
      pos += with.length(), ++count) {
    inout.replace(pos, what.length(), with.data(), with.length());
  }
  return count;
}
}

namespace db::sqlite {
_EXPORT
std::string createCreateFTSSyncTriggersScript(const std::string& src_table, const std::string& fts_table, const std::vector<std::string>& fields) {
  std::string fields_str_raw = "rowid";
  std::string fields_str_src = "SRC.rowid";
  std::string fields_str_new = "NEW.rowid";
  std::string fields_str_old = "OLD.rowid";

  auto values_clause = [](const std::string& field, const std::string& table) -> std::string {
    if (field.substr(0, 6) == "SELECT") {
      auto field_modified = field;
      string_replace_all(field_modified, "$SRC", table);

      return '(' + field_modified + ')';
    } else {
      return table + '.' + '"' + field + '"';
    }
  };

  for (const auto& field : fields) {
    fields_str_raw += ',';
    fields_str_raw += '"' + field + '"';

    fields_str_src += ',';
    fields_str_src += values_clause(field, "SRC");

    fields_str_new += ',';
    fields_str_new += values_clause(field, "NEW");

    fields_str_old += ',';
    fields_str_old += values_clause(field, "OLD");
  }

  std::string script = R"~(
INSERT INTO "${FTS_TABLE}" (
  "${FTS_TABLE}"
)
VALUES (
  'delete-all'
);

INSERT INTO "${FTS_TABLE}" (
  ${FIELDS_STR_RAW}
)
SELECT
  ${FIELDS_STR_SRC}
FROM "${SRC_TABLE}" SRC;

CREATE TRIGGER "trg_${SRC_TABLE}_after_insert_sync_FTS"
AFTER INSERT ON "${SRC_TABLE}"
BEGIN
  INSERT INTO "${FTS_TABLE}" (
    ${FIELDS_STR_RAW}
  ) VALUES (
    ${FIELDS_STR_NEW}
  );
END;

CREATE TRIGGER "trg_${SRC_TABLE}_after_update_sync_FTS"
AFTER UPDATE ON "${SRC_TABLE}"
BEGIN
  INSERT INTO "${FTS_TABLE}" (
    "${FTS_TABLE}",
    ${FIELDS_STR_RAW}
  ) VALUES (
    'delete',
    ${FIELDS_STR_OLD}
  );

  INSERT INTO "${FTS_TABLE}" (
    ${FIELDS_STR_RAW}
  ) VALUES (
    ${FIELDS_STR_NEW}
  );
END;

CREATE TRIGGER "trg_${SRC_TABLE}_after_delete_sync_FTS"
AFTER DELETE ON "${SRC_TABLE}"
BEGIN
  INSERT INTO "${FTS_TABLE}" (
    "${FTS_TABLE}",
    ${FIELDS_STR_RAW}
  ) VALUES (
    'delete',
    ${FIELDS_STR_OLD}
  );
END;
    )~";

  string_replace_all(script, "${SRC_TABLE}", src_table);
  string_replace_all(script, "${FTS_TABLE}", fts_table);
  string_replace_all(script, "${FIELDS_STR_RAW}", fields_str_raw);
  string_replace_all(script, "${FIELDS_STR_SRC}", fields_str_src);
  string_replace_all(script, "${FIELDS_STR_NEW}", fields_str_new);
  string_replace_all(script, "${FIELDS_STR_OLD}", fields_str_old);

  return script;
}

_EXPORT
std::string createDropFTSSyncTriggersScript(const std::string& src_table) {
  std::string script = R"~(
DROP TRIGGER "trg_${SRC_TABLE}_after_insert_sync_FTS";
DROP TRIGGER "trg_${SRC_TABLE}_after_update_sync_FTS";
DROP TRIGGER "trg_${SRC_TABLE}_after_delete_sync_FTS";
  )~";

  string_replace_all(script, "${SRC_TABLE}", src_table);

  return script;
}
}

void sqlite3_create_fts_sync_triggers(sqlite3_context* context, int argc, sqlite3_value** argv) {
  int rc = SQLITE_OK;
  char* errmsg = nullptr;

  sqlite3* conn = sqlite3_context_db_handle(context);

  std::string src_table = sqlite3_value_cppstring(argv[0]);
  std::string fts_table = sqlite3_value_cppstring(argv[1]);

  std::string fields_script = "PRAGMA table_info('${FTS_TABLE}')";
  string_replace_all(fields_script, "${FTS_TABLE}", fts_table);

  sqlite3_stmt* stmt;
  rc = sqlite3_prepare_v2(conn, fields_script.data(), fields_script.length(), &stmt, nullptr);
  if (rc != SQLITE_OK) {
    sqlite3_result_error(context, sqlite3_errmsg(conn), -1);
    return;
  }

  std::vector<std::string> fields;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    int col_count = sqlite3_data_count(stmt);

    for (int i = 0; i < col_count; i++) {
      if (std::string{sqlite3_column_name(stmt, i)} == "name") {
        sqlite3_value* col_value = sqlite3_column_value(stmt, i);
        std::string col_string = sqlite3_value_cppstring(col_value);

        fields.push_back(std::move(col_string));
        break;
      }
    }
  }

  rc = sqlite3_finalize(stmt);
  if (rc != SQLITE_OK) {
    sqlite3_result_error(context, sqlite3_errmsg(conn), -1);
    return;
  }

  std::string script = db::sqlite::createCreateFTSSyncTriggersScript(src_table, fts_table, fields);

  rc = sqlite3_exec(conn, script.data(), nullptr, nullptr, &errmsg);
  if (rc != SQLITE_OK) {
    sqlite3_result_error(context, errmsg, -1);
    return;
  }
}

void sqlite3_drop_fts_sync_triggers(sqlite3_context* context, int argc, sqlite3_value** argv) {
  int rc = SQLITE_OK;
  char* errmsg = nullptr;

  sqlite3* conn = sqlite3_context_db_handle(context);

  std::string src_table = sqlite3_value_cppstring(argv[0]);

  std::string script = db::sqlite::createDropFTSSyncTriggersScript(src_table);

  rc = sqlite3_exec(conn, script.data(), nullptr, nullptr, &errmsg);
  if (rc != SQLITE_OK) {
    sqlite3_result_error(context, errmsg, -1);
    return;
  }
}

_EXPORT
int sqlite3_createftssynctriggers_init(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi) {
  SQLITE_EXTENSION_INIT2(pApi);

  int rc = SQLITE_OK;
  (void)pzErrMsg;

  rc = sqlite3_create_function(db, "create_fts_sync_triggers", 2, SQLITE_UTF8, nullptr, sqlite3_create_fts_sync_triggers, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    return rc;
  }

  rc = sqlite3_create_function(db, "drop_fts_sync_triggers", 1, SQLITE_UTF8, nullptr, sqlite3_drop_fts_sync_triggers, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    return rc;
  }

  return rc;
}

_EXPORT
int sqlite3_createftssynctriggers_deinit(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi) {
  SQLITE_EXTENSION_INIT2(pApi);

  int rc = SQLITE_OK;
  (void)pzErrMsg;

  rc = sqlite3_create_function(db, "create_fts_sync_triggers", 2, SQLITE_UTF8, nullptr, nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    return rc;
  }

  rc = sqlite3_create_function(db, "drop_fts_sync_triggers", 1, SQLITE_UTF8, nullptr, nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    return rc;
  }

  return rc;
}
