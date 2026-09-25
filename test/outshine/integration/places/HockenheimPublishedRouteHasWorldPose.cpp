#include <Outshine.h>
#include <SDL3/SDL.h>

#include "Check.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the offscreen route scene");
  {
    Engine engine;
    Scenario::Document scene;
    scene.Ground.Declared = true;
    scene.Ground.Origin.LatitudeDeg = 49.3274;
    scene.Ground.Origin.LongitudeDeg = 8.5659;
    scene.Ground.Shape.Kind = "sineGrid";
    scene.Ground.VegetationEnabled = false;
    scene.Ground.SightM = 3000.0;
    scene.Render.Declared = true;
    scene.Render.Frame = {160, 90};
    scene.Lit.Declared = true;
    scene.Providers.push_back(
        {.Kind = "osm",
         .Revision = "pin-r1",
         .Missing = Data::MissingDataPolicy::Fail,
         .Dataset = "openstreetmap",
         .Location = "src/assets/world/osm/HockenheimringGrandPrix.osm",
         .Coverage = Data::SourceCoverage{
             .WestDeg = 8.54, .SouthDeg = 49.315, .EastDeg = 8.61, .NorthDeg = 49.34}});
    scene.Routes.push_back({.Id = "circuit", .OsmRelationId = 284588});
    Scenario::View view;
    view.Id = "overview";
    view.Placement = Scenario::CameraPlacement::Geodetic;
    view.Geographic.Geodetic = {.LongitudeDeg = 8.5659, .LatitudeDeg = 49.3274, .HeightM = 900.0};
    view.Geographic.BearingDeg = 90.0;
    view.Geographic.PitchDeg = -60.0;
    view.Sees.FovDeg = 55.0;
    scene.Views.push_back(view);

    CHECK(engine.setRoots({.Shipped = ".", .Offline = true}) && engine.setRenderTarget({160, 90}) &&
              engine.declare(scene) && engine.assemble(),
          "the pinned route and analytic DEM enter the public engine");
    const auto unknown = engine.routeInfo("missing");
    const auto pending = engine.routeInfo("circuit");
    const auto pendingContact = engine.sampleRouteContact("circuit", 0.0, 0.0);
    CHECK(!unknown && unknown.error().find("unknown route") != std::string::npos && !pending &&
              pending.error().find("no current published alignment") != std::string::npos &&
              !pendingContact,
          "unknown and unpublished routes are distinct refusals");
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    auto info = engine.routeInfo("circuit");
    while (!info && std::chrono::steady_clock::now() < until) {
      const auto advanced = engine.advance();
      CHECK(advanced, advanced ? "route scene advances" : advanced.error().c_str());
      if (!advanced) { break; }
      info = engine.routeInfo("circuit");
      if (!info) { SDL_Delay(1); }
    }
    CHECK(info && info->Closed && info->SegmentCount == 267 && info->LengthM > 4000.0 &&
              info->LengthM < 5000.0,
          info ? "published circuit has its logical 267-edge closed centerline"
               : "published circuit is missing");
    if (info) {
      const auto first = engine.sampleRoute("circuit", 0.0);
      const auto last = engine.sampleRoute("circuit", info->LengthM);
      CHECK(first && last && first->SegmentIndex == 0 && last->SegmentIndex == 0 &&
                Length(first->PositionM - last->PositionM) < 1e-6 &&
                Length(first->Forward - last->Forward) < 1e-6,
            "the public route closes with one local camera position and direction");
      size_t previousSegment = 0;
      bool validSamples = true;
      for (size_t sample = 0; sample < 512; ++sample) {
        const double stationM = info->LengthM * static_cast<double>(sample) / 512.0;
        const auto pose = engine.sampleRoute("circuit", stationM);
        validSamples &= pose && pose->SegmentIndex >= previousSegment &&
                        pose->SegmentIndex < info->SegmentCount && pose->WidthM > 0.0 &&
                        std::abs(Length(pose->Forward) - 1.0) < 1e-9 &&
                        std::abs(Length(pose->Up) - 1.0) < 1e-9 &&
                        std::abs(Dot(pose->Forward, pose->Up)) < 1e-9;
        if (pose) { previousSegment = pose->SegmentIndex; }
      }
      CHECK(validSamples, "sampled public stations advance on a finite orthonormal route basis");
      size_t missingContact = 0;
      double greatestCenterOffsetM = 0.0;
      for (size_t sample = 0; sample <= 512; ++sample) {
        const double stationM = info->LengthM * static_cast<double>(sample) / 512.0;
        const auto pose = engine.sampleRoute("circuit", stationM);
        if (!pose) {
          ++missingContact;
          continue;
        }
        for (double fraction : {-0.45, 0.0, 0.45}) {
          const auto contact =
              engine.sampleRouteContact("circuit", stationM, pose->WidthM * fraction);
          if (!contact || std::abs(Length(contact->Normal) - 1.0) > 1e-6 ||
              contact->Normal[1] <= 0.0) {
            ++missingContact;
            continue;
          }
          if (fraction == 0.0) {
            greatestCenterOffsetM = std::max(greatestCenterOffsetM,
                                             std::abs(contact->PositionM[1] - pose->PositionM[1]));
          }
        }
      }
      CHECK(missingContact == 0 && greatestCenterOffsetM < 0.2,
            "public route contact reads published native road triangles across the circuit");
      CHECK(!engine.sampleRoute("circuit", -1.0) &&
                !engine.sampleRoute("circuit", std::numeric_limits<double>::quiet_NaN()) &&
                !engine.sampleRoute("circuit", info->LengthM + 1.0),
            "invalid stations are rejected before any camera consumes them");
      CHECK(!engine.sampleRouteContact("circuit", -1.0, 0.0) &&
                !engine.sampleRouteContact(
                    "circuit", 0.0, std::numeric_limits<double>::quiet_NaN()) &&
                !engine.sampleRouteContact("circuit", 0.0, 100.0),
            "invalid contact coordinates are refused without terrain fallback");
      const auto refined = engine.preload(0.0, WorldQuality::Refined);
      CHECK(!refined && !engine.settled(WorldQuality::Refined),
            "an incomplete vector fixture cannot claim refined coverage");
      scene.Providers.front().Revision = "pin-r2";
      CHECK(engine.declare(scene) && engine.assemble() && !engine.routeInfo("circuit") &&
                !engine.sampleRouteContact("circuit", 0.0, 0.0),
            "a new source revision cannot expose the previously published alignment");
    }
  }
  SDL_Quit();
  return Report();
}
