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
  return {.BuiltinDataDirectory = "data",
          .SourceCacheDirectory = std::string(cacheDirectory),
          .Offline = offline,
          .AssetCacheDirectory = {}};
}

[[nodiscard]] inline std::expected<Roots, std::string> WithCacheDirectories(Roots roots) {
  if (roots.SourceCacheDirectory.empty()) {
    const std::unique_ptr<char, decltype(&SDL_free)> path(SDL_GetPrefPath("outshine", "outshine"),
                                                          SDL_free);
    if (!path) {
      return std::unexpected("could not locate the source cache: " + std::string(SDL_GetError()));
    }
    roots.SourceCacheDirectory = std::string(path.get()) + "sources";
    if (roots.AssetCacheDirectory.empty()) {
      roots.AssetCacheDirectory = std::string(path.get()) + "assets";
    }
  }
  if (roots.AssetCacheDirectory.empty()) {
    roots.AssetCacheDirectory = roots.SourceCacheDirectory + ".assets";
  }
  return roots;
}

}
#endif
