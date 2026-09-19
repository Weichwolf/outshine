#ifndef OUTSHINE_WORLD_WORLDSTORAGE_H
#define OUTSHINE_WORLD_WORLDSTORAGE_H

#include <string>

namespace outshine::World {

/// Owned filesystem roots required by streamed world data.
struct StoragePaths {
  std::string Shipped; ///< Base directory for built-in world tables and provider data.
  std::string Cache;   ///< Persistent provider cache directory; empty disables disk storage.
};

}

#endif
