#pragma once

#include "./resultset.hpp"
#include <ostream>
#include <string>

namespace db {
namespace orm {
template <typename T>
struct field_type_converter {
  static constexpr field_type type = UNKNOWN;
  static constexpr bool optional = false;

  using decayed = void;
};

template <>
struct field_type_converter<bool> {
  static constexpr field_type type = BOOLEAN;
  static constexpr bool optional = false;
};

template <>
struct field_type_converter<int> {
  static constexpr field_type type = INT32;
  static constexpr bool optional = false;
};

template <>
struct field_type_converter<int64_t> {
  static constexpr field_type type = INT64;
  static constexpr bool optional = false;
};

#ifdef __SIZEOF_INT128__
template <>
struct field_type_converter<__uint128_t> {
  static constexpr field_type type = UINT128;
  static constexpr bool optional = false;
};
#endif

template <>
struct field_type_converter<double> {
  static constexpr field_type type = DOUBLE;
  static constexpr bool optional = false;
};

template <>
struct field_type_converter<std::string> {
  static constexpr field_type type = STRING;
  static constexpr bool optional = false;
};

template <>
struct field_type_converter<std::vector<uint8_t>> {
  static constexpr field_type type = BLOB;
  static constexpr bool optional = false;
};

// template <>
// struct field_type_converter<std::time_t> {
//   static constexpr field_type type = DATETIME;
//   static constexpr bool optional = false;
// };

// template <>
// struct field_type_converter<std::chrono::system_clock::time_point> {
//   static constexpr field_type type = DATETIME;
//   static constexpr bool optional = false;
// };

template <>
struct field_type_converter<db::orm::date> {
  static constexpr field_type type = DATE;
  static constexpr bool optional = false;
};

template <>
struct field_type_converter<db::orm::time> {
  static constexpr field_type type = TIME;
  static constexpr bool optional = false;
};

template <>
struct field_type_converter<db::orm::datetime> {
  static constexpr field_type type = DATETIME;
  static constexpr bool optional = false;
};

template <typename T>
struct field_type_converter<std::optional<T>> : public field_type_converter<T> {
  static constexpr bool optional = true;

  using decayed = T;
};

template <typename T>
struct is_condition_or_field {
  static constexpr bool value = false;
};

template <typename T>
struct is_std_vector {
  static constexpr bool value = false;
};

template <typename T>
struct is_std_vector<std::vector<T>> {
  static constexpr bool value = true;
};

template <typename L, condition_operator O, typename R>
struct condition : public condition_container {
  L left;
  R right;

  condition(L&& l, R&& r) : condition_container(), left(std::move(l)), right(std::move(r)) {
  }

  condition(const L& l, R& r) : condition_container(), left(l), right(std::move(r)) {
  }

  condition(L&& l, R& r) : condition_container(), left(std::move(l)), right(std::move(r)) {
  }

  condition(const L& l, const R& r) : condition_container(), left(l), right(r) {
  }

  condition(L&& l, const R& r) : condition_container(), left(std::move(l)), right(r) {
  }

  template <typename _L, condition_operator _O, typename _R>
  constexpr condition<condition, condition_operator::AND, condition<_L, _O, _R>> operator&&(
      condition<_L, _O, _R> right) {
    return {std::move(*this), std::move(right)};
  }

  template <typename _L, condition_operator _O, typename _R>
  constexpr condition<condition, condition_operator::OR, condition<_L, _O, _R>> operator||(
      condition<_L, _O, _R> right) {
    return {std::move(*this), std::move(right)};
  }

  constexpr condition<std::nullopt_t, condition_operator::NOT, condition> operator!() {
    return {std::nullopt, std::move(*this)};
  }

  condition_operator getOperator() const override {
    return O;
  }

  void appendLeftToQuery(std::ostream& os, int& i) const override {
    if constexpr ((O | condition_operator::CUSTOM) == condition_operator::CUSTOM) {
      os << left;
    } else if constexpr (O == condition_operator::NOT) {
    } else {
      left.appendToQuery(os, i);
    }
  }

