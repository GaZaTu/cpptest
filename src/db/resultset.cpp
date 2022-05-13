#include "db/resultset.hpp"

namespace db {
resultset::iterator::iterator(resultset& rslt, bool done) : _rslt(rslt), _done(done) {
  if (!_done) {
    ++(*this);
  }
}

resultset::iterator& resultset::iterator::operator++() {
  _done = !_rslt.next();
  return *this;
}

resultset::iterator resultset::iterator::operator++(int) {
  iterator result = *this;
  ++(*this);
  return result;
}

bool resultset::iterator::operator==(const iterator& other) const {
  return _done == other._done;
}

bool resultset::iterator::operator!=(const iterator& other) const {
  return !(*this == other);
}

resultset& resultset::iterator::operator*() {
  return _rslt;
}

resultset::columns_iterable::iterator::iterator(const resultset& rslt, int i, bool done) : _rslt(rslt), _i(i), _done(done) {
  if (!_done) {
    ++(*this);
  }
}

resultset::columns_iterable::iterator& resultset::columns_iterable::iterator::operator++() {
  _done = ((_i += 1) > _rslt._datasource_resultset->columnCount());
  if (!_done) {
    _column_name = _rslt._datasource_resultset->columnName(_i - 1);
  }

  return *this;
}

resultset::columns_iterable::iterator resultset::columns_iterable::iterator::operator++(int) {
  iterator result = *this;
  ++(*this);
  return result;
}

bool resultset::columns_iterable::iterator::operator==(const iterator& other) const {
  return _done == other._done;
}

bool resultset::columns_iterable::iterator::operator!=(const iterator& other) const {
  return !(*this == other);
}

std::string_view resultset::columns_iterable::iterator::operator*() const {
  return _column_name;
}

resultset::columns_iterable::columns_iterable(const resultset& rslt) : _rslt(rslt) {
}

resultset::columns_iterable::iterator resultset::columns_iterable::begin() const {
  return iterator{_rslt, 0, false};
}

resultset::columns_iterable::iterator resultset::columns_iterable::end() const {
  return iterator{_rslt, -1, true};
}

resultset::polymorphic_field::polymorphic_field(const resultset& rslt, std::string_view name) : _rslt(rslt), _name(name) {
}

orm::field_type resultset::polymorphic_field::type() const {
  return _rslt.type(_name);
}

bool resultset::polymorphic_field::isNull() const {
  return _rslt.isNull(_name);
}

resultset::resultset(statement& stmt) {
  _datasource_resultset = stmt._datasource_statement->execute();
}

bool resultset::next() {
  return _datasource_resultset->next();
}

orm::field_type resultset::type(std::string_view name) const {
  return _datasource_resultset->getValueType((std::string)name);
}

bool resultset::isNull(std::string_view name) const {
  return _datasource_resultset->isValueNull((std::string)name);
}

resultset::polymorphic_field resultset::operator[](std::string_view name) const {
  return polymorphic_field{*this, name};
}

resultset::iterator resultset::begin() {
  return iterator{*this, false};
}

resultset::iterator resultset::end() {
  return iterator{*this, true};
}

resultset::columns_iterable resultset::columns() const {
  return columns_iterable{*this};
}
} // namespace db
