#ifndef OUTSHINE_CLIENT_SOURCECACHE_H
#define OUTSHINE_CLIENT_SOURCECACHE_H

#include <Outshine.h>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Client {

[[nodiscard]] inline Roots ClientRoots(std::string_view cacheDirectory = {}, bool offline = false) {
  return {.Assets = "src/assets/drive",
          .Shipped = "src/assets",
          .Cache = std::string(cacheDirectory),
          .Offline = offline,
          .AssetCache = {}};
}

[[nodiscard]] inline std::expected<Roots, std::string> WithSourceCache(Roots roots) {
  if (roots.Cache.empty()) {
    const std::unique_ptr<char, decltype(&SDL_free)> path(SDL_GetPrefPath("outshine", "outshine"),
                                                          SDL_free);
    if (!path) {
      return std::unexpected("could not locate the source cache: " + std::string(SDL_GetError()));
    }
    roots.Cache = std::string(path.get()) + "sources";
    if (roots.AssetCache.empty()) { roots.AssetCache = std::string(path.get()) + "assets"; }
  }
  if (roots.AssetCache.empty()) { roots.AssetCache = roots.Cache + ".assets"; }
  return roots;
}

}
#endif