  void appendRightToQuery(std::ostream& os, int& i) const override {
    if constexpr (is_condition_or_field<R>::value) {
      right.appendToQuery(os, i);
    } else {
      if constexpr (std::is_same<R, std::nullopt_t>::value) {
      } else if constexpr ((O | condition_operator::CUSTOM) == condition_operator::CUSTOM) {
      } else if constexpr (is_std_vector<R>::value) {
        auto start = i;
        os << "(";
        for (const auto& item : right) {
          if (i > start) {
            os << ", ";
          }
          os << ":" << i++;
        }
        os << ")";
      } else {
        os << ":" << i++;
      }
    }
  }

  void appendToQuery(std::ostream& os, int& i) const override {
    if constexpr ((O | condition_operator::ASSIGNMENT) != condition_operator::ASSIGNMENT) {
      os << "(";
    }

    appendLeftToQuery(os, i);
    os << O;
    appendRightToQuery(os, i);

    if constexpr ((O | condition_operator::ASSIGNMENT) != condition_operator::ASSIGNMENT) {
      os << ")";
    }
  }

  void assignToParams(db::statement& statement, int& i) const override {
    if constexpr (is_condition_or_field<L>::value) {
      if constexpr (is_condition_or_field<L>::is_condition) {
        left.assignToParams(statement, i);
      }
    }

    if constexpr (is_condition_or_field<R>::value) {
      if constexpr (is_condition_or_field<R>::is_condition) {
        right.assignToParams(statement, i);
      }
    } else {
      if constexpr (std::is_same<R, std::nullopt_t>::value) {
      } else if constexpr ((O | condition_operator::CUSTOM) == condition_operator::CUSTOM) {
        statement.params[right.name] = right.value;
      } else if constexpr (is_std_vector<R>::value) {
        for (const auto& item : right) {
          statement.params[std::string{":"} + std::to_string(i++)] = item;
        }
      } else {
        statement.params[std::string{":"} + std::to_string(i++)] = right;
      }
    }
  }
};

template <typename T>
struct field {
  const char* name;

