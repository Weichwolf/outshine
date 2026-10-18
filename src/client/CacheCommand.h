#ifndef OUTSHINE_CLIENT_CACHECOMMAND_H
#define OUTSHINE_CLIENT_CACHECOMMAND_H

#include <span>

namespace outshine::Client {
[[nodiscard]] int RunCacheCommand(std::span<const char *const> arguments);
}
#endif
