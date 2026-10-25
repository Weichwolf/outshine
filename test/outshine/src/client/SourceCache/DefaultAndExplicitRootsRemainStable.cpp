#include "src/client/SourceCache.h"
#include "Check.h"

int main() {
  using namespace outshine::Test;
  const auto first = outshine::Client::WithSourceCache({});
  const auto second = outshine::Client::WithSourceCache({});
  CHECK(first && second && !first->Cache.empty() && first->Cache == second->Cache,
        "separate client roots select the same persistent source cache");
  const auto explicitRoots = outshine::Client::WithSourceCache({.Assets = "assets",
                                                                .Shipped = "shipped",
                                                                .Cache = "/tmp/explicit-cache",
                                                                .Offline = true,
                                                                .AssetCache = {}});
  CHECK(explicitRoots && explicitRoots->Assets == "assets" && explicitRoots->Shipped == "shipped" &&
            explicitRoots->Cache == "/tmp/explicit-cache" && explicitRoots->Offline,
        "explicit content roots and offline policy are preserved");
  return Report();
}
