#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace db {
class connection;

class sql_error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class datasource {
public:
  class resultset {
  public:
    virtual ~resultset() {
    }

    virtual bool next() = 0;

    virtual bool isValueNull(const std::string_view name) = 0;

    virtual void getValue(const std::string_view name, bool& result) = 0;

    virtual void getValue(const std::string_view name, int& result) = 0;

    virtual void getValue(const std::string_view name, double& result) = 0;

    virtual void getValue(const std::string_view name, std::string& result) = 0;

    virtual void getValue(const std::string_view name, std::vector<std::byte>& result) = 0;

    virtual int columnCount() = 0;

    virtual std::string columnName(int i) = 0;
  };

  class statement {
  public:
    virtual ~statement() {
    }

    virtual std::shared_ptr<resultset> execute() = 0;

    virtual int executeUpdate() = 0;

    virtual void setParamToNull(const std::string_view name) = 0;

    virtual void setParam(const std::string_view name, bool value) = 0;

    virtual void setParam(const std::string_view name, int value) = 0;

    virtual void setParam(const std::string_view name, double value) = 0;

    virtual void setParam(const std::string_view name, const std::string_view value) = 0;

    virtual void setParam(const std::string_view name, const std::vector<std::byte>& value) = 0;
  };

  class connection {
  public:
    virtual ~connection() {
    }

    virtual std::shared_ptr<statement> prepareStatement(const std::string_view script) = 0;

    virtual void beginTransaction() = 0;

    virtual void commit() = 0;

    virtual void rollback() = 0;

    virtual void execute(const std::string_view script) {
      prepareStatement(script)->execute();
    }
  };

  virtual std::shared_ptr<connection> getConnection() = 0;

  virtual std::shared_ptr<connection> getPooledConnection() {
    return getConnection();
  }

  void onConnectionOpen(std::function<void(db::connection&)> value) {
    _onConnectionOpen = std::move(value);
  }

  std::function<void(db::connection&)>& onConnectionOpen() {
    return _onConnectionOpen;
  }

  void onConnectionClose(std::function<void(db::connection&)> value) {
    _onConnectionClose = std::move(value);
  }

  std::function<void(db::connection&)>& onConnectionClose() {
    return _onConnectionClose;
  }

private:
  std::function<void(db::connection&)> _onConnectionOpen;
  std::function<void(db::connection&)> _onConnectionClose;
};

//    class pooled_datasource : public datasource {
//    public:
//      struct pooled_connection {
//      public:
//        std::shared_ptr<datasource::connection> connection;
//        std::chrono::system_clock::time_point open_since = std::chrono::system_clock::now();
//        bool in_use = false;
//      };

//      virtual std::shared_ptr<connection> getPooledConnection() override {

//      }

//    private:

//    };

class resultset;
class statement;
class transaction;
class connection;

class statement {
public:
  friend resultset;

  class parameter {
  public:
    explicit parameter(statement& stmt, const std::string_view name);

    parameter(const parameter&) = delete;

    parameter(parameter&&) = default;

    parameter& operator=(const parameter&) = delete;

    parameter& operator=(parameter&&) = default;

    void set(bool value);

    void set(int value);

    void set(double value);

    void set(const std::string_view value);

    void set(const char* value);

    template <typename T>
    void set(std::optional<T> value);

    void set(std::nullopt_t);

    parameter& operator=(bool value);

    parameter& operator=(int value);

    parameter& operator=(double value);

    parameter& operator=(const std::string_view value);

    parameter& operator=(const char* value);

    template <typename T>
    parameter& operator=(std::optional<T> value);

    parameter& operator=(std::nullopt_t);

  private:
    std::reference_wrapper<statement> _stmt;
    std::string _name;

    template <typename T>
    void setValueByOptional(std::optional<T> value);

    template <typename T>
    void setValue(T value);

    void setNull();
  };

  class parameter_access {
  public:
    explicit parameter_access(statement& stmt);

    parameter& operator[](const std::string_view name);

  private:
    std::reference_wrapper<statement> _stmt;
    std::optional<parameter> _current_parameter;
  };

  parameter_access params = parameter_access(*this);

  explicit statement(connection& conn);

  statement(const statement&) = delete;

  statement(statement&&) = delete;

  statement& operator=(const statement&) = delete;

  statement& operator=(statement&&) = delete;

  void prepare(const std::string_view script);

  bool prepared();

  void assertPrepared();

