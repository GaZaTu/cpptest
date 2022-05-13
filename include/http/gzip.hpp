#pragma once

#ifdef HTTPPP_ZLIB
#include <string>

namespace http {
namespace gzip {
int compress(std::string& _body);

int uncompress(std::string& _body);
}
} // namespace http
#endif
