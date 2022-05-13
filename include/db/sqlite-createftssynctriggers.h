#pragma once

#include "sqlite3ext.h"
#ifdef __cplusplus
#include <string>
#include <vector>
#endif

#ifdef __cplusplus
extern "C" {
#endif
int sqlite3_createftssynctriggers_init(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi);
int sqlite3_createftssynctriggers_deinit(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace db::sqlite {
std::string createCreateFTSSyncTriggersScript(const std::string& src_table, const std::string& fts_table, const std::vector<std::string>& fields);
std::string createDropFTSSyncTriggersScript(const std::string& src_table);
}
#endif