  resultset execute();

  int executeUpdate();

  template <typename T>
  std::optional<T> executeAndGetSingle();

private:
  std::reference_wrapper<connection> _conn;
  std::shared_ptr<datasource::statement> _datasource_statement;
};

class connection {
public:
  friend statement;
  friend transaction;

  explicit connection(datasource* dsrc) : _dsrc(dsrc) {
    _datasource_connection = dsrc->getConnection();

    auto& onConnectionOpen = _dsrc->onConnectionOpen();

    if (onConnectionOpen) {
      onConnectionOpen(*this);
    }
  }

  connection(const connection&) = delete;

  connection(connection&&) = delete;

  connection& operator=(const connection&) = delete;

  connection& operator=(connection&&) = delete;

  ~connection() {
    auto& onConnectionClose = _dsrc->onConnectionClose();

    if (onConnectionClose) {
      onConnectionClose(*this);
    }
  }

  void execute(const std::string_view script) {
    _datasource_connection->execute(script);
  }

private:
  datasource* _dsrc;
  std::shared_ptr<datasource::connection> _datasource_connection;
};

class transaction {
public:
  explicit transaction(connection& conn) : _conn(conn) {
    _conn.get()._datasource_connection->beginTransaction();
  }

  transaction(const transaction&) = delete;

  transaction(transaction&&) = delete;

  transaction& operator=(const transaction&) = delete;

  transaction& operator=(transaction&&) = delete;

  ~transaction() {
    if (!_didCommitOrRollback) {
      if (std::uncaught_exceptions() == 0) {
        commit();
      } else {
        rollback();
      }
    }
  }

  void commit() {
    _conn.get()._datasource_connection->commit();
    _didCommitOrRollback = true;
  }

  void rollback() {
    _conn.get()._datasource_connection->rollback();
    _didCommitOrRollback = true;
  }

private:
  std::reference_wrapper<connection> _conn;
  bool _didCommitOrRollback = false;
};

class resultset {
public:
  class iterator {
  public:
    explicit iterator(resultset& rslt, bool done) : _rslt(rslt), _done(done) {
      if (!_done) {
        ++(*this);
      }
    }

    iterator(const iterator&) = default;

    iterator(iterator&&) = default;

    iterator& operator=(const iterator&) = default;

    iterator& operator=(iterator&&) = default;

    iterator& operator++() {
      _done = !_rslt.get().next();
      return *this;
    }

    iterator operator++(int) {
      iterator result = *this;
      ++(*this);
      return result;
    }

    bool operator==(const iterator& other) const {
      return _done == other._done;
    }

    bool operator!=(const iterator& other) const {
      return !(*this == other);
    }

    resultset& operator*() {
      return _rslt.get();
    }

  private:
    std::reference_wrapper<resultset> _rslt;
    bool _done;
  };

  class columns_iterable {
  public:
    class iterator {
    public:
      explicit iterator(resultset& rslt, int i, bool done) : _rslt(rslt), _i(i), _done(done) {
        if (!_done) {
          ++(*this);
        }
      }

      iterator(const iterator&) = default;

      iterator(iterator&&) = default;

      iterator& operator=(const iterator&) = default;

      iterator& operator=(iterator&&) = default;

      iterator& operator++() {
        _done = (_i += 1) > _rslt.get()._datasource_resultset->columnCount();

        if (!_done) {
          _column_name = _rslt.get()._datasource_resultset->columnName(_i - 1);
        }

        return *this;
      }

      iterator operator++(int) {
        iterator result = *this;
        ++(*this);
        return result;
      }

      bool operator==(const iterator& other) const {
        return _done == other._done;
      }

      bool operator!=(const iterator& other) const {
        return !(*this == other);
      }

      const std::string& operator*() const {
        return _column_name;
      }

    private:
      std::reference_wrapper<resultset> _rslt;
      int _i;
      bool _done;
      std::string _column_name;
    };

    explicit columns_iterable(resultset& rslt) : _rslt(rslt) {
    }

    columns_iterable(const columns_iterable&) = delete;

    columns_iterable(columns_iterable&&) = default;

    columns_iterable& operator=(const columns_iterable&) = delete;

    columns_iterable& operator=(columns_iterable&&) = default;

    iterator begin() {
      return iterator(_rslt.get(), 0, false);
    }

    iterator end() {
      return iterator(_rslt.get(), -1, true);
    }

