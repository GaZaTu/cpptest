#pragma once

#include "./connection.hpp"
#include <memory>
#include <optional>
#include <stdint.h>
#include <string>
#include <string_view>

namespace db {
class statement {
public:
  friend resultset;

  friend class db::orm::selector;
  friend class db::orm::inserter;
  friend class db::orm::updater;
  friend class db::orm::deleter;

  class parameter {
  public:
    explicit parameter(statement& stmt, std::string_view name);

    parameter(const parameter&) = delete;

    parameter(parameter&&) = default;

    parameter& operator=(const parameter&) = delete;

    parameter& operator=(parameter&&) = default;

    parameter& operator=(bool value);

    parameter& operator=(int8_t value);

    parameter& operator=(uint8_t value);

    parameter& operator=(int16_t value);

    parameter& operator=(uint16_t value);

    parameter& operator=(int32_t value);

    parameter& operator=(uint32_t value);

    parameter& operator=(int64_t value);

    parameter& operator=(uint64_t value);

#ifdef __SIZEOF_INT128__
    parameter& operator=(__uint128_t value);
#endif

    parameter& operator=(float value);

    parameter& operator=(double value);

    parameter& operator=(long double value);

    parameter& operator=(std::string_view value);

    parameter& operator=(std::string&& value);

    parameter& operator=(const char* value);

    parameter& operator=(const std::vector<uint8_t>& value);

    template <typename T>
    inline parameter& operator=(const std::optional<T>& value) {
      setValue(value);
      return *this;
    }

    parameter& operator=(orm::date value);

    parameter& operator=(orm::time value);

    parameter& operator=(orm::timestamp value);

    parameter& operator=(std::nullopt_t);

  private:
    std::reference_wrapper<statement> _stmt;
    std::string _name;

    template <typename T>
    void setValue(const std::optional<T>& value) {
      if (value) {
        if constexpr (type_converter<T>::specialized) {
          setValue(type_converter<T>::serialize(*value));
        } else {
          setValue(*value);
        }
      } else {
        setNull();
      }
    }

    template <typename T>
    void setValue(T&& value) {
      _stmt.get().assertPrepared();
      _stmt.get()._datasource_statement->setParam(_name, value);
    }

    void setNull();
  };

  class parameter_access {
  public:
    explicit parameter_access(statement& stmt);

    parameter& operator[](std::string_view name);

  private:
    std::reference_wrapper<statement> _stmt;
    std::optional<parameter> _current_parameter;
  };

  parameter_access params{*this};

  explicit statement(connection& conn);

  statement(const statement&) = delete;

  statement(statement&&);

  statement& operator=(const statement&) = delete;

  statement& operator=(statement&&) = delete;

  void prepare(std::string_view script);

  bool prepared();

  void assertPrepared();

  int executeUpdate();

  bool readonly();

  template <typename T = datasource::statement>
  std::shared_ptr<T> getNativeStatement() {
    return std::dynamic_pointer_cast<T>(_datasource_statement);
  }

protected:
  std::reference_wrapper<connection> _conn;
  std::shared_ptr<datasource::statement> _datasource_statement;
};
} // namespace db
