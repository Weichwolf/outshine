#include "HockenheimCircuitFixture.h"
#include "HockenheimSceneFixture.h"
#include "EngineFactory.h"
#include <Engine.h>
#include <SDL3/SDL.h>

#include "Check.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string_view>

namespace Circuit = outshine::Test::HockenheimFixture;

namespace {
constexpr double kFovDeg = 70.0;
constexpr double kChaseDistanceM = 6.0;
constexpr double kChaseRise = 0.4;
constexpr double kMinimumLapLengthM = 4000.0;
constexpr int kInitialTicks = 60;
constexpr double kEyeToleranceM = 1e-4;
constexpr double kStationToleranceM = 1e-3;
constexpr double kMaximumEyeStepM = 1.5;

[[nodiscard]] std::optional<double> Measure(const outshine::Engine &engine, std::string_view name) {
  for (const auto &sample : engine.measures()) {
    if (sample.Name == name) { return sample.Value; }
  }
  return std::nullopt;
}

}

namespace {
using namespace outshine;
using namespace outshine::Test;

double CheckFirstTicks(Engine &engine, const Scenario::View &lap, const RouteInfo &info) {
  double previousStationM = 0.0;
  bool movedOnAlignment = true;
  for (int tick = 0; tick < kInitialTicks; ++tick) {
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
                        *stationM <= info.LengthM;
    if (!stationM || !eastM || !upM || !eyeZM) { break; }
    const auto pose = engine.sampleRoute("circuit", *stationM);
    const Vec3 expectedEye = pose ? pose->PositionM + pose->Up * lap.Route.EyeHeightM : Vec3{};
    movedOnAlignment &= pose && Length(expectedEye - Vec3{{*eastM, *upM, *eyeZM}}) < kEyeToleranceM;
    previousStationM = *stationM;
  }
  CHECK(movedOnAlignment && previousStationM > 0.5,
        "60 fixed ticks move the eye monotonically along the published route at metre height");

  return previousStationM;
}

double CheckLap(Engine &engine,
                const Scenario::View &lap,
                const RouteInfo &info,
                double previousStationM) {
  bool movedOnAlignment = true;
  const double maximumStationStepM = lap.Route.MaximumSpeedMps * engine.stepSeconds() + 0.05;
  double greatestStationStepM = 0.0;
  double greatestEyeStepM = 0.0;
  const auto previousPose = engine.sampleRoute("circuit", previousStationM);
  movedOnAlignment &= previousPose.has_value();
  Vec3 previousEye =
      previousPose ? previousPose->PositionM + previousPose->Up * lap.Route.EyeHeightM : Vec3{};
  constexpr double kMaximumLapTimeS = 300.0;
  const int maximumTicks = static_cast<int>(std::ceil(kMaximumLapTimeS / engine.stepSeconds()));
  for (int tick = kInitialTicks;
       movedOnAlignment && tick < maximumTicks && previousStationM < info.LengthM;
       ++tick) {
    const auto advanced = engine.advance();
    if (!advanced) {
      movedOnAlignment = false;
      break;
    }
    const auto stationM = Measure(engine, "the route camera's station");
    const auto eastM = Measure(engine, "the route camera's eye, east");
    const auto upM = Measure(engine, "the route camera's eye, up");
    const auto eyeZM = Measure(engine, "the route camera's eye, south");
    if (!stationM || !eastM || !upM || !eyeZM || *stationM < previousStationM ||
        *stationM > info.LengthM) {
      movedOnAlignment = false;
      break;
    }
    const Vec3 eye{{*eastM, *upM, *eyeZM}};
    greatestStationStepM = std::max(greatestStationStepM, *stationM - previousStationM);
    greatestEyeStepM = std::max(greatestEyeStepM, Length(eye - previousEye));
    const auto pose = engine.sampleRoute("circuit", *stationM);
    movedOnAlignment &=
        pose && Length(pose->PositionM + pose->Up * lap.Route.EyeHeightM - eye) < kEyeToleranceM;
    previousStationM = *stationM;
    previousEye = eye;
    if (!movedOnAlignment) { break; }
  }
  CHECK(movedOnAlignment && previousStationM >= info.LengthM - kStationToleranceM &&
            greatestStationStepM <= maximumStationStepM && greatestEyeStepM <= kMaximumEyeStepM,
        "the complete circuit has bounded motion and no eye-position jump at source edges");

  return previousStationM;
}

void CheckChase(Engine &engine, const Scenario::View &chase, double previousStationM) {
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
  const Vec3 expectedChaseEye = chasePose ? chasePose->PositionM +
                                                chasePose->Up * chase.Route.EyeHeightM -
                                                chasePose->Forward * chase.DistanceM +
                                                chasePose->Up * (chase.DistanceM * chase.RisesBy)
                                          : Vec3{};
  CHECK(chasePose && chaseEastM && chaseUpM && chaseZM &&
            Length(expectedChaseEye - Vec3{{*chaseEastM, *chaseUpM, *chaseZM}}) < kEyeToleranceM,
        "third-person eye stays six metres behind its route seat with metric rise");
  CHECK(engine.setView("overview") && engine.setView("lap") && engine.advance(),
        "switching away and back restarts a fresh route camera motion");
  const auto restartedStationM = Measure(engine, "the route camera's station");
  CHECK(restartedStationM && *restartedStationM < previousStationM,
        "reselection restarts the route at its first station");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the route camera scene");
  {
    auto engine = App::CreateEngine();
    auto scene = Circuit::AnalyticScene();
    scene.Providers.front().Revision = "camera-pin-r1";
    Scenario::View lap;
    lap.Id = "lap";
    lap.Placement = Scenario::CameraPlacement::Route;
    lap.Route.RouteId = "circuit";
    lap.Person = "first";
    lap.Sees.FovDeg = kFovDeg;
    scene.Views.push_back(lap);
    Scenario::View chase = lap;
    chase.Id = "chase";
    chase.Person = "third";
    chase.DistanceM = kChaseDistanceM;
    chase.RisesBy = kChaseRise;
    scene.Views.push_back(chase);

    CHECK(engine.setRoots({.Assets = {},
                           .Shipped = "src/assets",
                           .Cache = {},
                           .Offline = true,
                           .AssetCache = {}}) &&
              engine.setRenderTarget(Circuit::Frame) && engine.declare(scene) && engine.assemble(),
          "the pinned circuit and route view enter the public engine");
    CHECK(!engine.setView("lap") && engine.advance(),
          "a route camera cannot be selected before its alignment is published");
    const auto info = Circuit::WaitForRoute(engine);
    CHECK(info && info->Closed && info->SegmentCount == Circuit::EdgeCount &&
              info->LengthM > kMinimumLapLengthM,
          info ? "a complete closed route is published" : "route publication timed out");
    if (info) {
      CHECK(engine.setView("lap"), "the ready route camera selects without rebuilding the world");
      const double stationM = CheckFirstTicks(engine, lap, *info);
      const double endStationM = CheckLap(engine, lap, *info, stationM);
      CheckChase(engine, chase, endStationM);
    }
  }
  SDL_Quit();
  return Report();
}
