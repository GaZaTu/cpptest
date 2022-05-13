cmake_minimum_required(VERSION 3.12.0 FATAL_ERROR)
project(cpptest VERSION 0.0.1 LANGUAGES CXX)

set(PROJECT_ORGANIZATION gazatu.xyz)

cmake_policy(SET CMP0074 NEW)

add_definitions(
  -DPROJECT_ORGANIZATION="${PROJECT_ORGANIZATION}"
  -DPROJECT_NAME="${PROJECT_NAME}"
  -DPROJECT_VERSION="${PROJECT_VERSION}"
  -DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  -DSQLITE_CORE=true
  -DUVPP_TASK_INCLUDE="task.hpp"
  -DUVPP_TASK_TYPE=taskpp::task
  -DUVPP_TASK_CREATE=taskpp::create
  -DUVPP_SSL_INCLUDE="ssl/ssl.hpp"
  -DHTTPPP_TASK_INCLUDE="task.hpp"
  -DHTTPPP_TASK_TYPE=taskpp::task
  -DHTTPPP_TASK_CREATE=taskpp::create
  -DHTTPPP_SSL_DRIVER_INCLUDE="ssl/ssl-openssl.hpp"
  -DHTTPPP_SSL_DRIVER_TYPE=ssl::openssl::driver
  -DHTTPPP_HTTP2=true
  # -DHTTPPP_ZLIB=true
  -DSSLPP_TASK_INCLUDE="task.hpp"
  -DSSLPP_TASK_TYPE=taskpp::task
  -DSSLPP_TASK_CREATE=taskpp::create
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_library(UV_LIB uv REQUIRED)
add_library(UV STATIC SHARED IMPORTED)
set_target_properties(UV PROPERTIES
  IMPORTED_LOCATION ${UV_LIB}
)

find_library(OPENSSL_LIB ssl REQUIRED)
add_library(OPENSSL STATIC SHARED IMPORTED)
set_target_properties(OPENSSL PROPERTIES
  IMPORTED_LOCATION ${OPENSSL_LIB}
)

find_library(CRYPTO_LIB crypto REQUIRED)
add_library(CRYPTO STATIC SHARED IMPORTED)
set_target_properties(CRYPTO PROPERTIES
  IMPORTED_LOCATION ${CRYPTO_LIB}
)

find_library(ZLIB_LIB z REQUIRED)
add_library(ZLIB STATIC SHARED IMPORTED)
set_target_properties(ZLIB PROPERTIES
  IMPORTED_LOCATION ${ZLIB_LIB}
)

find_library(HTTPPARSER_LIB http_parser REQUIRED)
add_library(HTTPPARSER STATIC SHARED IMPORTED)
set_target_properties(HTTPPARSER PROPERTIES
  IMPORTED_LOCATION ${HTTPPARSER_LIB}
)

find_library(NGHTTP2_LIB nghttp2 REQUIRED)
add_library(NGHTTP2 STATIC SHARED IMPORTED)
set_target_properties(NGHTTP2 PROPERTIES
  IMPORTED_LOCATION ${NGHTTP2_LIB}
)

find_library(SQLITE3_LIB sqlite3 REQUIRED)
add_library(SQLITE3 STATIC SHARED IMPORTED)
set_target_properties(SQLITE3 PROPERTIES
  IMPORTED_LOCATION ${SQLITE3_LIB}
)

find_library(PQ_LIB pq REQUIRED)
add_library(PQ STATIC SHARED IMPORTED)
set_target_properties(PQ PROPERTIES
  IMPORTED_LOCATION ${PQ_LIB}
)

include_directories(
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/include
)

set(SRC_FILES
  src/main.cpp
  src/task.cpp
  src/db/connection.cpp
  src/db/datasource.cpp
  src/db/orm.cpp
  src/db/orm-common.cpp
  src/db/orm-repository.cpp
  src/db/pool.cpp
  src/db/resultset.cpp
  src/db/statement.cpp
  src/db/transaction.cpp
  src/db/pgsql.cpp
  src/db/sqlite.cpp
  src/db/sqlite-createftssynctriggers.cpp
  src/db/sqlite-createisotimestamptriggers.cpp
  src/db/sqlite-sanitizewebsearch.cpp
  src/cppcoro/async_manual_reset_event.cpp
  src/uvpp/async.cpp
  src/uvpp/check.cpp
  src/uvpp/dns.cpp
  src/uvpp/error.cpp
  src/uvpp/fs.cpp
  src/uvpp/handle.cpp
  src/uvpp/req.cpp
  src/uvpp/signal.cpp
  src/uvpp/stream.cpp
  src/uvpp/tcp.cpp
  src/uvpp/threading.cpp
  src/uvpp/timer.cpp
  src/uvpp/tty.cpp
  src/http/base64.cpp
  src/http/common.cpp
  src/http/fetch.cpp
  src/http/http1.cpp
  src/http/http2.cpp
  src/http/gzip.cpp
  src/http/http1-serve.cpp
  src/http/http2-serve.cpp
  src/http/serve.cpp
  src/ssl/ssl.cpp
  src/ssl/ssl-openssl.cpp
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

set_target_properties(${PROJECT_NAME} PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

include(cmake/CMakeRC.cmake)
cmrc_add_resource_library(resources ALIAS res NAMESPACE res
  src/res/sql/upgrades/v00001.sql
  src/res/sql/upgrades/v00002.sql
)

target_link_libraries(${PROJECT_NAME}
  res
  UV
  OPENSSL
  CRYPTO
  ZLIB
  HTTPPARSER
  NGHTTP2
  SQLITE3
  PQ
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -fcoroutines")

add_library(sanitizewebsearch SHARED src/db/sqlite-sanitizewebsearch.cpp)

set_target_properties(sanitizewebsearch PROPERTIES
  CXX_STANDARD 14
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

target_link_libraries(sanitizewebsearch
  SQLITE3
)
