#pragma once

#include "./statement.hpp"
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace db {
class resultset {
public:
  class iterator {
  public:
    explicit iterator(resultset& rslt, bool done);

    iterator(const iterator&) = default;

    iterator(iterator&&) = default;

    iterator& operator=(const iterator&) = default;

    iterator& operator=(iterator&&) = default;

    iterator& operator++();

    iterator operator++(int);

    bool operator==(const iterator& other) const;

    bool operator!=(const iterator& other) const;

    resultset& operator*();

  private:
    resultset& _rslt;
    bool _done;
  };

  class columns_iterable {
  public:
    class iterator {
    public:
      explicit iterator(const resultset& rslt, int i, bool done);

      iterator(const iterator&) = default;

      iterator(iterator&&) = default;

      iterator& operator=(const iterator&) = default;

      iterator& operator=(iterator&&) = default;

      iterator& operator++();

      iterator operator++(int);

      bool operator==(const iterator& other) const;

      bool operator!=(const iterator& other) const;

      std::string_view operator*() const;

    private:
      const resultset& _rslt;
      int _i;
      bool _done;
      std::string _column_name;
    };

    explicit columns_iterable(const resultset& rslt);

    columns_iterable(const columns_iterable&) = delete;

    columns_iterable(columns_iterable&&) = default;

    columns_iterable& operator=(const columns_iterable&) = delete;

    columns_iterable& operator=(columns_iterable&&) = default;

    iterator begin() const;

    iterator end() const;

  private:
    const resultset& _rslt;
  };

  class polymorphic_field {
  public:
    explicit polymorphic_field(const resultset& rslt, std::string_view name);

    polymorphic_field(const polymorphic_field&) = delete;

    polymorphic_field(polymorphic_field&&) = default;

    polymorphic_field& operator=(const polymorphic_field&) = delete;

    polymorphic_field& operator=(polymorphic_field&&) = default;

    orm::field_type type() const;

    bool isNull() const;

    template <typename T>
    std::optional<T> get() const {
      return _rslt.get<T>(_name);
    }

    template <typename T>
    T value() const {
      return get<T>().value();
    }

    template <typename T>
    operator std::optional<T>() const {
      return get<T>();
    }

    template <typename T>
    operator T() const {
      return value<T>();
    }

  private:
    const resultset& _rslt;
    std::string _name;
  };

  explicit resultset(statement& stmt);

  resultset(const resultset&) = delete;

  resultset(resultset&&) = default;

  resultset& operator=(const resultset&) = delete;

  resultset& operator=(resultset&&) = delete;

  bool next();

  orm::field_type type(std::string_view name) const;

  bool isNull(std::string_view name) const;

  template <typename T>
  bool get(std::string_view name, T& result) const {
    if (_datasource_resultset->isValueNull((std::string)name)) {
      return false;
    }

    if constexpr (type_converter<T>::specialized) {
      typename type_converter<T>::db_type tmp;
      _datasource_resultset->getValue((std::string)name, tmp);
      result = type_converter<T>::deserialize(tmp);
    } else {
      _datasource_resultset->getValue((std::string)name, result);
    }

    return true;
  }

  template <typename T>
  inline void get(std::string_view name, std::optional<T>& result) const {
    T value;
    if (get<T>(name, value)) {
      result = std::move(value);
    } else {
      result = std::nullopt;
    }
  }

  template <typename T>
  inline std::optional<T> get(std::string_view name) const {
    std::optional<T> result;
    get<T>(name, result);
    return result;
  }

  template <typename T>
  inline T value(std::string_view name) const {
    T result;
    get<T>(name, result);
    return result;
  }

  polymorphic_field operator[](std::string_view name) const;

  iterator begin();

  iterator end();

  columns_iterable columns() const;

  template <typename T>
  std::optional<T> firstValue() {
    for (auto& rslt : *this) {
      for (const auto& col : rslt.columns()) {
        return rslt.get<T>(col);
      }
    }

    return {};
  }

  // template <typename ...T>
  // std::tuple<std::optional<T>...> firstTuple() {
  //   return {};
  // }

  template <typename T = datasource::resultset>
  std::shared_ptr<T> getNativeResultset() const {
    return std::dynamic_pointer_cast<T>(_datasource_resultset);
  }

private:
  mutable std::shared_ptr<datasource::resultset> _datasource_resultset;
};
} // namespace db
