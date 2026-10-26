#ifndef OUTSHINE_TEST_HOCKENHEIMSCENEFIXTURE_H
#define OUTSHINE_TEST_HOCKENHEIMSCENEFIXTURE_H

#include "HockenheimCircuitFixture.h"
#include "Engine.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <expected>

namespace outshine::Test::HockenheimFixture {
inline constexpr double SightM = 3000;
inline constexpr double OverviewHeightM = 900;
inline constexpr double OverviewBearingDeg = 90;
inline constexpr double OverviewPitchDeg = -60;
inline constexpr double OverviewFovDeg = 55;
inline constexpr auto PublicationTimeout = std::chrono::seconds(60);

[[nodiscard]] inline Scenario::Document AnalyticScene() {
  Scenario::Document scene;
  scene.Ground.Declared = true;
  scene.Ground.Origin.LatitudeDeg = LatitudeDeg;
  scene.Ground.Origin.LongitudeDeg = LongitudeDeg;
  scene.Ground.Shape.Kind = "sineGrid";
  scene.Ground.VegetationEnabled = false;
  scene.Ground.SightM = SightM;
  scene.Render.Declared = true;
  scene.Render.Frame = Frame;
  scene.Lit.Declared = true;
  scene.Providers.push_back(LocalSource());
  scene.Routes.push_back({.Id = "circuit", .OsmRelationId = RelationId});
  Scenario::View view;
  view.Id = "overview";
  view.Placement = Scenario::CameraPlacement::Geodetic;
  view.Geographic.Geodetic = {
      .LongitudeDeg = LongitudeDeg, .LatitudeDeg = LatitudeDeg, .HeightM = OverviewHeightM};
  view.Geographic.BearingDeg = OverviewBearingDeg;
  view.Geographic.PitchDeg = OverviewPitchDeg;
  view.Sees.FovDeg = OverviewFovDeg;
  scene.Views.push_back(view);
  return scene;
}

[[nodiscard]] inline Holds<RouteInfo> WaitForRoute(Engine &engine) {
  const auto until = std::chrono::steady_clock::now() + PublicationTimeout;
  auto info = engine.routeInfo("circuit");
  while (!info && std::chrono::steady_clock::now() < until) {
    const auto advanced = engine.advance();
    CHECK(advanced, advanced ? "route scene advances" : advanced.error().c_str());
    if (!advanced) { return std::unexpected(advanced.error()); }
    info = engine.routeInfo("circuit");
    if (!info) { SDL_Delay(1); }
  }
  return info;
}
}
#endif
