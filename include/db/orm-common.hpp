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
using datetime = detail::timestamp<detail::format_iso, sizeof(detail::output_iso)>;

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

std::ostream& operator<<(std::ostream& os, order_by_direction op) {
  switch (op) {
  case ASCENDING:
    os << "ASC";
    break;
  case DESCENDING:
    os << "DESC";
    break;
  }

  return os;
}

enum order_by_nulls {
  NULLS_DEFAULT,
  NULLS_FIRST,
  NULLS_LAST,
};

std::ostream& operator<<(std::ostream& os, order_by_nulls op) {
  switch (op) {
  case NULLS_FIRST:
    os << "NULLS FIRST";
    break;
  case NULLS_LAST:
    os << "NULLS LAST";
    break;
  }

  return os;
}

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

std::ostream& operator<<(std::ostream& os, condition_operator op) {
  switch (op) {
  case condition_operator::ASSIGNMENT:
    os << " = ";
    break;
  case condition_operator::AND:
    os << " AND ";
    break;
  case condition_operator::OR:
    os << " OR ";
    break;
  case condition_operator::EQUALS:
    os << " = ";
    break;
  case condition_operator::NOT_EQUALS:
    os << " != ";
    break;
  case condition_operator::LOWER_THAN:
    os << " < ";
    break;
  case condition_operator::LOWER_THAN_EQUALS:
    os << " <= ";
    break;
  case condition_operator::GREATER_THAN:
    os << " > ";
    break;
  case condition_operator::GREATER_THAN_EQUALS:
    os << " >= ";
    break;
  case condition_operator::IS_NULL:
    os << " IS NULL";
    break;
  case condition_operator::IS_NOT_NULL:
    os << " IS NOT NULL";
    break;
  case condition_operator::NOT:
    os << " NOT ";
    break;
  case condition_operator::IN:
    os << " IN ";
    break;
  }

  return os;
}

struct condition_container {
  virtual ~condition_container() {
  }

  virtual condition_operator getOperator() const = 0;

  virtual void appendLeftToQuery(std::ostream& os, int& i) const = 0;

  virtual void appendRightToQuery(std::ostream& os, int& i) const = 0;

  virtual void appendToQuery(std::ostream& os, int& i) const = 0;

  void appendToQuery(std::ostream& os) const {
    int i = 666;
    appendToQuery(os, i);
  }

  virtual void assignToParams(db::statement& statement, int& i) const = 0;

  void assignToParams(db::statement& statement) const {
    int i = 666;
    assignToParams(statement, i);
  }
};

struct selection {
  std::string name;
  std::string alias = "";

  selection() = default;

  selection(const char* name, const char* alias) {
    this->name = name;
    this->alias = alias;
  }

  selection(const char* name) {
    this->name = name;
  }

  selection(const std::string& name) {
    this->name = name;
  }

  selection(std::string_view name) {
    this->name = name;
  }

  selection(std::string&& name) {
    this->name = std::move(name);
  }

  explicit operator std::string() const {
    std::string result;
    // result += '"';
    result += name;
    // result += '"';
    if (alias.length() > 0) {
      result += " AS ";
      result += '"';
      result += alias;
      result += '"';
    }
    return result;
  }

  operator bool() const {
    return name.length() > 0;
  }
};

struct query_builder_data {
  struct join_on_clause {
    selection table;
    std::string condition;
  };

  struct order_by_clause {
    std::string field;
    order_by_direction direction = DIRECTION_DEFAULT;
    order_by_nulls nulls = NULLS_DEFAULT;
  };

  enum upsert_mode {
    DISABLED,
    NOTHING,
    UPDATE,
    REPLACE,
  };

  selection table;
  std::vector<selection> fields;
  std::vector<join_on_clause> joins;
  std::vector<std::shared_ptr<condition_container>> assignments;
  std::vector<std::shared_ptr<condition_container>> conditions;
  std::vector<order_by_clause> ordering;
  int limit = 0;
  upsert_mode upsert = DISABLED;
  // db::orm::query_builder_data upsert_data;
};
} // namespace db::orm
