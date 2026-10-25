#include "HockenheimCircuitFixture.h"
#include "HockenheimSceneFixture.h"
#include "EngineFactory.h"
#include <Engine.h>
#include <SDL3/SDL.h>

#include "Check.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <filesystem>
#include <print>
#include <string>

namespace Circuit = outshine::Test::HockenheimFixture;

namespace {
constexpr double kMinimumLapLengthM = 4000.0;
constexpr double kMaximumLapLengthM = 5000.0;
constexpr double kPoseTolerance = 1e-6;
constexpr double kBasisTolerance = 1e-9;
constexpr double kMaximumCenterOffsetM = 0.2;

void ShowPending(const outshine::Engine &engine) {
  using namespace outshine;
  const auto loading = engine.loading();
  std::println("route: {}; world: {}; ground {}/{}; vector {}/{}; outstanding {}",
               "alignment unavailable",
               engine.unsettledReasons(WorldQuality::Playable),
               loading.GroundArrived,
               loading.GroundWanted,
               loading.VectorArrived,
               loading.VectorWanted,
               loading.Outstanding);
  for (const auto &metric : engine.measures()) {
    if (metric.Name.contains("alignment") || metric.Name.contains("semantic OSM") ||
        metric.Name.contains("route")) {
      std::println("{}={} {}", metric.Name, metric.Value, metric.Unit);
    }
  }
}
}

