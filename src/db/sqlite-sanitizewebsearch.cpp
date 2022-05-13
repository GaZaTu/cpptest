#include "db/sqlite-sanitizewebsearch.h"

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

void sqlite3_result_cppstring(sqlite3_context* context, const std::string& value) {
  sqlite3_result_text(context, value.data(), value.length(), SQLITE_TRANSIENT);
}
}

namespace db::sqlite {
_EXPORT
std::string sanitizeWebSearch(const std::string& search) {
  std::string result = "";

  bool quoted = false;
  bool quoted_auto = false;
  int quoted_start_idx = 0;
  int quoted_end_idx = 0;
  bool negate_next = false;

  for (int i = 0; i <= search.length(); i++) {
    auto quote = [&]() {
      if (!quoted_auto) {
        if (quoted && (i - quoted_start_idx) == 1) {
          quoted_start_idx += 1;
          return;
        }

        if (!quoted && (i - quoted_end_idx) == 1 && i > 0) {
          quoted_end_idx += 1;
          return;
        }
      }

      quoted = !quoted;

      if (quoted) {
        quoted_start_idx = i;
      } else {
        quoted_end_idx = i;
      }

      if (quoted && !result.empty()) {
        if (negate_next) {
          negate_next = false;
          result += " NOT ";
        } else {
          result += " AND ";
        }
      }

      result += '"';

      if (!quoted) {
        result += '*';
        quoted_auto = false;
      }
    };

    if (i == search.length()) {
      if (quoted) {
        quoted_start_idx = -1;
        quote();
      }
      continue;
    }

    auto chr = search[i];

    if (chr == '"') {
      quote();
      continue;
    }

    if (quoted) {
      if (std::isspace(chr) && quoted_auto) {
        quote();
        continue;
      }
    } else {
      if (std::isspace(chr) || chr == '*' || chr == '+' || chr == '-') {
        negate_next = (chr == '-') && !negate_next;
        continue;
      }

      quoted_auto = true;
      quote();
    }

    result += chr;
  }

  if (result.empty()) {
    result = "*";
  }

  return result;
}
}

void sqlite3_sanitize_web_search(sqlite3_context* context, int argc, sqlite3_value** argv) {
  std::string search = sqlite3_value_cppstring(argv[0]);
  std::string result = db::sqlite::sanitizeWebSearch(search);

  sqlite3_result_cppstring(context, result);
}

_EXPORT
int sqlite3_sanitizewebsearch_init(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi) {
  SQLITE_EXTENSION_INIT2(pApi);

  int rc = SQLITE_OK;
  (void)pzErrMsg;

  rc = sqlite3_create_function(db, "sanitize_web_search", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr, sqlite3_sanitize_web_search, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    return rc;
  }

  return rc;
}

_EXPORT
int sqlite3_sanitizewebsearch_deinit(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi) {
  SQLITE_EXTENSION_INIT2(pApi);

  int rc = SQLITE_OK;
  (void)pzErrMsg;

  rc = sqlite3_create_function(db, "sanitize_web_search", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr, nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    return rc;
  }

  return rc;
}
