#include "db/statement.hpp"

namespace db {
statement::parameter::parameter(statement& stmt, std::string_view name) : _stmt(stmt), _name(name) {
  if (std::isalnum(name[0])) {
    throw sql_error{std::string{"invalid param name: " + (std::string)name}};
  }
}

statement::parameter& statement::parameter::operator=(bool value) {
  setValue(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(int8_t value) {
  setValue(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(uint8_t value) {
  setValue(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(int16_t value) {
  setValue(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(uint16_t value) {
  setValue(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(int32_t value) {
  setValue(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(uint32_t value) {
  setValue(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(int64_t value) {
  setValue(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(uint64_t value) {
  setValue(value);
  return *this;
}

#ifdef __SIZEOF_INT128__
statement::parameter& statement::parameter::operator=(__uint128_t value) {
  setValue(value);
  return *this;
}
#endif

statement::parameter& statement::parameter::operator=(float value) {
  setValue(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(double value) {
  setValue(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(long double value) {
  setValue(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(std::string_view value) {
  setValue(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(std::string&& value) {
  setValue(std::forward<decltype(value)>(value));
  return *this;
}

statement::parameter& statement::parameter::operator=(const char* value) {
  return (*this) = std::string_view{value};
}

statement::parameter& statement::parameter::operator=(const std::vector<uint8_t>& value) {
  setValue(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(orm::date value) {
  setValue(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(orm::time value) {
  setValue(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(orm::timestamp value) {
  setValue(value);
  return *this;
}

statement::parameter& statement::parameter::operator=(std::nullopt_t) {
  setNull();
  return *this;
}

void statement::parameter::setNull() {
  _stmt.get().assertPrepared();
  _stmt.get()._datasource_statement->setParamToNull(_name);
}

statement::parameter_access::parameter_access(statement& stmt) : _stmt(stmt) {
}

statement::parameter& statement::parameter_access::operator[](std::string_view name) {
  return *(_current_parameter = statement::parameter{_stmt, name});
}

statement::statement(connection& conn) : _conn(conn) {
}

statement::statement(statement&& other) : _conn(other._conn) {
  std::swap(_datasource_statement, other._datasource_statement);
}

void statement::prepare(std::string_view script) {
  _datasource_statement = _conn.get()._datasource_connection->prepareStatement(script);
}

bool statement::prepared() {
  return !!_datasource_statement;
}

void statement::assertPrepared() {
  if (!prepared()) {
    // throw new sql_error("statement not yet prepared");
  }
}

int statement::executeUpdate() {
  assertPrepared();

  return _datasource_statement->executeUpdate();
}

bool statement::readonly() {
  return _datasource_statement->readonly();
}
} // namespace db
