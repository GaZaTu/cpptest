#include "db/sqlite-createisotimestamptriggers.h"

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
std::string createCreateISOTimestampTriggersScript(const std::string& table, const std::string& column) {
  std::string script = R"~(
CREATE TRIGGER "trg_${TABLE}_after_insert_of_${COLUMN}_fix_iso_timestamp" AFTER INSERT ON "${TABLE}"
BEGIN
UPDATE "${TABLE}"
SET "${COLUMN}" = strftime('%Y-%m-%dT%H:%M:%fZ', NEW."${COLUMN}")
WHERE rowid = NEW.rowid;
END;

CREATE TRIGGER "trg_${TABLE}_after_update_of_${COLUMN}_fix_iso_timestamp" AFTER UPDATE OF "${COLUMN}" ON "${TABLE}"
WHEN NEW."${COLUMN}" IS NOT OLD."${COLUMN}"
BEGIN
UPDATE "${TABLE}"
SET "${COLUMN}" = strftime('%Y-%m-%dT%H:%M:%fZ', NEW."${COLUMN}")
WHERE rowid = NEW.rowid;
END;
  )~";

  string_replace_all(script, "${TABLE}", table);
  string_replace_all(script, "${COLUMN}", column);

  return script;
}

_EXPORT
std::string createDropISOTimestampTriggersScript(const std::string& table, const std::string& column) {
  std::string script = R"~(
DROP TRIGGER "trg_${TABLE}_after_insert_of_${COLUMN}_fix_iso_timestamp";
DROP TRIGGER "trg_${TABLE}_after_update_of_${COLUMN}_fix_iso_timestamp";
  )~";

  string_replace_all(script, "${TABLE}", table);
  string_replace_all(script, "${COLUMN}", column);

  return script;
}
}

void sqlite3_create_iso_timestamp_triggers(sqlite3_context* context, int argc, sqlite3_value** argv) {
  int rc = SQLITE_OK;
  char* errmsg = nullptr;

  sqlite3* conn = sqlite3_context_db_handle(context);

  std::string table = sqlite3_value_cppstring(argv[0]);
  std::string column = sqlite3_value_cppstring(argv[1]);
  std::string script = db::sqlite::createCreateISOTimestampTriggersScript(table, column);

  rc = sqlite3_exec(conn, script.data(), nullptr, nullptr, &errmsg);
  if (rc != SQLITE_OK) {
    sqlite3_result_error(context, errmsg, -1);
    return;
  }
}

void sqlite3_drop_iso_timestamp_triggers(sqlite3_context* context, int argc, sqlite3_value** argv) {
  int rc = SQLITE_OK;
  char* errmsg = nullptr;

  sqlite3* conn = sqlite3_context_db_handle(context);

  std::string table = sqlite3_value_cppstring(argv[0]);
  std::string column = sqlite3_value_cppstring(argv[1]);
  std::string script = db::sqlite::createDropISOTimestampTriggersScript(table, column);

  rc = sqlite3_exec(conn, script.data(), nullptr, nullptr, &errmsg);
  if (rc != SQLITE_OK) {
    sqlite3_result_error(context, errmsg, -1);
    return;
  }
}

_EXPORT
int sqlite3_createisotimestamptriggers_init(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi) {
  SQLITE_EXTENSION_INIT2(pApi);

  int rc = SQLITE_OK;
  (void)pzErrMsg;

  rc = sqlite3_create_function(db, "create_iso_timestamp_triggers", 2, SQLITE_UTF8, nullptr, sqlite3_create_iso_timestamp_triggers, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    return rc;
  }

  rc = sqlite3_create_function(db, "drop_iso_timestamp_triggers", 2, SQLITE_UTF8, nullptr, sqlite3_drop_iso_timestamp_triggers, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    return rc;
  }

  return rc;
}

_EXPORT
int sqlite3_createisotimestamptriggers_deinit(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi) {
  SQLITE_EXTENSION_INIT2(pApi);

  int rc = SQLITE_OK;
  (void)pzErrMsg;

  rc = sqlite3_create_function(db, "create_iso_timestamp_triggers", 2, SQLITE_UTF8, nullptr, nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    return rc;
  }

  rc = sqlite3_create_function(db, "drop_iso_timestamp_triggers", 2, SQLITE_UTF8, nullptr, nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    return rc;
  }

  return rc;
}