namespace {
using namespace outshine;
using namespace outshine::Test;

void CheckContacts(const Engine &engine, const RouteInfo &info) {
  size_t missingContact = 0;
  double greatestCenterOffsetM = 0.0;
  for (size_t sample = 0; sample <= 512; ++sample) {
    const double stationM = info.LengthM * static_cast<double>(sample) / 512.0;
    const auto pose = engine.sampleRoute("circuit", stationM);
    if (!pose) {
      ++missingContact;
      continue;
    }
    for (const double fraction : {-0.45, 0.0, 0.45}) {
      const auto contact = engine.sampleRouteContact("circuit", stationM, pose->WidthM * fraction);
      if (!contact || std::abs(Length(contact->Normal) - 1.0) > kPoseTolerance ||
          contact->Normal[1] <= 0.0) {
        ++missingContact;
        continue;
      }
      if (fraction == 0.0) {
        greatestCenterOffsetM =
            std::max(greatestCenterOffsetM, std::abs(contact->PositionM[1] - pose->PositionM[1]));
      }
    }
  }
  CHECK(missingContact == 0 && greatestCenterOffsetM < kMaximumCenterOffsetM,
        "public route contact reads published native road triangles across the circuit");
}

void CheckGeometry(const Engine &engine, const RouteInfo &info) {
  const auto first = engine.sampleRoute("circuit", 0.0);
  const auto last = engine.sampleRoute("circuit", info.LengthM);
  CHECK(first && last && first->SegmentIndex == 0 && last->SegmentIndex == 0 &&
            Length(first->PositionM - last->PositionM) < kPoseTolerance &&
            Length(first->Forward - last->Forward) < kPoseTolerance,
        "the public route closes with one local camera position and direction");
  size_t previousSegment = 0;
  bool validSamples = true;
  for (size_t sample = 0; sample < 512; ++sample) {
    const double stationM = info.LengthM * static_cast<double>(sample) / 512.0;
    const auto pose = engine.sampleRoute("circuit", stationM);
    validSamples &= pose && pose->SegmentIndex >= previousSegment &&
                    pose->SegmentIndex < info.SegmentCount && pose->WidthM > 0.0 &&
                    std::abs(Length(pose->Forward) - 1.0) < kBasisTolerance &&
                    std::abs(Length(pose->Up) - 1.0) < kBasisTolerance &&
                    std::abs(Dot(pose->Forward, pose->Up)) < kBasisTolerance;
    if (pose) { previousSegment = pose->SegmentIndex; }
  }
  CHECK(validSamples, "sampled public stations advance on a finite orthonormal route basis");
  CheckContacts(engine, info);
  CHECK(!engine.sampleRoute("circuit", -1.0) &&
            !engine.sampleRoute("circuit", std::numeric_limits<double>::quiet_NaN()) &&
            !engine.sampleRoute("circuit", info.LengthM + 1.0),
        "invalid stations are rejected before any camera consumes them");
  CHECK(!engine.sampleRouteContact("circuit", -1.0, 0.0) &&
            !engine.sampleRouteContact("circuit", 0.0, std::numeric_limits<double>::quiet_NaN()) &&
            !engine.sampleRouteContact("circuit", 0.0, 100.0),
        "invalid contact coordinates are refused without terrain fallback");
}

void CheckRevisions(Engine &engine, Scenario::Document &scene) {
  const auto refined = engine.preload(0.0, WorldQuality::Refined);
  CHECK(!refined && !engine.settled(WorldQuality::Refined),
        "an incomplete vector fixture cannot claim refined coverage");
  CHECK(engine.declare(scene) && engine.assemble() && engine.routeInfo("circuit") &&
            engine.sampleRouteContact("circuit", 0.0, 0.0),
        "unchanged source and route preserve the published alignment across reuse");
  auto invalid = scene;
  invalid.Routes.front().Id.clear();
  CHECK(!engine.declare(invalid) && engine.routeInfo("circuit") &&
            engine.sampleRouteContact("circuit", 0.0, 0.0),
        "a rejected declaration preserves the valid published route and contact");
  scene.Providers.front().Revision = "pin-r2";
  const auto changedSource = engine.declare(scene);
  CHECK(changedSource,
        changedSource ? "new source declaration succeeds" : changedSource.error().c_str());
  if (changedSource) {
    CHECK(!engine.routeInfo("circuit") && !engine.sampleRoute("circuit", 0.0) &&
              !engine.sampleRouteContact("circuit", 0.0, 0.0),
          "a declared source change revokes old metadata, pose and native contact");
    CHECK(engine.assemble() && !engine.routeInfo("circuit") &&
              !engine.sampleRouteContact("circuit", 0.0, 0.0),
          "a new source revision cannot expose the previously published alignment");
    const auto replacement = Circuit::WaitForRoute(engine);
    if (!engine.routeInfo("circuit")) { ShowPending(engine); }
    CHECK(replacement && engine.routeInfo("circuit") && engine.sampleRoute("circuit", 0.0) &&
              engine.sampleRouteContact("circuit", 0.0, 0.0),
          "the replacement source publishes a usable route through ordinary advancement");
    ++scene.Routes.front().OsmRelationId;
    CHECK(engine.declare(scene) && !engine.routeInfo("circuit") &&
              !engine.sampleRoute("circuit", 0.0) &&
              !engine.sampleRouteContact("circuit", 0.0, 0.0),
          "a changed relation under the same route name cannot reuse old source geometry");
  }
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the offscreen route scene");
  {
    auto engine = App::CreateEngine();
    auto scene = Circuit::AnalyticScene();
    scene.Time = {.Declared = true, .Live = false, .Start = "2026-09-07T10:40:00Z"};

    CHECK(engine.setRoots({.Assets = {},
                           .Shipped = "src/assets",
                           .Cache = {},
                           .Offline = true,
                           .AssetCache = {}}) &&
              engine.setRenderTarget(Circuit::Frame) && engine.declare(scene) && engine.assemble(),
          "the pinned route and analytic DEM enter the public engine");
    const auto unknown = engine.routeInfo("missing");
    const auto pending = engine.routeInfo("circuit");
    const auto pendingContact = engine.sampleRouteContact("circuit", 0.0, 0.0);
    CHECK(!unknown && unknown.error().contains("unknown route") && !pending &&
              pending.error().contains("no current published alignment") && !pendingContact,
          "unknown and unpublished routes are distinct refusals");
    const auto info = Circuit::WaitForRoute(engine);
    if (!info) { ShowPending(engine); }
    CHECK(info && info->Closed && info->SegmentCount == Circuit::EdgeCount &&
              info->LengthM > kMinimumLapLengthM && info->LengthM < kMaximumLapLengthM,
          info ? "published circuit has its logical 267-edge closed centerline"
               : "published circuit is missing");
    if (info) {
      CheckGeometry(engine, *info);
      CheckRevisions(engine, scene);
    }
  }
  SDL_Quit();
  return Report();
}
