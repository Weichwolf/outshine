#include "EngineFactory.h"
#include <Engine.h>
#include <SDL3/SDL.h>

#include "Check.h"

#include <chrono>
#include <print>
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

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl; }

  [[nodiscard]] Coverage Covers(const SourceRequest &request) const noexcept override {
    return request.Kind() == DataKind::Elevation ? Coverage::Inside : Coverage::Outside;
  }

  [[nodiscard]] Address Serves(const SourceRequest &request) const noexcept override {
    return request.Where();
  }

  [[nodiscard]] FetchStart Begin([[maybe_unused]] const Address &where,
                                 [[maybe_unused]] SourceTransport &transport) const override {
    return Ticket::None;
  }

  [[nodiscard]] SourceResponse Collect([[maybe_unused]] const Address &where,
                                       [[maybe_unused]] Ticket ticket,
                                       [[maybe_unused]] SourceTransport &transport) const override {
    return SourceResponse::Delivered({0});
  }

  [[nodiscard]] std::expected<HeightRaster, DecodeFailure>
  DecodeElevation([[maybe_unused]] std::span<const uint8_t> bytes) const override {
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
  [[nodiscard]] std::string_view kind() const override { return "test-terrain"; }

  [[nodiscard]] std::expected<std::unique_ptr<Source>, std::string>
  make([[maybe_unused]] const SourceProvider &declaration,
       [[maybe_unused]] std::string_view builtinDataDirectory) const override {
    return std::make_unique<FlatTerrain>();
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the public preload boundary");
  Roots roots;
  roots.BuiltinDataDirectory = "data";
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
  const FlatTerrainProvider terrainProvider;
  {
    auto engine = App::CreateEngine();
    CHECK(engine.registerProvider(terrainProvider), "local terrain provider registers");
    CHECK(engine.setRoots(roots) && engine.setRenderTarget(scene.Render.Frame) &&
              engine.declare(scene) && engine.assemble(),
          "offline scene queues its declared source requests");
    const auto playable = engine.preload(0.0, WorldQuality::Playable);
    CHECK(!playable && !playable.error().contains("required vector input unavailable"),
          "playable quality retains its existing zero-budget behavior");
    constexpr double patienceS = 5.0;
    const auto started = std::chrono::steady_clock::now();
    const auto refined = engine.preload(patienceS, WorldQuality::Refined);
    const auto elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    CHECK(!refined && refined.error().contains("required vector input unavailable") &&
              refined.error().contains("offline cache miss") &&
              refined.error().contains("vector/") &&
              refined.error().contains("source=test.offline.vector") &&
              refined.error().contains("revision=test-r7") && refined.error().contains("key="),
          "refined failure reports the actual missing vector and source identity");
    CHECK(!refined && !refined.error().contains("did not become resident within"),
          "a final offline vector miss does not wait for the generic timeout");
    CHECK(!engine.settled(WorldQuality::Refined),
          "failed acquisition never publishes a ready world");
    std::println("refined preload elapsed {:.6f} s, result: {}",
                 elapsed,
                 refined ? "ready" : refined.error().c_str());
  }
  SDL_Quit();
  return Report();
}
