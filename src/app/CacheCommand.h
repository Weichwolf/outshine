#ifndef OUTSHINE_APP_CACHECOMMAND_H
#define OUTSHINE_APP_CACHECOMMAND_H

#include <span>

namespace outshine::App {
[[nodiscard]] int RunCacheCommand(std::span<const char *const> arguments);
}
#endif
