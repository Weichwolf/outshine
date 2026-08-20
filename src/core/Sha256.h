#ifndef SHA256_H
#define SHA256_H

#include <cstddef>
#include <string>

namespace outshine {

[[nodiscard]] std::string Sha256Hex(const void *data, size_t bytes);
[[nodiscard]] std::string Sha256Hex(const std::string &text);

}
#endif
