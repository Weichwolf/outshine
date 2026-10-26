#ifndef OUTSHINE_APP_APPLICATIONROOTS_H
#define OUTSHINE_APP_APPLICATIONROOTS_H

#include <Engine.h>
#include <SDL3/SDL.h>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::App {

[[nodiscard]] inline Roots MakeApplicationRoots(std::string_view cacheDirectory = {},
                                                bool offline = false) {
  return {.Shipped = "data",
          .Cache = std::string(cacheDirectory),
          .Offline = offline,
          .AssetCache = {}};
}

[[nodiscard]] inline std::expected<Roots, std::string> WithCacheDirectories(Roots roots) {
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
