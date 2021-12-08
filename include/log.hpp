#pragma once

#include "rxcpp/rx.hpp"
#include <chrono>
#include <experimental/source_location>
#include <string>
#include <vector>

namespace log {
class entry {
public:
  std::chrono::time_point<std::chrono::system_clock> time_point;
  std::experimental::source_location source_location;
  std::string message;
};

std::vector<entry> entries;
rxcpp::subjects::subject<entry> entryAdded;
} // namespace log
