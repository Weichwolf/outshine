#include <Engine.h>
#include <SDL3/SDL.h>

#include "Check.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <memory>
#include <expected>
#include <span>
#include <cstdint>
#include <string_view>

namespace {
using namespace outshine;
using namespace outshine::Data;

class FlatTerrain final : public Source {
public:
  FlatTerrain() {
    Decl.Id = "test.flat.terrain";
    Decl.Revision = "r1";
    Decl.Kind = DataKind::Elevation;
    Decl.Wire = WireFormat::ProviderDefined;
    Decl.MaxZoom = 8;
    Decl.Keeps = Cacheability::Never;
  }

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const SourceRequest &request) const noexcept override {
    return request.Kind() == DataKind::Elevation ? Coverage::Inside : Coverage::Outside;
  }

  Address Serves(const SourceRequest &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address &, Transport &) const override { return Ticket::None; }

  SourceResponse Collect(const Address &, Ticket, Transport &) const override {
    return SourceResponse::Delivered({0});
  }

  std::expected<HeightRaster, DecodeFailure>
  DecodeElevation(std::span<const uint8_t>) const override {
    HeightRaster raster;
    raster.Rows = 2;
    raster.Cols = 2;
    raster.Meters.assign(4, 0.0f);
    return raster;
  }

private:
  SourceDecl Decl;
};

class FlatTerrainProvider final : public Provider {
public:
  std::string_view kind() const override { return "test-terrain"; }

  std::expected<std::unique_ptr<Source>, std::string> make(const SourceProvider &,
                                                           std::string_view) const override {
    return std::make_unique<FlatTerrain>();
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the public preload boundary");
  Roots roots;
  roots.Shipped = "src/assets";
  roots.Offline = true;
  Scenario::Document scene;
  scene.Ground.Declared = true;
  scene.Ground.SightM = 2400.0;
  scene.Ground.Origin = {.LatitudeDeg = 54.79, .LongitudeDeg = 9.44};
  scene.Render.Declared = true;
  scene.Render.Frame = {.WidthPx = 80, .HeightPx = 45};
  Scenario::View view;
  view.Id = "missing-data";
  view.Placement = Scenario::CameraPlacement::Geodetic;
  view.Geographic.Geodetic = {.LongitudeDeg = 9.44, .LatitudeDeg = 54.79, .HeightM = 50};
  view.Geographic.SamplesHeight = false;
  scene.Views.push_back(view);
  Data::SourceProvider vector;
  vector.Kind = "vector";
  vector.Dataset = "test.offline.vector";
  vector.Revision = "test-r7";
  vector.Endpoint = "https://fixture.invalid/{z}/{x}/{y}";
  vector.Schema = "shortbread";
  scene.Providers.push_back(vector);
  Data::SourceProvider terrain;
  terrain.Kind = "test-terrain";
  terrain.Revision = "test-r1";
  scene.Providers.push_back(terrain);
  FlatTerrainProvider terrainProvider;
  {
    Engine engine;
    CHECK(engine.registerProvider(terrainProvider), "local terrain provider registers");
    CHECK(engine.setRoots(roots) && engine.setRenderTarget(scene.Render.Frame) &&
              engine.declare(scene) && engine.assemble(),
          "offline scene queues its declared source requests");
    const auto playable = engine.preload(0.0, WorldQuality::Playable);
    CHECK(!playable &&
              playable.error().find("required vector input unavailable") == std::string::npos,
          "playable quality retains its existing zero-budget behavior");
    constexpr double patienceS = 5.0;
    const auto started = std::chrono::steady_clock::now();
    const auto refined = engine.preload(patienceS, WorldQuality::Refined);
    const auto elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    CHECK(!refined &&
              refined.error().find("required vector input unavailable") != std::string::npos &&
              refined.error().find("offline cache miss") != std::string::npos &&
              refined.error().find("vector/") != std::string::npos &&
              refined.error().find("source=test.offline.vector") != std::string::npos &&
              refined.error().find("revision=test-r7") != std::string::npos &&
              refined.error().find("key=") != std::string::npos,
          "refined failure reports the actual missing vector and source identity");
    CHECK(!refined && refined.error().find("did not become resident within") == std::string::npos,
          "a final offline vector miss does not wait for the generic timeout");
    CHECK(!engine.settled(WorldQuality::Refined),
          "failed acquisition never publishes a ready world");
    std::printf("refined preload elapsed %.6f s, result: %s\n",
                elapsed,
                refined ? "ready" : refined.error().c_str());
  }
  SDL_Quit();
  return Report();
}
