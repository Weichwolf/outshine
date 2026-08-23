#ifndef OUTSHINE_GROUND_GROUNDSTACK_H
#define OUTSHINE_GROUND_GROUNDSTACK_H

#include <memory>
#include <span>
#include <string_view>

#include "ContentStore.h"
#include "DeclaredSources.h"
#include "TerrainLoader.h"
#include "TilePool.h"

namespace outshine {
class Sink;
}

namespace outshine::Ground {

class GroundStack {
public:
  GroundStack() = default;
  ~GroundStack() { Close(); }
  GroundStack(const GroundStack &) = delete;
  GroundStack &operator=(const GroundStack &) = delete;

  [[nodiscard]] bool Open(std::string_view cacheDir, std::string_view assetsDir,
                          std::span<const Provider> providers, double focusLat,
                          double focusLon, Data::Transport &wire, Sink &say);
  void Close();

  [[nodiscard]] bool Opened() const { return Opened_; }
  [[nodiscard]] TilePool &Pool() const { return *Pool_; }
  [[nodiscard]] GroundStream &Ground() const { return *Ground_; }

private:
  std::unique_ptr<Data::ContentStore> Store_;
  std::unique_ptr<Data::SourceSet> Sources_;
  std::unique_ptr<TilePool> Pool_;
  std::unique_ptr<GroundStream> Ground_;
  bool Opened_ = false;
};

}

#endif
