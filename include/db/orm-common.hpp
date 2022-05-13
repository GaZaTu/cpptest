#pragma once

#include "./common.hpp"
#include <functional>
#include <iomanip>
#include <memory>
#include <optional>
#include <stdint.h>
#include <string>
#include <vector>

namespace db::orm {
enum field_type {
  UNKNOWN,
  BOOLEAN,
  INT32,
  INT64,
#ifdef __SIZEOF_INT128__
  UINT128,
#endif
  DOUBLE,
  STRING,
  BLOB,
  DATE,
  TIME,
  DATETIME,
};

struct field_info {
  const char* name;
  field_type type = UNKNOWN;
  bool optional = false;
};

namespace detail {
constexpr char format_date[] = "%Y-%m-%d";
constexpr char output_date[] = "YYYY:mm:dd";

constexpr char format_time[] = "%H:%M";
constexpr char output_time[] = "HH:MM";

constexpr char format_iso[] = "%Y-%m-%dT%H:%M:%SZ";
constexpr char output_iso[] = "YYYY-mm-ddTHH:MM:SSZ";

template <const char* FORMAT, size_t LENGTH>
struct timestamp {
  std::time_t value = 0;

  static timestamp now() {
    std::time_t now;
    ::time(&now);
    return now;
  }

  timestamp() {
  }

  timestamp(std::time_t t) {
    *this = t;
  }

  timestamp(std::string_view s) {
    *this = s;
  }

  timestamp& operator=(std::time_t t) {
    value = t;
    return *this;
  }

  timestamp& operator=(std::string_view s) {
    std::istringstream ss(s.data());
    std::tm tm = {0};
    ss >> std::get_time(&tm, FORMAT);
    value = mktime(&tm);
    return *this;
  }

  operator std::time_t() {
    return value;
  }

  explicit operator std::string() {
    std::string result;
    result.resize(LENGTH);
    strftime(result.data(), LENGTH, FORMAT, gmtime(&value));
    return result;
  }

  operator bool() {
    return value != 0;
  }
};
} // namespace detail

using date = detail::timestamp<detail::format_date, sizeof(detail::output_date)>;
using time = detail::timestamp<detail::format_time, sizeof(detail::output_time)>;
using timestamp = detail::timestamp<detail::format_iso, sizeof(detail::output_iso)>;

template <typename T>
struct primary {
  static constexpr bool specialized = false;

  using tuple_type = void;
};

template <typename T>
struct meta {
  static constexpr bool specialized = false;
};

template <typename T>
struct idmeta {
  static constexpr bool specialized = false;
};

struct update {
  int version;

  std::function<void(db::connection&)> up;
  std::function<void(db::connection&)> down;
};

enum order_by_direction {
  DIRECTION_DEFAULT,
  ASCENDING,
  DESCENDING,
};

std::ostream& operator<<(std::ostream& os, order_by_direction op);

enum order_by_nulls {
  NULLS_DEFAULT,
  NULLS_FIRST,
  NULLS_LAST,
};

std::ostream& operator<<(std::ostream& os, order_by_nulls op);

enum join_mode {
  JOIN_INNER,
  JOIN_LEFT,
  JOIN_CROSS,
};

std::ostream& operator<<(std::ostream& os, join_mode op);

enum class condition_operator {
  CUSTOM              = 1 <<  0,
  ASSIGNMENT          = 1 <<  1,
  COMMA               = 1 <<  2,
  AND                 = 1 <<  3,
  OR                  = 1 <<  4,
  EQUALS              = 1 <<  5,
  NOT_EQUALS          = 1 <<  6,
  LOWER_THAN          = 1 <<  7,
  LOWER_THAN_EQUALS   = 1 <<  8,
  GREATER_THAN        = 1 <<  9,
  GREATER_THAN_EQUALS = 1 << 10,
  IS_NULL             = 1 << 11,
  IS_NOT_NULL         = 1 << 12,
  NOT                 = 1 << 13,
  IN                  = 1 << 14,
};

inline constexpr condition_operator operator|(condition_operator a, condition_operator b) {
  return (condition_operator)((int)a | (int)b);
}
inline constexpr condition_operator operator&(condition_operator a, condition_operator b) {
  return (condition_operator)((int)a & (int)b);
}
inline constexpr condition_operator operator^(condition_operator a, condition_operator b) {
  return (condition_operator)((int)a ^ (int)b);
}
inline constexpr bool operator==(condition_operator a, int b) {
  return (int)a == (int)b;
}

std::ostream& operator<<(std::ostream& os, condition_operator op);

struct condition_container {
  virtual ~condition_container();

  virtual condition_operator getOperator() const = 0;

  virtual void appendLeftToQuery(std::ostream& os, int& i) const = 0;

  virtual void appendRightToQuery(std::ostream& os, int& i) const = 0;

  virtual void appendToQuery(std::ostream& os, int& i) const = 0;

  void appendToQuery(std::ostream& os) const;

  virtual void assignToParams(db::statement& statement, int& i) const = 0;

  void assignToParams(db::statement& statement) const;
};

struct selection {
  std::string name;
  std::string alias = "";

  selection() = default;

  selection(const char* name, const char* alias);

  selection(const char* name);

  selection(const std::string& name);

  selection(std::string_view name);

  selection(std::string&& name);

  explicit operator std::string() const;

  operator bool() const;
};

struct query_builder_data {
  struct join_on_clause {
    selection table;
    std::shared_ptr<condition_container> condition = nullptr;
    join_mode mode = JOIN_INNER;
  };

  struct order_by_clause {
    std::string field;
    order_by_direction direction = DIRECTION_DEFAULT;
    order_by_nulls nulls = NULLS_DEFAULT;
  };

  enum upsert_mode {
    UP_DISABLED,
    UP_NOTHING,
    UP_UPDATE,
    UP_REPLACE,
  };

  selection table;
  std::vector<selection> fields;
  std::vector<join_on_clause> joins;
  std::vector<std::shared_ptr<condition_container>> assignments;
  std::vector<std::shared_ptr<condition_container>> conditions;
  std::vector<std::string> grouping;
  std::vector<std::shared_ptr<condition_container>> having_conditions;
  std::vector<order_by_clause> ordering;
  uint32_t offset = 0;
  uint32_t limit = 0;
  upsert_mode upsert = UP_DISABLED;
  bool distinct = false;
  // db::orm::query_builder_data upsert_data;
};
} // namespace db::orm
