#include "EngineHeld.h"
#include "math/Units.h"

#include <cmath>
#include <string>

namespace outshine {

namespace {
constexpr int kZoomMost = 24;
}

bool Engine::State::RequestTerrainCoverage() {
  const Scenario::Document &declared = Session.Declared;
  if (!declared.Ground.Declared) { return true; }
  if (!Picture.Standing || !World.Stack.Opened()) { return true; }
  Around over;
  const LongitudeLatitude focus = WhereTheEyeStands();
  over.LatitudeDeg = focus.LatitudeDeg;
  over.LongitudeDeg = focus.LongitudeDeg;
  over.Zoom = World.Stack.FinestZoomOf(Data::DataKind::Elevation);
  over.Asking = true;
  over.PlayableOnly = !World.GroundPublished.Current();
  {
    const double tileSpanM =
        40075017.0 * std::cos(over.LatitudeDeg * kDeg2Rad) / std::ldexp(1.0, over.Zoom);
    const double nearest = 4.0 * tileSpanM;
    const double wanted = declared.Ground.SightM > 0.0 ? declared.Ground.SightM : 240000.0;
    over.Levels =
        1 + static_cast<int>(std::ceil(wanted > nearest ? std::log2(wanted / nearest) : 0.0));
  }
  World.Stack.Pool().Focus({.LongitudeDeg = over.LongitudeDeg, .LatitudeDeg = over.LatitudeDeg});
  auto asked = World.Shipping.Covering().Lay(World.Stack.Pool(), over);
  if (!asked) {
    Error = asked.error();
    return false;
  }
  World.Pending = asked->Pending;
  World.Wanted = asked->Tiles;
  World.AskedPending = asked->Pending;
  World.AskedWanted = asked->Tiles;
  if (over.PlayableOnly) { World.AskedPlayablePending = asked->Pending; }
  {
    const Ground::TilePool::Ledger kept = World.Stack.Pool().Counters();
    Published.Places("mesh jobs the pool finished", static_cast<double>(kept.MeshTiles), "tiles");
    Published.Places("mesh jobs it refused", static_cast<double>(kept.MeshRefused), "tiles");
    Published.Places("field jobs the pool finished", static_cast<double>(kept.FieldTiles), "tiles");
    Published.Places(
        "field jobs it dropped and will retry", static_cast<double>(kept.FieldDropped), "jobs");
    Published.Places("field jobs' worker time", kept.FieldCpuMs, "ms");
    Published.Places("mesh jobs' worker time", kept.MeshCpuMs, "ms");
    Published.Places(
        "mesh jobs with no tile behind them", static_cast<double>(kept.MeshAbsent), "tiles");
    Published.Places("fetches it ran", static_cast<double>(kept.Fetches), "fetches");
    Published.Places("fetches it gave up on", static_cast<double>(kept.FetchGaveUp), "fetches");
    Published.Places("fetches it refused", static_cast<double>(kept.FetchRefused), "fetches");
    Published.Places("jobs it posted", static_cast<double>(kept.Posts), "jobs");
    Published.Places(
        "jobs deferred by bounded admission", static_cast<double>(kept.AdmissionDeferred), "jobs");
    Published.Places("asks that repeated a posted job", static_cast<double>(kept.Repeats), "asks");
    Published.Places("megabytes it fetched", kept.FetchedMB, "MB");
    Published.Places("jobs still outstanding", static_cast<double>(kept.Outstanding), "jobs");
    Published.Places("keys with jobs parked behind them", static_cast<double>(kept.Parked), "keys");
    Published.Places("jobs parked in all", static_cast<double>(kept.ParkedJobs), "jobs");
    Published.Places("results it holds", static_cast<double>(kept.Held), "results");
    Published.Places(
        "mesh jobs it dropped and will retry", static_cast<double>(kept.MeshDropped), "jobs");
    Published.Places("jobs waiting in the queue", static_cast<double>(kept.QueueDepth), "jobs");
  }
  for (int zoom = 0; zoom < kZoomMost; ++zoom) {
    if (asked->WantedAtZoom[zoom] == 0) { continue; }
    Published.Places("zoom " + std::to_string(zoom) + " wants " +
                         std::to_string(asked->WantedAtZoom[zoom]) + " and still waits for",
                     static_cast<double>(asked->PendingAtZoom[zoom]),
                     "tiles");
  }
  return true;
}
}