  private:
    std::reference_wrapper<resultset> _rslt;
  };

  class polymorphic_field {
  public:
    explicit polymorphic_field(resultset& rslt, const std::string_view name) : _rslt(rslt), _name(name) {
    }

    polymorphic_field(const polymorphic_field&) = delete;

    polymorphic_field(polymorphic_field&&) = default;

    polymorphic_field& operator=(const polymorphic_field&) = delete;

    polymorphic_field& operator=(polymorphic_field&&) = default;

    template <typename T>
    std::optional<T> get() {
      return _rslt.get().get<T>(_name);
    }

    template <typename T>
    operator std::optional<T>() {
      return get<T>();
    }

  private:
    std::reference_wrapper<resultset> _rslt;
    std::string _name;
  };

  explicit resultset(statement& stmt) : _stmt(stmt) {
    _datasource_resultset = stmt._datasource_statement->execute();
  }

  resultset(const resultset&) = delete;

  resultset(resultset&&) = delete;

  resultset& operator=(const resultset&) = delete;

  resultset& operator=(resultset&&) = delete;

  bool next() {
    return _datasource_resultset->next();
  }

  template <typename T>
  std::optional<T> get(const std::string_view name) {
    if (_datasource_resultset->isValueNull(name)) {
      return {};
    } else {
      T result;
      _datasource_resultset->getValue(name, result);
      return result;
    }
  }

  polymorphic_field operator[](const std::string_view name) {
    return polymorphic_field(*this, name);
  }

  iterator begin() {
    return iterator(*this, false);
  }

  iterator end() {
    return iterator(*this, true);
  }

  columns_iterable columns() {
    return columns_iterable(*this);
  }

private:
  std::reference_wrapper<statement> _stmt;
  std::shared_ptr<datasource::resultset> _datasource_resultset;
};

// statement::parameter

statement::parameter::parameter(statement& stmt, const std::string_view name) : _stmt(stmt), _name(name) {
}

void statement::parameter::set(bool value) {
  setValue(value);
}

void statement::parameter::set(int value) {
  setValue(value);
}

void statement::parameter::set(double value) {
  setValue(value);
}

void statement::parameter::set(const std::string_view value) {
  setValue(value);
}

void statement::parameter::set(const char* value) {
  set(std::string_view{value});
}

template <typename T>
void statement::parameter::set(std::optional<T> value) {
  setValueByOptional(value);
}

void statement::parameter::set(std::nullopt_t) {
  setNull();
}

statement::parameter& statement::parameter::operator=(bool value) {
  set(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(int value) {
  set(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(double value) {
  set(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(const std::string_view value) {
  set(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(const char* value) {
  return (*this) = std::string_view{value};
}

template <typename T>
statement::parameter& statement::parameter::operator=(std::optional<T> value) {
  set(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(std::nullopt_t) {
  setNull();
  return *this;
}

template <typename T>
void statement::parameter::setValueByOptional(std::optional<T> value) {
  if (value) {
    setValue(*value);
  } else {
    setNull();
  }
}

template <typename T>
void statement::parameter::setValue(T value) {
  _stmt.get().assertPrepared();
  _stmt.get()._datasource_statement->setParam(_name, value);
}

void statement::parameter::setNull() {
  _stmt.get().assertPrepared();
  _stmt.get()._datasource_statement->setParamToNull(_name);
}

// statement::parameter_access

statement::parameter_access::parameter_access(statement& stmt) : _stmt(stmt) {
}

statement::parameter& statement::parameter_access::operator[](const std::string_view name) {
  return *(_current_parameter = statement::parameter(_stmt.get(), name));
}

// statement

statement::statement(connection& conn) : _conn(conn) {
}

void statement::prepare(const std::string_view script) {
  _datasource_statement = _conn.get()._datasource_connection->prepareStatement(script);
}

bool statement::prepared() {
  return !!_datasource_statement;
}

void statement::assertPrepared() {
  if (!prepared()) {
    // throw new std::runtime_error("statement not yet prepared");
  }
}

resultset statement::execute() {
  assertPrepared();

  return resultset(*this);
}

int statement::executeUpdate() {
  assertPrepared();

  return _datasource_statement->executeUpdate();
}

template <typename T>
std::optional<T> statement::executeAndGetSingle() {
  for (auto& rslt : execute()) {
    for (const auto& col : rslt.columns()) {
      return rslt.get<T>(col);
    }
  }

  return {};
}
} // namespace db
