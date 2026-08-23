#include "GroundStack.h"

#include <cmath>
#include <string>

#include "Sink.h"
#include "TileGeodesy.h"

namespace outshine::Ground {

bool GroundStack::Open(std::string_view cacheDir, std::string_view assetsDir,
                       std::span<const Provider> providers, double focusLat,
                       double focusLon, Data::Transport &wire, Sink &say) {
  Close();
  const bool onTheBand = std::fabs(focusLat) <= kMercatorLatMaxDeg;
  say.Claim(onTheBand,
        "**THE DECLARED FOCUS LIES ON THE TILING'S MERCATOR BAND** -- beyond 85.05 degrees the "
        "tile pyramid has no rows, so a stack there cannot stand and refuses instead");
  if (!onTheBand) { return false; }
  outshine::Data::ContentStore::Config keeping;
  keeping.Directory = std::string(cacheDir);
  Store_ = std::make_unique<outshine::Data::ContentStore>(keeping);
  Sources_ = std::make_unique<outshine::Data::SourceSet>(*Store_);
  outshine::Data::SourceSet &sources = *Sources_;
  std::string refused;
  const bool registered = outshine::Data::RegisterDeclared(
      sources, providers, std::string(assetsDir) + "/sky", refused);
  if (!registered) { say.Say(Line("REFUSED %s", refused.c_str())); }
  say.Claim(registered, "the declared providers register, ranked and without a clash");
  if (!registered) {
    Close();
    return false;
  }
  say.Number("sources registered", (double)sources.Count(), "sources");

  outshine::Ground::GroundSurface surface;
  surface.Z = 12;   // [SET] the Terrarium pyramid level the drive corridor streams at
  surface.Grid = 64; // [SET] posts per tile edge, the source's own tile granularity
  Pool_ = std::make_unique<outshine::Ground::TilePool>(
      outshine::Ground::GroundPoolConfig(focusLat, focusLon), sources, wire);
  Ground_ = std::make_unique<outshine::Ground::GroundStream>(*Pool_, surface);
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
