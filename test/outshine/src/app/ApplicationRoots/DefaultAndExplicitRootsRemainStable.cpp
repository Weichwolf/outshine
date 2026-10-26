#include "src/app/ApplicationRoots.h"
#include "Check.h"

int main() {
  using namespace outshine::Test;
  const auto first = outshine::App::WithCacheDirectories({});
  const auto second = outshine::App::WithCacheDirectories({});
  CHECK(first && second && !first->Cache.empty() && first->Cache == second->Cache,
        "separate client roots select the same persistent source cache");
  const auto explicitRoots = outshine::App::WithCacheDirectories(
      {.Shipped = "shipped", .Cache = "/tmp/explicit-cache", .Offline = true, .AssetCache = {}});
  CHECK(explicitRoots && explicitRoots->Shipped == "shipped" &&
            explicitRoots->Cache == "/tmp/explicit-cache" && explicitRoots->Offline,
        "explicit content roots and offline policy are preserved");
  return Report();
}
