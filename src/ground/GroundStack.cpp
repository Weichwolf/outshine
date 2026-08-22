#include "GroundStack.h"

#include <string>

#include "Sink.h"

namespace outshine::World {

bool GroundStack::Open(std::string_view cacheDir, std::string_view assetsDir,
                       double focusLat, double focusLon, Data::Transport &wire, Sink &say) {
  Close();
  outshine::Data::ContentStore::Config keeping;
  keeping.Directory = std::string(cacheDir);
  Store_ = std::make_unique<outshine::Data::ContentStore>(keeping);
  Sources_ = std::make_unique<outshine::Data::SourceSet>(*Store_);
  outshine::Data::SourceSet &sources = *Sources_;
  const bool registered =
      outshine::Data::RegisterDeclared(sources, {std::string(assetsDir) + "/sky", true}) ==
      outshine::Data::Registered::Complete;
  say.Claim(registered, "the declared upstream sources register, ranked and without a clash");
  if (!registered) {
    Close();
    return false;
  }
  say.Number("sources registered", (double)sources.Count(), "sources");

  outshine::World::GroundSurface surface;
  surface.Z = 12;   // [SET] the Terrarium pyramid level the drive corridor streams at
  surface.Grid = 64; // [SET] posts per tile edge, the source's own tile granularity
  Pool_ = std::make_unique<outshine::World::TilePool>(
      outshine::World::GroundPoolConfig(focusLat, focusLon), sources, wire);
  Ground_ = std::make_unique<outshine::World::GroundStream>(*Pool_, surface);
  Opened_ = true;
  return true;
}

void GroundStack::Close(void) {
  Ground_.reset();
  Pool_.reset();
  Sources_.reset();
  Store_.reset();
  Opened_ = false;
}

}
