#include "src/app/ApplicationRoots.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <string>

int main() {
  using namespace outshine::Test;
  const auto first = outshine::App::WithCacheDirectories({});
  const auto second = outshine::App::WithCacheDirectories({});
  CHECK(first && second && !first->SourceCacheDirectory.empty() &&
            first->SourceCacheDirectory == second->SourceCacheDirectory,
        "separate client roots select the same persistent source cache");
  CHECK(first && first->AssetCacheDirectory == std::string(SDL_GetBasePath()) + "assets",
        "default asset cache is beside the application executable");
  const auto explicitRoots =
      outshine::App::WithCacheDirectories({.BuiltinDataDirectory = "shipped",
                                           .SourceCacheDirectory = "/tmp/explicit-cache",
                                           .Offline = true,
                                           .AssetCacheDirectory = {}});
  CHECK(explicitRoots && explicitRoots->BuiltinDataDirectory == "shipped" &&
            explicitRoots->SourceCacheDirectory == "/tmp/explicit-cache" && explicitRoots->Offline,
        "explicit content roots and offline policy are preserved");
  return Report();
}
