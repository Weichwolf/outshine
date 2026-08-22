#include "GroundStack.h"

#include "Sink.h"

namespace outshine::World {

bool GroundStack::Open(const std::string &cacheDir, const std::string &assetsDir,
                       double focusLat, double focusLon, Data::Transport &wire,
                       Sink &say) {
  Close();
  outshine::Data::ContentStore::Config keeping;
  keeping.Directory = cacheDir;
  Store_ = std::make_unique<outshine::Data::ContentStore>(keeping);
  Sources_ = std::make_unique<outshine::Data::SourceSet>(*Store_);
  outshine::Data::SourceSet &sources = *Sources_;
  say.Claim(outshine::Data::RegisterDeclared(sources, {assetsDir + "/sky", true}) ==
            outshine::Data::Registered::Complete,
        "the declared upstream sources register, ranked and without a clash");
  say.Number("sources registered", (double)sources.Count(), "sources");

  outshine::World::GroundSurface surface;
  surface.Z = 12;
  surface.Grid = 64;
  Pool_ = std::make_unique<outshine::World::TilePool>(
      outshine::World::GroundPoolConfig(focusLat, focusLon), sources, wire);
  Ground_ = std::make_unique<outshine::World::GroundStream>(*Pool_, surface);
  Opened_ = true;
  return true;
}

void GroundStack::Close(void) {
  if (!Opened_) { return; }
  Ground_.reset();
  Pool_.reset();
  Sources_.reset();
  Store_.reset();
  Opened_ = false;
}

}
