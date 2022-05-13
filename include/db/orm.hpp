#pragma once

#include "./resultset.hpp"
#include <ostream>
#include <string>
#include <set>

namespace db {
namespace orm {
template <typename T>
struct field_type_converter {
  static constexpr field_type type = UNKNOWN;
  static constexpr bool optional = false;

  using decayed = void;
};

template <typename T>
struct field_type_converter<std::optional<T>> {
  static constexpr field_type type = field_type_converter<T>::type;
  static constexpr bool optional = true;

  using decayed = T;
};

template <typename T>
struct is_condition_or_field {
  static constexpr bool value = false;
};

template <typename T>
struct is_std_set {
  static constexpr bool value = false;
};

template <typename T>
struct is_std_set<std::set<T>> {
  static constexpr bool value = true;
};

template <typename T>
struct is_std_function {
  static constexpr bool value = false;
};

template <typename T>
struct is_std_function<std::function<T>> {
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
      condition<_L, _O, _R>&& right) {
    return {std::move(*this), std::move(right)};
  }

  template <typename _L, condition_operator _O, typename _R>
  constexpr condition<condition, condition_operator::OR, condition<_L, _O, _R>> operator||(
      condition<_L, _O, _R>&& right) {
    return {std::move(*this), std::move(right)};
  }

  constexpr condition<std::nullopt_t, condition_operator::NOT, condition> operator!() {
    return {std::nullopt, std::move(*this)};
  }

  template <typename T>
  constexpr condition<condition, condition_operator::EQUALS, T> operator==(const T& right) const {
    return {std::move(*this), right};
  }

  template <typename T>
  constexpr condition<condition, condition_operator::EQUALS, T> operator==(T&& right) const {
    return {std::move(*this), std::move(right)};
  }

  template <typename T>
  constexpr condition<condition, condition_operator::NOT_EQUALS, T> operator!=(const T& right) const {
    return {std::move(*this), right};
  }

  template <typename T>
  constexpr condition<condition, condition_operator::NOT_EQUALS, T> operator!=(T&& right) const {
    return {std::move(*this), std::move(right)};
  }

  condition_operator getOperator() const override {
    return O;
  }

  void appendLeftToQuery(std::ostream& os, int& i) const override {
    if constexpr ((O | condition_operator::CUSTOM) == condition_operator::CUSTOM) {
      if constexpr (is_std_function<R>::value) {
        left(os, i);
      } else {
        os << left;
      }
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
      } else if constexpr (is_std_set<R>::value) {
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
        if constexpr (is_std_function<R>::value) {
          right(statement, i);
        } else {
          statement.params[right.name] = right.value;
        }
      } else if constexpr (is_std_set<R>::value) {
        for (const auto& item : right) {
          statement.params[std::string{":"} + std::to_string(i++)] = std::move(item);
        }
      } else {
        statement.params[std::string{":"} + std::to_string(i++)] = std::move(right);
      }
    }
  }
};

template <typename T>
struct field {
  const char* name;
  const char* source = "";

