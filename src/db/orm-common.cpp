#include "db/orm-common.hpp"

namespace db::orm {
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

std::ostream& operator<<(std::ostream& os, join_mode op) {
  switch (op) {
  case JOIN_INNER:
    os << "JOIN";
    break;
  case JOIN_LEFT:
    os << "LEFT JOIN";
    break;
  case JOIN_CROSS:
    os << "CROSS JOIN";
    break;
  }

  return os;
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

condition_container::~condition_container() {}

void condition_container::appendToQuery(std::ostream& os) const {
  int i = 666;
  appendToQuery(os, i);
}

void condition_container::assignToParams(db::statement& statement) const {
  int i = 666;
  assignToParams(statement, i);
}

selection::selection(const char* name, const char* alias) {
  this->name = name;
  this->alias = alias;
}

selection::selection(const char* name) {
  this->name = name;
}

selection::selection(const std::string& name) {
  this->name = name;
}

selection::selection(std::string_view name) {
  this->name = name;
}

selection::selection(std::string&& name) {
  this->name = std::move(name);
}

selection::operator std::string() const {
  bool quote = std::isalpha(name[0]);
  for (auto c : name) {
    if (!std::isalnum(c) && c != '_') {
      quote = false;
      break;
    }
  }

  std::string result;
  if (quote) {
    result += '"';
  }
  result += name;
  if (quote) {
    result += '"';
  }
  if (!alias.empty()) {
    result += " AS ";
    result += '"';
    result += alias;
    result += '"';
  }
  return result;
}

selection::operator bool() const {
  return name.length() > 0;
}
} // namespace db::orm
