#include <Outshine.h>
#include <SDL3/SDL.h>

#include "Check.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <string_view>

namespace {

[[nodiscard]] std::optional<double> Measure(const outshine::Engine &engine, std::string_view name) {
  for (const auto &sample : engine.measures()) {
    if (sample.Name == name) { return sample.Value; }
  }
  return std::nullopt;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the route camera scene");
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
         .Revision = "camera-pin-r1",
         .Missing = Data::MissingDataPolicy::Fail,
         .Dataset = "openstreetmap",
         .Location = "src/assets/world/osm/HockenheimringGrandPrix.osm",
         .Coverage = Data::SourceCoverage{
             .WestDeg = 8.54, .SouthDeg = 49.315, .EastDeg = 8.61, .NorthDeg = 49.34}});
    scene.Routes.push_back({.Id = "circuit", .OsmRelationId = 284588});
    Scenario::View overview;
    overview.Id = "overview";
    overview.Placement = Scenario::CameraPlacement::Geodetic;
    overview.Geographic.Geodetic = {
        .LongitudeDeg = 8.5659, .LatitudeDeg = 49.3274, .HeightM = 900.0};
    overview.Geographic.BearingDeg = 90.0;
    overview.Geographic.PitchDeg = -60.0;
    overview.Sees.FovDeg = 55.0;
    scene.Views.push_back(overview);
    Scenario::View lap;
    lap.Id = "lap";
    lap.Placement = Scenario::CameraPlacement::Route;
    lap.Route.RouteId = "circuit";
    lap.Person = "first";
    lap.Sees.FovDeg = 70.0;
    scene.Views.push_back(lap);
    Scenario::View chase = lap;
    chase.Id = "chase";
    chase.Person = "third";
    chase.DistanceM = 6.0;
    chase.RisesBy = 0.4;
    scene.Views.push_back(chase);

    CHECK(engine.setRoots({.Shipped = ".", .Offline = true}) && engine.setRenderTarget({160, 90}) &&
              engine.declare(scene) && engine.assemble(),
          "the pinned circuit and route view enter the public engine");
    CHECK(!engine.setView("lap") && engine.advance(),
          "a route camera cannot be selected before its alignment is published");
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    auto info = engine.routeInfo("circuit");
    while (!info && std::chrono::steady_clock::now() < until) {
      const auto advanced = engine.advance();
      CHECK(advanced, advanced ? "route scene advances" : advanced.error().c_str());
      if (!advanced) { break; }
      info = engine.routeInfo("circuit");
      if (!info) { SDL_Delay(1); }
    }
    CHECK(info && info->Closed && info->LengthM > 4000.0,
          info ? "a complete closed route is published" : "route publication timed out");
    if (info) {
      CHECK(engine.setView("lap"), "the ready route camera selects without rebuilding the world");
      double previousStationM = 0.0;
      bool movedOnAlignment = true;
      for (int tick = 0; tick < 60; ++tick) {
        const auto advanced = engine.advance();
        if (!advanced) {
          movedOnAlignment = false;
          break;
        }
        const auto stationM = Measure(engine, "the route camera's station");
        const auto eastM = Measure(engine, "the route camera's eye, east");
        const auto upM = Measure(engine, "the route camera's eye, up");
        const auto eyeZM = Measure(engine, "the route camera's eye, south");
        movedOnAlignment &= stationM && eastM && upM && eyeZM && *stationM >= previousStationM &&
                            *stationM <= info->LengthM;
        if (!stationM || !eastM || !upM || !eyeZM) { break; }
        const auto pose = engine.sampleRoute("circuit", *stationM);
        const Vec3 expectedEye = pose ? pose->PositionM + pose->Up * lap.Route.EyeHeightM : Vec3{};
        movedOnAlignment &= pose && Length(expectedEye - Vec3{{*eastM, *upM, *eyeZM}}) < 1e-4;
        previousStationM = *stationM;
      }
      CHECK(movedOnAlignment && previousStationM > 0.5,
            "60 fixed ticks move the eye monotonically along the published route at metre height");
      CHECK(engine.setView("chase") && engine.advance(),
            "the third-person rig also binds to the same native alignment");
      const auto chaseStationM = Measure(engine, "the route camera's station");
      const auto chaseEastM = Measure(engine, "the route camera's eye, east");
      const auto chaseUpM = Measure(engine, "the route camera's eye, up");
      const auto chaseZM = Measure(engine, "the route camera's eye, south");
      std::optional<RoutePose> chasePose;
      if (chaseStationM) {
        const auto sampled = engine.sampleRoute("circuit", *chaseStationM);
        if (sampled) { chasePose = *sampled; }
      }
      const Vec3 expectedChaseEye =
          chasePose ? chasePose->PositionM + chasePose->Up * chase.Route.EyeHeightM -
                          chasePose->Forward * chase.DistanceM +
                          chasePose->Up * (chase.DistanceM * chase.RisesBy)
                    : Vec3{};
      CHECK(chasePose && chaseEastM && chaseUpM && chaseZM &&
                Length(expectedChaseEye - Vec3{{*chaseEastM, *chaseUpM, *chaseZM}}) < 1e-4,
            "third-person eye stays six metres behind its route seat with metric rise");
      CHECK(engine.setView("overview") && engine.setView("lap") && engine.advance(),
            "switching away and back restarts a fresh route camera motion");
      const auto restartedStationM = Measure(engine, "the route camera's station");
      CHECK(restartedStationM && *restartedStationM < previousStationM,
            "reselection restarts the route at its first station");
    }
  }
  SDL_Quit();
  return Report();
}
