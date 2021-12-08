#pragma once

#include "db.hpp"
#include <iostream>

namespace db::mock {
class datasource : public db::datasource {
public:
  class resultset : public db::datasource::resultset {
  public:
    resultset() {
      std::cout << "mock::datasource::resultset::resultset()" << std::endl;
    }

    virtual ~resultset() override {
      std::cout << "mock::datasource::resultset::~resultset()" << std::endl;
    }

    virtual bool next() override {
      std::cout << "mock::datasource::resultset::next()" << std::endl;
      return (++_i) <= 5;
    }

    virtual bool isValueNull(const std::string_view name) override {
      std::cout << "mock::datasource::resultset::isValueNull(\"" << name << "\")" << std::endl;
      return (_i % 2) == 0;
    }

    virtual void getValue(const std::string_view name, bool& result) override {
      std::cout << "mock::datasource::resultset::getValue(\"" << name << "\", " << result << ")" << std::endl;
      result = _i == 0;
    }

    virtual void getValue(const std::string_view name, int& result) override {
      std::cout << "mock::datasource::resultset::getValue(\"" << name << "\", " << result << ")" << std::endl;
      result = _i;
    }

    virtual void getValue(const std::string_view name, double& result) override {
      std::cout << "mock::datasource::resultset::getValue(\"" << name << "\", " << result << ")" << std::endl;
      result = 1.5 * _i;
    }

    virtual void getValue(const std::string_view name, std::string& result) override {
      std::cout << "mock::datasource::resultset::getValue(\"" << name << "\", \"" << result << "\")" << std::endl;
      result = std::to_string(_i);
    }

    virtual int columnCount() override {
      std::cout << "mock::datasource::resultset::columnCount()" << std::endl;
      return 3;
    }

    virtual std::string columnName(int i) override {
      std::cout << "mock::datasource::resultset::columnName(" << i << ")" << std::endl;
      return "mockcol" + std::to_string(i);
    }

  private:
    int _i = 0;
  };

  class statement : public db::datasource::statement {
  public:
    statement() {
      std::cout << "mock::datasource::statement::statement()" << std::endl;
    }

    virtual ~statement() override {
      std::cout << "mock::datasource::statement::~statement()" << std::endl;
    }

    virtual std::shared_ptr<db::datasource::resultset> execute() override {
      std::cout << "mock::datasource::statement::execute()" << std::endl;
      return std::make_shared<resultset>();
    }

    virtual int executeUpdate() override {
      std::cout << "mock::datasource::statement::executeUpdate()" << std::endl;
      return 1;
    }

    virtual void setParamToNull(const std::string_view name) override {
      std::cout << "mock::datasource::statement::setParamToNull(\"" << name << "\")" << std::endl;
    }

    virtual void setParam(const std::string_view name, bool value) override {
      std::cout << "mock::datasource::statement::setParam(\"" << name << "\", " << value << ")" << std::endl;
    }

    virtual void setParam(const std::string_view name, int value) override {
      std::cout << "mock::datasource::statement::setParam(\"" << name << "\", " << value << ")" << std::endl;
    }

    virtual void setParam(const std::string_view name, double value) override {
      std::cout << "mock::datasource::statement::setParam(\"" << name << "\", " << value << ")" << std::endl;
    }

    virtual void setParam(const std::string_view name, const std::string_view value) override {
      std::cout << "mock::datasource::statement::setParam(\"" << name << "\", \"" << value << "\")" << std::endl;
    }
  };

  class connection : public db::datasource::connection {
  public:
    connection() {
      std::cout << "mock::datasource::connection::connection()" << std::endl;
    }

    virtual ~connection() override {
      std::cout << "mock::datasource::connection::~connection()" << std::endl;
    }

    virtual std::shared_ptr<db::datasource::statement> prepareStatement(const std::string_view script) override {
      std::cout << "mock::datasource::connection::prepareStatement(\"" << script << "\")" << std::endl;
      return std::make_shared<statement>();
    }

    virtual void beginTransaction() override {
      std::cout << "mock::datasource::connection::beginTransaction()" << std::endl;
    }

    virtual void commit() override {
      std::cout << "mock::datasource::connection::commit()" << std::endl;
    }

    virtual void rollback() override {
      std::cout << "mock::datasource::connection::rollback()" << std::endl;
    }

    virtual void execute(const std::string_view script) override {
      std::cout << "mock::datasource::connection::execute(\"" << script << "\")" << std::endl;
    }
  };

  datasource() {
    std::cout << "mock::datasource::datasource()" << std::endl;
  }

  virtual std::shared_ptr<db::datasource::connection> getConnection() override {
    std::cout << "mock::datasource::getConnection()" << std::endl;
    return std::make_shared<connection>();
  }
};
} // namespace db::mock
