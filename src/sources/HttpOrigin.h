#ifndef OUTSHINE_SOURCES_HTTPORIGIN_H
#define OUTSHINE_SOURCES_HTTPORIGIN_H

#include <string>

namespace outshine::Data {
[[nodiscard]] std::string HttpOrigin(const std::string &url);
}

#endif
