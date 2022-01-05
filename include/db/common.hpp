#pragma once

#include <stdexcept>

namespace db {
class resultset;
class statement;
class transaction;
class connection;

class sql_error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

template <typename T>
struct type_converter {
  static constexpr bool specialized = false;
};

namespace orm {
template <typename T>
class selector;

template <typename T>
class inserter;

template <typename T>
class updater;

template <typename T>
class deleter;
}
} // namespace db