  constexpr condition<field, condition_operator::ASSIGNMENT, T> operator=(const T& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::ASSIGNMENT, T> operator=(T&& right) const {
    return {*this, std::move(right)};
  }

  template <typename T2>
  constexpr condition<field, condition_operator::ASSIGNMENT, field<T2>> operator=(const field<T2>& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::EQUALS, T> operator==(const T& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::EQUALS, T> operator==(T&& right) const {
    return {*this, std::move(right)};
  }

  template <typename T2>
  constexpr condition<field, condition_operator::EQUALS, field<T2>> operator==(const field<T2>& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::NOT_EQUALS, T> operator!=(const T& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::NOT_EQUALS, T> operator!=(T&& right) const {
    return {*this, std::move(right)};
  }

  template <typename T2>
  constexpr condition<field, condition_operator::NOT_EQUALS, field<T2>> operator!=(const field<T2>& right) const {
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

  constexpr condition<field, condition_operator::IN, std::set<T>> IN(const std::set<T>& right) const {
    return {*this, right};
  }

  constexpr condition<field, condition_operator::IN, std::set<T>> IN(std::set<T>&& right) const {
    return {*this, std::move(right)};
  }

  constexpr operator condition<field, condition_operator::EQUALS, bool>() const {
    return *this == true;
  }

  constexpr condition<field, condition_operator::EQUALS, bool> operator!() const {
    return *this == false;
  }

  inline void appendToQuery(std::ostream& os, int&) const {
    if (source != nullptr && source != "") {
      os << source << '.';
    }

    os << '"' << name << '"';
  }

  // operator const char*() {
  //   return name;
  // }

  // operator std::string() {
  //   return name;
  // }

  // operator std::string_view() {
  //   return name;
  // }
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
struct param {
  const char* name;
  T value;
};

struct query {
  std::string code;

  query(std::string&& code) {
    this->code = std::move(code);
  }

  query(const char* code) {
    this->code = code;
  }

  template <typename T = std::nullopt_t>
  constexpr condition<std::string, condition_operator::CUSTOM, std::nullopt_t> asCondition() const {
    return {std::move(code), std::nullopt};
  }

  template <typename T>
  constexpr condition<std::string, condition_operator::CUSTOM, param<T>> bind(const char* name, T&& value) const {
    return {std::move(code), param<T>{name, std::move(value)}};
  }
};

// struct dynamic_field {
//   const char* name;

//   template <typename T>
//   constexpr condition<field<T>, condition_operator::ASSIGNMENT, T> operator=(const T& right) const {
//     return {field<T>{name}, right};
//   }

//   template <typename T>
//   constexpr condition<field<T>, condition_operator::EQUALS, T> operator==(const T& right) const {
//     return {field<T>{name}, right};
//   }

//   template <typename T = std::nullopt_t>
//   constexpr condition<field<T>, condition_operator::IS_NULL, std::nullopt_t> operator==(std::nullopt_t right) const {
//     return {field<T>{name}, right};
//   }

//   template <typename T>
//   constexpr condition<field<T>, condition_operator::NOT_EQUALS, T> operator!=(const T& right) const {
//     return {field<T>{name}, right};
//   }

//   template <typename T = std::nullopt_t>
//   constexpr condition<field<T>, condition_operator::IS_NOT_NULL, std::nullopt_t> operator!=(std::nullopt_t right)
//   const {
//     return {field<T>{name}, right};
//   }

//   template <typename T>
//   constexpr condition<field<T>, condition_operator::LOWER_THAN, T> operator<(const T& right) const {
//     return {field<T>{name}, right};
//   }

//   template <typename T>
//   constexpr condition<field<T>, condition_operator::LOWER_THAN_EQUALS, T> operator<=(const T& right) const {
//     return {field<T>{name}, right};
//   }

//   template <typename T>
//   constexpr condition<field<T>, condition_operator::GREATER_THAN, T> operator>(const T& right) const {
//     return {field<T>{name}, right};
//   }

//   template <typename T>
//   constexpr condition<field<T>, condition_operator::GREATER_THAN_EQUALS, T> operator>=(const T& right) const {
//     return {field<T>{name}, right};
//   }

//   template <typename T>
//   constexpr condition<field<T>, condition_operator::IN, std::set<T>> IN(const std::set<T>& right) const {
//     return {field<T>{name}, right};
//   }

//   template <typename T = bool>
//   constexpr operator condition<field<T>, condition_operator::EQUALS, bool>() const {
//     return field<T>{name} == true;
//   }

//   template <typename T = bool>
//   constexpr condition<field<T>, condition_operator::EQUALS, bool> operator!() const {
//     return field<T>{name} == false;
//   }
// };

namespace literals {
// constexpr inline db::orm::dynamic_field operator ""_f(const char* name, std::size_t) {
//   return {name};
// }

inline db::orm::query operator""_q(const char* code, std::size_t) {
  return {code};
}
} // namespace literals

template <typename T>
class selection_iterator {
public:
  explicit selection_iterator(db::resultset::iterator&& it) : _it(it) {
  }

  selection_iterator(const selection_iterator&) = default;

  selection_iterator(selection_iterator&&) = default;

  selection_iterator& operator=(const selection_iterator&) = default;

  selection_iterator& operator=(selection_iterator&&) = default;

  selection_iterator& operator++() {
    _it++;
    return *this;
  }

  selection_iterator operator++(int) {
    return selection_iterator{++_it};
  }

  bool operator==(const selection_iterator& other) const {
    return _it == other._it;
  }

  bool operator!=(const selection_iterator& other) const {
    return _it != other._it;
  }

  T& operator*() {
    db::orm::meta<T>::deserialize(*_it, _value);
    return _value;
  }

private:
  db::resultset::iterator _it;

  T _value;
};

template <typename T>
class selection_iterable {
public:
  explicit selection_iterable(db::statement& stmt) : _rslt{stmt} {
  }

  selection_iterator<T> begin() {
    return selection_iterator<T>{_rslt.begin()};
  }

  selection_iterator<T> end() {
    return selection_iterator<T>{_rslt.end()};
  }

  std::vector<T> toVector() {
    std::vector<T> result;
    for (T& value : *this) {
      result.push_back(std::move(value));
    }
    return result;
  }

private:
  db::resultset _rslt;
};

class selector {
public:
  class joiner {
  public:
    friend selector;

    joiner(selector& s, query_builder_data::join_on_clause& d);

    joiner& inner();

    joiner& left();

    joiner& cross();

    template <typename L, condition_operator O, typename R>
    selector& on(db::orm::condition<L, O, R>&& condition) {
      _data.condition =
          std::make_shared<db::orm::condition<L, O, R>>(std::move(condition.left), std::move(condition.right));

      return _selector;
    }

    selector& on(db::orm::query&& condition);

    selector& always();

  private:
    selector& _selector;
    query_builder_data::join_on_clause& _data;
  };

  selector(db::connection& connection);

  selector& select();

  selector& select(const std::vector<selection>& fields);

  selector& distinct(bool distinct = true);

  selector& from(std::string_view table, std::string_view alias = "");

  template <typename C>
  selector& from(const db::orm::fields<C>& f = "") {
    return from(db::orm::fields<C>::class_name, f.__alias);
  }

  joiner& join(std::string_view table, std::string_view alias = "");

  template <typename C>
  joiner& join(const db::orm::fields<C>& f = "") {
    return join(db::orm::fields<C>::class_name, f.__alias);
  }

  template <typename L, condition_operator O, typename R>
  selector& where(db::orm::condition<L, O, R>&& condition) {
    if (!_push_next) {
      _push_next = true;
      return *this;
    }

    auto ptr = std::make_shared<db::orm::condition<L, O, R>>(std::move(condition.left), std::move(condition.right));
    _data.conditions.push_back(ptr);

    return *this;
  }

  selector& where(db::orm::query&& condition);

  selector& groupBy(std::string_view field);

  template <typename F>
  selector& groupBy(db::orm::field<F> field) {
    return groupBy(field.name);
  }

  template <typename L, condition_operator O, typename R>
  selector& having(db::orm::condition<L, O, R>&& condition) {
    if (!_push_next) {
      _push_next = true;
      return *this;
    }

    auto ptr = std::make_shared<db::orm::condition<L, O, R>>(std::move(condition.left), std::move(condition.right));
    _data.having_conditions.push_back(ptr);

    return *this;
  }

  selector& having(db::orm::query&& condition);

  selector& orderBy(
      std::string_view field, order_by_direction direction = DIRECTION_DEFAULT, order_by_nulls nulls = NULLS_DEFAULT);

  template <typename F>
  selector& orderBy(
      db::orm::field<F> field, order_by_direction direction = DIRECTION_DEFAULT, order_by_nulls nulls = NULLS_DEFAULT) {
    return orderBy(field.name, direction, nulls);
  }

  selector& offset(uint32_t offset);

  selector& limit(uint32_t limit);

  template <typename T>
  selector& bind(const char* name, T&& value) {
    using Condition = db::orm::condition<std::string, db::orm::condition_operator::CUSTOM, db::orm::param<T>>;

    auto ptr = std::make_shared<Condition>("", db::orm::param<T>{name, std::move(value)});
    _params.push_back(ptr);

    return *this;
  }

  void prepare(db::statement& statement);

  db::resultset findMany();

  template <typename C>
  selection_iterable<C> findMany(const db::orm::fields<C>& f = "") {
    selectClassFields<C>();

    db::statement statement{_connection};
    prepare(statement);
    return selection_iterable<C>{statement};
  }

  db::resultset findOne();

  template <typename C>
  std::optional<C> findOne(const db::orm::fields<C>& f = "") {
    selectClassFields<C>();

    _data.limit = 1;

    for (auto row : findMany<C>()) {
      return {std::move(row)};
    }

    return std::nullopt;
  }

  // selector& pushNextIf(bool push_next);

  condition<std::function<void(std::ostream&, int&)>, condition_operator::CUSTOM,
      std::function<void(db::statement&, int&)>>
  exists(const char* name, bool value);

private:
  db::connection& _connection;
  db::orm::query_builder_data _data;

  std::shared_ptr<joiner> _joiner;

  std::vector<std::shared_ptr<db::orm::condition_container>> _params;

  bool _push_next = true;

  template <typename C>
  void selectClassFields() {
    if (_data.table.name == db::orm::meta<C>::class_name) {
      for (const auto& existing : db::orm::meta<C>::class_members) {
        if (_data.table.alias.empty()) {
          _data.fields.push_back('"' + std::string{existing.name} + '"');
        } else {
          _data.fields.push_back(_data.table.alias + '.' + '"' + std::string{existing.name} + '"');
        }
      }
    }
  }
};

class updater {
public:
  // friend db::orm::inserter;

  updater(db::connection& connection);

  updater& in(std::string_view table, std::string_view alias = "");

  template <typename C>
  updater& in(const db::orm::fields<C>& f = "") {
    return in(db::orm::fields<C>::class_name, f.__alias);
  }

  template <typename L, db::orm::condition_operator O, typename R>
  updater& set(db::orm::condition<L, O, R>&& assignment) {
    auto ptr = std::make_shared<db::orm::condition<L, O, R>>(std::move(assignment.left), std::move(assignment.right));
    _data.assignments.push_back(ptr);

    return *this;
  }

  updater& set(db::orm::query&& query);

  template <typename L, db::orm::condition_operator O, typename R>
  updater& where(db::orm::condition<L, O, R>&& condition) {
    auto ptr = std::make_shared<db::orm::condition<L, O, R>>(std::move(condition.left), std::move(condition.right));
    _data.conditions.push_back(ptr);

    return *this;
  }

  updater& where(db::orm::query&& query);

  void prepare(db::statement& statement);

  int executeUpdate();

private:
  db::connection& _connection;
  db::orm::query_builder_data _data;

  std::function<int(db::statement&)> _prepare;
};

class inserter {
public:
  class on_conflict {
  public:
    on_conflict(inserter& i);

    inserter& doNothing();

    inserter& doReplace();

    // updater<T>& doUpdate();

  private:
    inserter& _inserter;
    // updater<T> _updater;
  };

  inserter(db::connection& connection);

  inserter& into(std::string_view table);

  template <typename C>
  inserter& into(const db::orm::fields<C>& f = "") {
    return into(db::orm::fields<C>::class_name);
  }

  template <typename L, db::orm::condition_operator O, typename R>
  inserter& set(db::orm::condition<L, O, R>&& assignment) {
    auto ptr = std::make_shared<db::orm::condition<L, O, R>>(std::move(assignment.left), std::move(assignment.right));
    _data.assignments.push_back(ptr);

    return *this;
  }

  inserter& set(db::orm::query&& query);

  on_conflict& onConflict(const std::vector<std::string>& fields);

  int prepare(db::statement& statement);

  int executeUpdate();

private:
  db::connection& _connection;
  db::orm::query_builder_data _data;

  on_conflict _on_conflict{*this};
};

class deleter {
public:
  deleter(db::connection& connection);

  deleter& from(std::string_view table, std::string_view alias = "");

  template <typename C>
  deleter& from(const db::orm::fields<C>& f = "") {
    return from(db::orm::fields<C>::class_name, f.__alias);
  }

  template <typename L, condition_operator O, typename R>
  deleter& where(db::orm::condition<L, O, R>&& condition) {
    auto ptr = std::make_shared<db::orm::condition<L, O, R>>(std::move(condition.left), std::move(condition.right));
    _data.conditions.push_back(ptr);

    return *this;
  }

  deleter& where(db::orm::query&& query);

  void prepare(db::statement& statement);

  int executeUpdate();

private:
  db::connection& _connection;
  db::orm::query_builder_data _data;
};

namespace detail {
template <class... Ts>
std::tuple<Ts const&...> ctie(Ts&... vs) {
  return std::tie(std::cref(vs)...);
}
} // namespace detail
} // namespace orm
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

#define DB_ORM_SERIALIZER_STATEMENT_BY_I_SET(FIELD)                    \
  if (std::find(fields.begin(), fields.end(), #FIELD) != fields.end()) \
    statement.params[std::string{":"} + std::to_string(i++)] = source.FIELD;

#define DB_ORM_DESERIALIZER_RESULTSET_GET(FIELD) resultset.get(#FIELD, result.FIELD);

#define DB_ORM_FIELD_CHANGES(FIELD)   \
  if (source.FIELD != original.FIELD) \
    result.push_back(#FIELD);

#define DB_ORM_FIELD_INIT(FIELD) db::orm::field<decltype(class_type::FIELD)> FIELD{#FIELD};

#define DB_ORM_FIELD_HAS(FIELD) \
  if (field == #FIELD)          \
    return true;

#define DB_ORM_FIELD_SET_SOURCE(FIELD) FIELD.source = source;

#define DB_ORM_PRIMARY_KEY_FIELD_TYPE_IN_TUPLE(FIELD) , decltype(class_type::FIELD)

#define DB_ORM_PRIMARY_KEY_GET_FIRST_ARG_IN_ARRAY_APPEND(FIELD) , source.FIELD

#define DB_ORM_STRINGIFY_TYPE_FIELD(FIELD) os << "  ." << #FIELD << " = " << obj.FIELD << std::endl;

#define DB_ORM_PRIMARY_KEY(TYPE, FIELD0, ...)                                                                 \
  namespace db::orm {                                                                                         \
  template <>                                                                                                 \
  struct primary<TYPE> {                                                                                      \
    using class_type = TYPE;                                                                                  \
    using tuple_type = std::tuple<decltype(class_type::FIELD0) __VA_OPT__(                                    \
        FOR_EACH(DB_ORM_PRIMARY_KEY_FIELD_TYPE_IN_TUPLE, __VA_ARGS__))>;                                      \
                                                                                                              \
    static constexpr bool specialized = true;                                                                 \
    static constexpr db::orm::field_info class_members[] = {                                                  \
        DB_ORM_STRINGIFY_META_FIELD(FIELD0) __VA_OPT__(FOR_EACH(DB_ORM_STRINGIFY_META_FIELD, __VA_ARGS__))};  \
                                                                                                              \
    static auto tie(TYPE& source) {                                                                           \
      return std::tie(                                                                                        \
          source.FIELD0 __VA_OPT__(FOR_EACH(DB_ORM_PRIMARY_KEY_GET_FIRST_ARG_IN_ARRAY_APPEND, __VA_ARGS__))); \
    }                                                                                                         \
  };                                                                                                          \
  }

#define DB_ORM_SPECIALIZE(TYPE, FIELDS...)                                                                   \
  namespace db::orm {                                                                                        \
  template <>                                                                                                \
  struct meta<TYPE> {                                                                                        \
    using class_type = TYPE;                                                                                 \
                                                                                                             \
    static constexpr bool specialized = true;                                                                \
    static constexpr const char class_name[] = #TYPE;                                                        \
    static constexpr db::orm::field_info class_members[] = {FOR_EACH(DB_ORM_STRINGIFY_META_FIELD, FIELDS)};  \
                                                                                                             \
    static void serialize(db::statement& statement, const TYPE& source) {                                    \
      FOR_EACH(DB_ORM_SERIALIZER_STATEMENT_SET, FIELDS);                                                     \
    }                                                                                                        \
                                                                                                             \
    static void serialize(                                                                                   \
        db::statement& statement, const TYPE& source, int& i, const std::vector<std::string_view>& fields) { \
      FOR_EACH(DB_ORM_SERIALIZER_STATEMENT_BY_I_SET, FIELDS);                                                \
    }                                                                                                        \
                                                                                                             \
    static void deserialize(db::resultset& resultset, TYPE& result) {                                        \
      FOR_EACH(DB_ORM_DESERIALIZER_RESULTSET_GET, FIELDS);                                                   \
    }                                                                                                        \
                                                                                                             \
    static std::vector<std::string_view> changes(const TYPE& source, const TYPE& original = {}) {            \
      std::vector<std::string_view> result;                                                                  \
      FOR_EACH(DB_ORM_FIELD_CHANGES, FIELDS);                                                                \
      return result;                                                                                         \
    }                                                                                                        \
  };                                                                                                         \
                                                                                                             \
  template <>                                                                                                \
  struct fields<TYPE> {                                                                                      \
    using class_type = TYPE;                                                                                 \
                                                                                                             \
    static constexpr bool specialized = true;                                                                \
    static constexpr const char class_name[] = #TYPE;                                                        \
                                                                                                             \
    const char* __alias = "";                                                                                \
                                                                                                             \
    constexpr fields(const char* source = "") {                                                              \
      __alias = source;                                                                                      \
                                                                                                             \
      FOR_EACH(DB_ORM_FIELD_SET_SOURCE, FIELDS);                                                             \
    }                                                                                                        \
                                                                                                             \
    static bool has(std::string_view field) {                                                                \
      FOR_EACH(DB_ORM_FIELD_HAS, FIELDS);                                                                    \
      return false;                                                                                          \
    }                                                                                                        \
                                                                                                             \
    FOR_EACH(DB_ORM_FIELD_INIT, FIELDS)                                                                      \
  };                                                                                                         \
  }                                                                                                          \
                                                                                                             \
  using _##TYPE = db::orm::fields<TYPE>;

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
