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
  say.Number("the focus latitude the stack was asked for", focusLat, "deg");
  say.Number("the furthest the tiling reaches", kMercatorLatMaxDeg, "deg");
  if (!onTheBand) {
    say.Say("REFUSED the focus is off the tiling band");
    return false;
  }
  outshine::Data::ContentStore::Config keeping;
  keeping.Directory = std::string(cacheDir);
  Store_ = std::make_unique<outshine::Data::ContentStore>(keeping);
  Sources_ = std::make_unique<outshine::Data::SourceSet>(*Store_);
  outshine::Data::SourceSet &sources = *Sources_;
  std::string refused;
  const bool registered = outshine::Data::RegisterDeclared(
      sources, providers, std::string(assetsDir) + "/sky", refused);
  if (!registered) {
    say.Say(Line("REFUSED %s", refused.c_str()));
    Close();
    return false;
  }
  say.Number("sources registered", (double)sources.Count(), "sources");

  outshine::Ground::GroundSurface surface;
  surface.Z = 12;
  surface.Grid = 64;
  Pool_ = std::make_unique<outshine::Ground::TilePool>(
      outshine::Ground::GroundPoolConfig(focusLat, focusLon), sources, wire);
  Ground_ = std::make_unique<outshine::Ground::GroundStream>(*Pool_, surface);
  Opened_ = true;
  return true;
}

void GroundStack::Close() {
  Ground_.reset();
  Pool_.reset();
  Sources_.reset();
  Store_.reset();
  Opened_ = false;
}

int GroundStack::FinestZoomOf(Data::DataKind kind) const {
  int finest = 0;
  if (!Sources_) { return finest; }
  for (size_t at = 0; at < Sources_->Count(); ++at) {
    const Data::SourceDecl &decl = Sources_->At(at).Declaration();
    if (decl.Kind != kind) { continue; }
    finest = decl.MaxZoom > finest ? decl.MaxZoom : finest;
  }
  return finest;
}

}
