#pragma once

#include "sqlite3ext.h"
#ifdef __cplusplus
#include <string>
#endif

#ifdef __cplusplus
extern "C" {
#endif
int sqlite3_sanitizewebsearch_init(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi);
int sqlite3_sanitizewebsearch_deinit(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace db::sqlite {
std::string sanitizeWebSearch(const std::string& search);
}
#endif