  constexpr condition<field, condition_operator::ASSIGNMENT, T> operator=(const T& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::EQUALS, T> operator==(const T& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::IS_NULL, std::nullopt_t> operator==(std::nullopt_t right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::NOT_EQUALS, T> operator!=(const T& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::IS_NOT_NULL, std::nullopt_t> operator!=(std::nullopt_t right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::LOWER_THAN, T> operator<(const T& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::LOWER_THAN_EQUALS, T> operator<=(const T& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::GREATER_THAN, T> operator>(const T& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::GREATER_THAN_EQUALS, T> operator>=(const T& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::IN, std::vector<T>> IN(const std::vector<T>& right) const {
    return {*this, right};
  }

  constexpr operator condition<field, condition_operator::EQUALS, bool>() const {
    return *this == true;
  }

  constexpr condition<field, condition_operator::EQUALS, bool> operator!() const {
    return *this == false;
  }

  void appendToQuery(std::ostream& os, int&) const {
    os << name;
  }

  operator const char*() {
    return name;
  }

  operator std::string() {
    return name;
  }

  operator std::string_view() {
    return name;
  }
};

template <typename L, condition_operator O, typename R>
struct is_condition_or_field<condition<L, O, R>> {
  static constexpr bool value = true;
  static constexpr bool is_condition = true;
  static constexpr bool is_field = false;
};

template <typename T>
struct is_condition_or_field<field<T>> {
  static constexpr bool value = true;
  static constexpr bool is_condition = false;
  static constexpr bool is_field = true;
};

template <typename T>
struct fields {
  static constexpr bool specialized = false;
};

template <typename T>
class selector {
public:
  using meta = db::orm::meta<T>;

  class joiner {
  public:
    friend selector;

    joiner(selector& s, query_builder_data::join_on_clause& d) : _selector(s), _data(d) {}

    selector& on(std::string_view condition) {
      _data.condition = condition;

      return _selector;
    }

  private:
    selector& _selector;
    query_builder_data::join_on_clause& _data;
  };

  selector(db::connection& connection) : _connection(connection) {
  }

  selector& select() {
    _data.fields.clear();

    if constexpr (meta::specialized) {
      for (const auto& existing : meta::class_members) {
        _data.fields.push_back(existing.name);
      }
    } else {
      _data.fields.push_back("*");
    }

    return *this;
  }

  selector& select(const std::vector<selection>& fields) {
    _data.fields.clear();

    for (const auto& field : fields) {
      assertFieldExists(field.name);

      _data.fields.push_back(field);
    }

    return *this;
  }

  selector& columns(const std::vector<selection>& fields) {
    return select(fields);
  }

  selector& from(std::string_view table, std::string_view alias = "") {
    _data.table.name = table;
    _data.table.alias = alias;

    return *this;
  }

  joiner& join(std::string_view table, std::string_view alias = "") {
    auto& join = _data.joins.emplace_back();
    join.table.name = table;
    join.table.alias = alias;

    if (!_joiner) {
      _joiner = std::make_shared<joiner>(*this, join);
    } else {
      _joiner->_data = join;
    }

    return *_joiner;
  }

  template <typename L, condition_operator O, typename R>
  selector& where(db::orm::condition<L, O, R>&& condition) {
    auto ptr = std::make_shared<db::orm::condition<L, O, R>>(std::move(condition.left), std::move(condition.right));

    _data.conditions.push_back(ptr);

    return *this;
  }

  selector& orderBy(const std::string_view field, order_by_direction direction = DIRECTION_DEFAULT,
      order_by_nulls nulls = NULLS_DEFAULT) {
    assertFieldExists(field);

    _data.ordering.emplace_back(field, direction, nulls);

    return *this;
  }

  selector& limit(int limit) {
    _data.limit = limit;

    return *this;
  }

  void prepare(db::statement& statement) {
    if constexpr (meta::specialized) {
      _data.table = meta::class_name;
    }

    std::string script = _connection._datasource_connection->createSelectScript(_data);
    statement.prepare(script);

    for (auto condition : _data.conditions) {
      condition->assignToParams(statement);
    }
  }

  class iterator {
  public:
    explicit iterator(db::resultset::iterator&& it) : _it(it) {}

    iterator(const iterator&) = default;

    iterator(iterator&&) = default;

    iterator& operator=(const iterator&) = default;

    iterator& operator=(iterator&&) = default;

    iterator& operator++() {
      _it++;
      return *this;
    }

    iterator operator++(int) {
      return iterator{++_it};
    }

    bool operator==(const iterator& other) const {
      return _it == other._it;
    }

    bool operator!=(const iterator& other) const {
      return _it != other._it;
    }

    T operator*() {
      T value;
      meta::deserialize(*_it, value);
      return value;
    }

  private:
    db::resultset::iterator _it;
  };

  class iterable {
  public:
    explicit iterable(db::statement& stmt) : _rslt(stmt) {}

    iterator begin() {
      return iterator{_rslt.begin()};
    }

    iterator end() {
      return iterator{_rslt.end()};
    }

    std::vector<T> toVector() {
      return {begin(), end()};
    }

  private:
    db::resultset _rslt;
  };

  using findall_result = std::conditional_t<meta::specialized, iterable, db::resultset>;

  findall_result findAll() {
    db::statement statement(_connection);
    prepare(statement);

    if constexpr (std::is_same_v<findall_result, db::resultset>) {
      return db::resultset{statement};
    } else {
      return iterable{statement};
    }
  }

  using findone_result = std::conditional_t<meta::specialized, std::optional<T>, db::resultset>;

  findone_result findOne() {
    _data.limit = 1;

    if constexpr (std::is_same_v<findone_result, db::resultset>) {
      return findAll();
    } else {
      for (auto row : findAll()) {
        return {std::move(row)};
      }

      return {};
    }
  }

private:
  db::connection& _connection;
  db::orm::query_builder_data _data;

  std::shared_ptr<joiner> _joiner;

  void assertFieldExists(const std::string_view field) {
    if constexpr (meta::specialized) {
      for (const auto& existing : meta::class_members) {
        if (field == existing.name) {
          return;
        }
      }

      throw db::sql_error{"invalid field"};
    }
  }
};

template <typename T>
class updater {
public:
  using meta = db::orm::meta<T>;

  // friend db::orm::inserter;

  updater(db::connection& connection) : _connection(connection) {
  }

  updater& in(std::string_view table, std::string_view alias = "") {
    _data.table.name = table;
    _data.table.alias = alias;

    return *this;
  }

  template <typename L, db::orm::condition_operator O, typename R>
  updater& set(db::orm::condition<L, O, R>&& condition) {
    auto ptr = std::make_shared<db::orm::condition<L, O, R>>(std::move(condition.left), std::move(condition.right));

    _data.assignments.push_back(ptr);

    return *this;
  }

  template <typename L, db::orm::condition_operator O, typename R>
  updater& where(db::orm::condition<L, O, R>&& condition) {
    auto ptr = std::make_shared<db::orm::condition<L, O, R>>(std::move(condition.left), std::move(condition.right));

    _data.conditions.push_back(ptr);

    return *this;
  }

  void prepare(db::statement& statement) {
    if (!_prepare) {
      if constexpr (meta::specialized) {
        _data.table = meta::class_name;
      }

      std::string script = _connection._datasource_connection->createUpdateScript(_data);
      statement.prepare(script);
    }

    int param = 666;
    if (_prepare) {
      param = _prepare(statement);
    }

    for (auto assignment : _data.assignments) {
      assignment->assignToParams(statement, param);
    }

    for (auto condition : _data.conditions) {
      condition->assignToParams(statement, param);
    }
  }

  int executeUpdate() {
    db::statement statement(_connection);
    prepare(statement);

    return statement.executeUpdate();
  }

private:
  db::connection& _connection;
  db::orm::query_builder_data _data;

  std::function<int(db::statement&)> _prepare;
};

template <typename T>
class inserter {
public:
  using meta = db::orm::meta<T>;

  class on_conflict {
  public:
    on_conflict(inserter& i) : _inserter(i), _updater(i._connection) {}

    inserter& doNothing() {
      _inserter._data.upsert = db::orm::query_builder_data::NOTHING;

      return _inserter;
    }

    inserter& doReplace() {
      _inserter._data.upsert = db::orm::query_builder_data::REPLACE;

      return _inserter;
    }

    // updater<T>& doUpdate() {
    //   _inserter._data.upsert = db::orm::query_builder_data::UPDATE;

    //   _updater._prepare = std::bind(inserter::prepare, _inserter);
    //   return _updater;
    // }

  private:
    inserter& _inserter;
    updater<T> _updater;
  };

  inserter(db::connection& connection) : _connection(connection) {
  }

  inserter& into(std::string_view table) {
    _data.table = table;

    return *this;
  }

  template <typename L, db::orm::condition_operator O, typename R>
  inserter& set(db::orm::condition<L, O, R>&& condition) {
    auto ptr = std::make_shared<db::orm::condition<L, O, R>>(std::move(condition.left), std::move(condition.right));

    _data.assignments.push_back(ptr);

    return *this;
  }

  on_conflict& onConflict(const std::vector<std::string>& fields) {
    _data.fields.clear();
    for (const auto& field : fields) {
      _data.fields.push_back({field});
    }

    return _on_conflict;
  }

  int prepare(db::statement& statement) {
    if constexpr (meta::specialized) {
      _data.table = meta::class_name;
    }

    std::string script;
    if (_data.upsert == db::orm::query_builder_data::DISABLED) {
      script = _connection._datasource_connection->createInsertScript(_data);
    } else {
      script = _connection._datasource_connection->createUpsertScript(_data);
    }

    statement.prepare(script);

    int param = 666;

    for (auto assignment : _data.assignments) {
      assignment->assignToParams(statement, param);
    }

    return param;
  }

  int executeUpdate() {
    db::statement statement(_connection);
    prepare(statement);

    return statement.executeUpdate();
  }

private:
  db::connection& _connection;
  db::orm::query_builder_data _data;

  on_conflict _on_conflict{*this};
};

template <typename T>
class deleter {
public:
  using meta = db::orm::meta<T>;

  deleter(db::connection& connection) : _connection(connection) {
  }

  deleter& from(std::string_view table, std::string_view alias = "") {
    _data.table.name = table;
    _data.table.alias = alias;

    return *this;
  }

  template <typename L, condition_operator O, typename R>
  deleter& where(db::orm::condition<L, O, R>&& condition) {
    auto ptr = std::make_shared<db::orm::condition<L, O, R>>(std::move(condition.left), std::move(condition.right));

    _data.conditions.push_back(ptr);

    return *this;
  }

  void prepare(db::statement& statement) {
    if constexpr (meta::specialized) {
      _data.table = meta::class_name;
    }

    std::string script = _connection._datasource_connection->createDeleteScript(_data);
    statement.prepare(script);

    for (auto condition : _data.conditions) {
      condition->assignToParams(statement);
    }
  }

  int executeUpdate() {
    db::statement statement(_connection);
    prepare(statement);

    return statement.executeUpdate();
  }

private:
  db::connection& _connection;
  db::orm::query_builder_data _data;
};

template <typename T>
struct param {
  const char* name;
  T value;
};

struct query {
  std::string query;

  template <typename T = std::nullopt_t>
  constexpr operator condition<std::string, condition_operator::CUSTOM, std::nullopt_t>() const {
    return {std::move(query), std::nullopt};
  }

  template <typename T>
  constexpr condition<std::string, condition_operator::CUSTOM, param<T>> operator<<(param<T>&& p) const {
    return {std::move(query), std::move(p)};
  }

  template <typename T>
  constexpr condition<std::string, condition_operator::CUSTOM, param<T>> bind(const char* name, T&& value) const {
    return *this << param<T>{name, value};
  }

  template <typename T = std::nullopt_t>
  constexpr condition<std::string, condition_operator::CUSTOM, std::nullopt_t> asCondition() const {
    return *this;
  }
};

namespace literals {
struct dynamic_field {
  const char* name;

  template <typename T>
  constexpr condition<field<T>, condition_operator::ASSIGNMENT, T> operator=(const T& right) const {
    return {field<T>{name}, right};
  }

  template <typename T>
  constexpr condition<field<T>, condition_operator::EQUALS, T> operator==(const T& right) const {
    return {field<T>{name}, right};
  }

  template <typename T = std::nullopt_t>
  constexpr condition<field<T>, condition_operator::IS_NULL, std::nullopt_t> operator==(std::nullopt_t right) const {
    return {field<T>{name}, right};
  }

  template <typename T>
  constexpr condition<field<T>, condition_operator::NOT_EQUALS, T> operator!=(const T& right) const {
    return {field<T>{name}, right};
  }

  template <typename T = std::nullopt_t>
  constexpr condition<field<T>, condition_operator::IS_NOT_NULL, std::nullopt_t> operator!=(std::nullopt_t right) const {
    return {field<T>{name}, right};
  }

  template <typename T>
  constexpr condition<field<T>, condition_operator::LOWER_THAN, T> operator<(const T& right) const {
    return {field<T>{name}, right};
  }

  template <typename T>
  constexpr condition<field<T>, condition_operator::LOWER_THAN_EQUALS, T> operator<=(const T& right) const {
    return {field<T>{name}, right};
  }

  template <typename T>
  constexpr condition<field<T>, condition_operator::GREATER_THAN, T> operator>(const T& right) const {
    return {field<T>{name}, right};
  }

  template <typename T>
  constexpr condition<field<T>, condition_operator::GREATER_THAN_EQUALS, T> operator>=(const T& right) const {
    return {field<T>{name}, right};
  }

  template <typename T>
  constexpr condition<field<T>, condition_operator::IN, std::vector<T>> IN(const std::vector<T>& right) const {
    return {field<T>{name}, right};
  }

  template <typename T = bool>
  constexpr operator condition<field<T>, condition_operator::EQUALS, bool>() const {
    return field<T>{name} == true;
  }

  template <typename T = bool>
  constexpr condition<field<T>, condition_operator::EQUALS, bool> operator!() const {
    return field<T>{name} == false;
  }
};

constexpr dynamic_field operator ""_f(const char* name, std::size_t) {
  return {name};
}

db::orm::query operator ""_q(const char* code, std::size_t) {
  return {code};
}
}

namespace detail {
template<class... Ts>
std::tuple<Ts const&...> ctie(Ts&... vs){
  return std::tie(std::cref(vs)...);
}
}
} // namespace orm

orm::selector<std::nullopt_t> connection::select() {
  orm::selector<std::nullopt_t> builder{*this};
  builder.select();
  return builder;
}

orm::selector<std::nullopt_t> connection::select(const std::vector<orm::selection>& fields) {
  orm::selector<std::nullopt_t> builder{*this};
  builder.select(fields);
  return builder;
}

orm::inserter<std::nullopt_t> connection::insert() {
  orm::inserter<std::nullopt_t> builder{*this};
  return builder;
}

orm::updater<std::nullopt_t> connection::update() {
  orm::updater<std::nullopt_t> builder{*this};
  return builder;
}

orm::deleter<std::nullopt_t> connection::remove() {
  orm::deleter<std::nullopt_t> builder{*this};
  return builder;
}
} // namespace db

#ifndef FOR_EACH
#define FIRST_ARG(N, ...) N
#define FIRST_ARG_IN_ARRAY(N, ...) N,
#define FIRST_ARG_AS_STRING(N, ...) #N
#define FIRST_ARG_AS_STRING_IN_ARRAY(N, ...) #N,
#define FIRST_ARG_IN_ARRAY_APPEND(N, ...) , N

// Make a FOREACH macro
#define FE_00(WHAT)
#define FE_01(WHAT, X) WHAT(X)
#define FE_02(WHAT, X, ...) WHAT(X) FE_01(WHAT, __VA_ARGS__)
#define FE_03(WHAT, X, ...) WHAT(X) FE_02(WHAT, __VA_ARGS__)
#define FE_04(WHAT, X, ...) WHAT(X) FE_03(WHAT, __VA_ARGS__)
#define FE_05(WHAT, X, ...) WHAT(X) FE_04(WHAT, __VA_ARGS__)
#define FE_06(WHAT, X, ...) WHAT(X) FE_05(WHAT, __VA_ARGS__)
#define FE_07(WHAT, X, ...) WHAT(X) FE_06(WHAT, __VA_ARGS__)
#define FE_08(WHAT, X, ...) WHAT(X) FE_07(WHAT, __VA_ARGS__)
#define FE_09(WHAT, X, ...) WHAT(X) FE_08(WHAT, __VA_ARGS__)
#define FE_10(WHAT, X, ...) WHAT(X) FE_09(WHAT, __VA_ARGS__)
#define FE_11(WHAT, X, ...) WHAT(X) FE_10(WHAT, __VA_ARGS__)
#define FE_12(WHAT, X, ...) WHAT(X) FE_11(WHAT, __VA_ARGS__)
#define FE_13(WHAT, X, ...) WHAT(X) FE_12(WHAT, __VA_ARGS__)
#define FE_14(WHAT, X, ...) WHAT(X) FE_13(WHAT, __VA_ARGS__)
#define FE_15(WHAT, X, ...) WHAT(X) FE_14(WHAT, __VA_ARGS__)
#define FE_16(WHAT, X, ...) WHAT(X) FE_15(WHAT, __VA_ARGS__)
#define FE_17(WHAT, X, ...) WHAT(X) FE_16(WHAT, __VA_ARGS__)
#define FE_18(WHAT, X, ...) WHAT(X) FE_17(WHAT, __VA_ARGS__)
#define FE_19(WHAT, X, ...) WHAT(X) FE_18(WHAT, __VA_ARGS__)
#define FE_20(WHAT, X, ...) WHAT(X) FE_19(WHAT, __VA_ARGS__)
//... repeat as needed

#define GET_MACRO(_00, _01, _02, _03, _04, _05, _06, _07, _08, _09, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, \
    _20, NAME, ...)                                                                                                   \
  NAME
#define FOR_EACH(ACTION, ...)                                                                                     \
  GET_MACRO(_00, __VA_ARGS__, FE_20, FE_19, FE_18, FE_17, FE_16, FE_15, FE_14, FE_13, FE_12, FE_11, FE_10, FE_09, \
      FE_08, FE_07, FE_06, FE_05, FE_04, FE_03, FE_02, FE_01, FE_00)                                              \
  (ACTION, __VA_ARGS__)
#endif

#define DB_ORM_STRINGIFY_META_FIELD(FIELD)                                   \
  {#FIELD, db::orm::field_type_converter<decltype(class_type::FIELD)>::type, \
      db::orm::field_type_converter<decltype(class_type::FIELD)>::optional},

#define DB_ORM_SERIALIZER_STATEMENT_SET(FIELD) statement.params[":" #FIELD] = source.FIELD;

#define DB_ORM_SERIALIZER_STATEMENT_BY_I_SET(FIELD) statement.params[std::string{":"} + std::to_string(i++)] = source.FIELD;

#define DB_ORM_DESERIALIZER_RESULTSET_GET(FIELD) resultset.get(#FIELD, result.FIELD);

#define DB_ORM_FIELD_INIT(FIELD) static constexpr db::orm::field<decltype(class_type::FIELD)> FIELD{#FIELD};

#define DB_ORM_PRIMARY_KEY_FIELD_TYPE_IN_TUPLE(FIELD) , decltype(class_type::FIELD)

#define DB_ORM_PRIMARY_KEY_GET_FIRST_ARG_IN_ARRAY_APPEND(FIELD) , source.FIELD

#define DB_ORM_STRINGIFY_TYPE_FIELD(FIELD) os << "  ." << #FIELD << " = " << obj.FIELD << std::endl;

#define DB_ORM_PRIMARY_KEY(TYPE, FIELD0, ...)                                                              \
  namespace db::orm {                                                                                            \
  template <>                                                                                                    \
  struct primary<TYPE> {                                                                                         \
    using class_type = TYPE;                                                                                     \
    using tuple_type =                                                                                           \
        std::tuple<decltype(class_type::FIELD0) __VA_OPT__(FOR_EACH(DB_ORM_PRIMARY_KEY_FIELD_TYPE_IN_TUPLE, __VA_ARGS__))>;       \
                                                                                                                 \
    static constexpr bool specialized = true;                                                                    \
    static constexpr db::orm::field_info class_members[] = {DB_ORM_STRINGIFY_META_FIELD(FIELD0) __VA_OPT__(FOR_EACH(DB_ORM_STRINGIFY_META_FIELD, __VA_ARGS__))}; \
                                                                                                                 \
    static auto tie(TYPE& source) {                                                                              \
      return std::tie(source.FIELD0 __VA_OPT__(FOR_EACH(DB_ORM_PRIMARY_KEY_GET_FIRST_ARG_IN_ARRAY_APPEND, __VA_ARGS__)));         \
    }                                                                                                            \
  };                                                                                                             \
  }

#define DB_ORM_SPECIALIZE(TYPE, FIELDS...)                                                                  \
  namespace db::orm {                                                                                       \
  template <>                                                                                               \
  struct meta<TYPE> {                                                                                       \
    using class_type = TYPE;                                                                                \
                                                                                                            \
    static constexpr bool specialized = true;                                                               \
    static constexpr const char class_name[] = #TYPE;                                                        \
    static constexpr db::orm::field_info class_members[] = {FOR_EACH(DB_ORM_STRINGIFY_META_FIELD, FIELDS)}; \
                                                                                                            \
    static void serialize(db::statement& statement, const TYPE& source) {                                   \
      FOR_EACH(DB_ORM_SERIALIZER_STATEMENT_SET, FIELDS)                                                     \
    }                                                                                                       \
                                                                                                            \
    static void serialize(db::statement& statement, const TYPE& source, int& i) {                                   \
      FOR_EACH(DB_ORM_SERIALIZER_STATEMENT_BY_I_SET, FIELDS)                                                     \
    }                                                                                                       \
                                                                                                            \
    static void deserialize(db::resultset& resultset, TYPE& result) {                                       \
      FOR_EACH(DB_ORM_DESERIALIZER_RESULTSET_GET, FIELDS)                                                   \
    }                                                                                                       \
  };                                                                                                        \
                                                                                                            \
  template <>                                                                                               \
  struct fields<TYPE> {                                                                                     \
    using class_type = TYPE;                                                                                \
                                                                                                            \
    static constexpr bool specialized = true;                                                               \
                                                                                                            \
    FOR_EACH(DB_ORM_FIELD_INIT, FIELDS)                                                                     \
  };                                                                                                        \
  }\
  \
  std::ostream& operator<<(std::ostream& os, const TYPE& obj) {\
    os << #TYPE << " {" << std::endl;\
    FOR_EACH(DB_ORM_STRINGIFY_TYPE_FIELD, FIELDS)                                                   \
    os << "}" << std::endl;\
\
    return os;\
  }\
  \
  std::ostream& operator<<(std::ostream& os, const std::optional<TYPE>& obj) {\
    if (obj) {\
      os << *obj;\
    } else {\
      os << #TYPE << " { <null> }" << std::endl;\
    }\
\
    return os;\
  }

namespace db {
template <typename T>
void serialize(db::statement& statement, const T& source) {
  db::orm::meta<T>::serialize(statement, source);
}

template <typename T>
void deserialize(db::resultset& resultset, T& result) {
  db::orm::meta<T>::deserialize(resultset, result);
}
} // namespace db

namespace db::orm {
// template <typename ...A>
// struct call {
//   const char* name;
//   std::tuple<A...> args;

//   template <typename T>
//   constexpr condition<call, EQUALS, T> operator==(const T& right) const {
//     return {std::move(*this), right};
//   }

//   template <typename T>
//   constexpr condition<call, IS_NULL, std::nullopt_t> operator==(std::nullopt_t right) const {
//     return {std::move(*this), right};
//   }

//   template <typename T>
//   constexpr condition<call, NOT_EQUALS, T> operator!=(const T& right) const {
//     return {std::move(*this), right};
//   }

//   template <typename T>
//   constexpr condition<call, IS_NOT_NULL, std::nullopt_t> operator!=(std::nullopt_t right) const {
//     return {std::move(*this), right};
//   }

//   template <typename T>
//   constexpr condition<call, LOWER_THAN, T> operator<(const T& right) const {
//     return {std::move(*this), right};
//   }

//   template <typename T>
//   constexpr condition<call, LOWER_THAN_EQUALS, T> operator<=(const T& right) const {
//     return {std::move(*this), right};
//   }

//   template <typename T>
//   constexpr condition<call, GREATER_THAN, T> operator>(const T& right) const {
//     return {std::move(*this), right};
//   }

//   template <typename T>
//   constexpr condition<call, GREATER_THAN_EQUALS, T> operator>=(const T& right) const {
//     return {std::move(*this), right};
//   }

//   void appendToQuery(std::ostream& os, int& i) const {
//     os << name << "(";

//     std::apply([&os](auto&&... args) {((os << args), ...);}, args);

//     os << ")";
//   }
// };

// struct func {
//   const char* name;

//   template <typename ...A>
//   constexpr call<A...> operator()(A&&... args) {
//     return {name, std::forward_as_tuple(args...)};
//   }
// };
} // namespace db::orm
