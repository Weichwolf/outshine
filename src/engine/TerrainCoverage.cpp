#include "EngineHeld.h"
#include "math/Units.h"

#include <cmath>
#include <string>

namespace outshine {

namespace {
constexpr int kZoomMost = 24;
constexpr double kDefaultSightM = 240000.0;
}

double Engine::State::TerrainSightM() const noexcept {
  return Session.Declared.Ground.SightM > 0.0 ? Session.Declared.Ground.SightM : kDefaultSightM;
}

Around Engine::State::TerrainCoverageAt(LongitudeLatitude focus) const {
  Around over;
  over.LatitudeDeg = focus.LatitudeDeg;
  over.LongitudeDeg = focus.LongitudeDeg;
  over.Zoom = World.Stack.FinestZoomOf(Data::DataKind::Elevation);
  {
    const double tileSpanM =
        40075017.0 * std::cos(over.LatitudeDeg * kDeg2Rad) / std::ldexp(1.0, over.Zoom);
    const double nearest = 4.0 * tileSpanM;
    const double wanted = TerrainSightM();
    over.Levels =
        1 + static_cast<int>(std::ceil(wanted > nearest ? std::log2(wanted / nearest) : 0.0));
  }
  return over;
}

bool Engine::State::RequestTerrainCoverage() {
  const Scenario::Document &declared = Session.Declared;
  if (!declared.Ground.Declared) { return true; }
  if (!Picture.Standing || !World.Stack.Opened()) { return true; }
  Around over = TerrainCoverageAt(CurrentGeographicFocus());
  over.Asking = true;
  over.PlayableOnly = !World.GroundPublished.Current();
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
    Published.RecordMetric(
        "mesh jobs the pool finished", static_cast<double>(kept.MeshTiles), "tiles");
    Published.RecordMetric("mesh jobs it refused", static_cast<double>(kept.MeshRefused), "tiles");
    Published.RecordMetric(
        "field jobs the pool finished", static_cast<double>(kept.FieldTiles), "tiles");
    Published.RecordMetric(
        "field jobs it dropped and will retry", static_cast<double>(kept.FieldDropped), "jobs");
    Published.RecordMetric("field jobs' worker time", kept.FieldCpuMs, "ms");
    Published.RecordMetric("mesh jobs' worker time", kept.MeshCpuMs, "ms");
    Published.RecordMetric(
        "mesh jobs with no tile behind them", static_cast<double>(kept.MeshAbsent), "tiles");
    Published.RecordMetric("fetches it ran", static_cast<double>(kept.Fetches), "fetches");
    Published.RecordMetric(
        "fetches it gave up on", static_cast<double>(kept.FetchGaveUp), "fetches");
    Published.RecordMetric("fetches it refused", static_cast<double>(kept.FetchRefused), "fetches");
    Published.RecordMetric("jobs it posted", static_cast<double>(kept.Posts), "jobs");
    Published.RecordMetric(
        "jobs deferred by bounded admission", static_cast<double>(kept.AdmissionDeferred), "jobs");
    Published.RecordMetric(
        "asks that repeated a posted job", static_cast<double>(kept.Repeats), "asks");
    Published.RecordMetric("megabytes it fetched", kept.FetchedMB, "MB");
    Published.RecordMetric("jobs still outstanding", static_cast<double>(kept.Outstanding), "jobs");
    Published.RecordMetric(
        "keys with jobs parked behind them", static_cast<double>(kept.Parked), "keys");
    Published.RecordMetric("jobs parked in all", static_cast<double>(kept.ParkedJobs), "jobs");
    Published.RecordMetric("results it holds", static_cast<double>(kept.Held), "results");
    Published.RecordMetric(
        "mesh jobs it dropped and will retry", static_cast<double>(kept.MeshDropped), "jobs");
    Published.RecordMetric(
        "jobs waiting in the queue", static_cast<double>(kept.QueueDepth), "jobs");
  }
  for (int zoom = 0; zoom < kZoomMost; ++zoom) {
    if (asked->WantedAtZoom[zoom] == 0) { continue; }
    Published.RecordMetric("zoom " + std::to_string(zoom) + " wants " +
                               std::to_string(asked->WantedAtZoom[zoom]) + " and still waits for",
                           static_cast<double>(asked->PendingAtZoom[zoom]),
                           "tiles");
  }
  return true;
}
}
