#ifndef OUTSHINE_CLIENT_RENDERASSET_H
#define OUTSHINE_CLIENT_RENDERASSET_H

#include <span>

namespace outshine::Client {
[[nodiscard]] int RenderAsset(std::span<char *const> arguments);
}

#endif
