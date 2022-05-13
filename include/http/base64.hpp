#pragma once

#include <string>
#include <vector>

namespace base64 {
std::string encode(const std::vector<uint8_t>& buffer);
std::vector<uint8_t> decode(const std::string& string);
}
