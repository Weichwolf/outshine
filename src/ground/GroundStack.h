#ifndef OUTSHINE_WORLD_GROUNDSTACK_H
#define OUTSHINE_WORLD_GROUNDSTACK_H

#include <memory>
#include <string>

#include "ContentStore.h"
#include "DeclaredSources.h"
#include "TerrainLoader.h"
#include "TilePool.h"

namespace outshine {
class Sink;
}

namespace outshine::World {

class GroundStack {
public:
  GroundStack() = default;
  ~GroundStack() { Close(); }
  GroundStack(const GroundStack &) = delete;
  GroundStack &operator=(const GroundStack &) = delete;

  [[nodiscard]] bool Open(const std::string &cacheDir, const std::string &assetsDir,
                          double focusLat, double focusLon, Data::Transport &wire,
                          Sink &say);
  void Close(void);

  [[nodiscard]] bool Opened(void) const { return Opened_; }
  [[nodiscard]] TilePool &Pool(void) const { return *Pool_; }
  [[nodiscard]] GroundStream &Ground(void) const { return *Ground_; }

private:
  std::unique_ptr<Data::ContentStore> Store_;
  std::unique_ptr<Data::SourceSet> Sources_;
  std::unique_ptr<TilePool> Pool_;
  std::unique_ptr<GroundStream> Ground_;
  bool Opened_ = false;
};

}

#endif
