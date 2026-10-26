#include "MeshInput.h"
#include "Check.h"
#include "TestEngine.h"
#include "WorldCandidate.h"
#include "Engine.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <limits>
#include <string_view>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr int kSide = 32;
constexpr double kNearM = 0.1, kFarM = 10;

double Metric(const Engine &engine, std::string_view name) {
  for (const auto &sample : engine.measures()) {
    if (sample.Name == name) { return sample.Value; }
  }
  return std::numeric_limits<double>::quiet_NaN();
}

void Exercise() {
  Engine engine;
  Scenario::Document scenario;
  scenario.Render.Declared = true;
  scenario.Render.Frame = {.WidthPx = kSide, .HeightPx = kSide};
  scenario.Render.Outputs = {"sceneLinear"};
  Scenario::View view;
  view.Id = "fixture";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{0, 0, 4}};
  view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = kNearM, .FarM = kFarM});
  scenario.Views.push_back(view);
  CHECK(engine.setRenderTarget({.WidthPx = kSide, .HeightPx = kSide}) && engine.declare(scenario) &&
            engine.assemble(),
        "an offscreen engine assembles its declared published view");
  Geometry geometry;
  const auto surface = geometry.addSurface("fixture", Material{});
  if (!surface) { return; }
  const auto part = geometry.addPart("fixture", *surface);
  CHECK(part && geometry.setPositions(*part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
            geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}) &&
            engine.setGeometry(geometry) && engine.advance() && engine.renderer().render({}),
        "actual native published geometry draws before candidate creation");
  auto &state = TestEngine::StateOf(engine);
  if (!state.Picture.Standing) { return; }
  const auto batches = state.Picture.Device.GeometryBatchCount();
  const auto pushes = state.Picture.Device.GeometryUniformPushes();
  CHECK(batches > 0 && pushes > 0 && Metric(engine, "batches the picture draws") == batches,
        "the initial render measurements match the actual native published world");
  state.Picture.Device.WaitForGpu();
  std::vector<float> before;
  CHECK(engine.renderer().readPixels(Buffer::Linear, before) && !before.empty(),
        "the published baseline image is available");
  {
    Core::WorldCandidate candidate(state.Picture.Device);
    {
      const auto uploads = state.Picture.Device.OfflineUploads(std::chrono::seconds(5));
      const auto prepared = candidate.Prepare(*state.Picture.Standing,
                                              nullptr,
                                              Render::SceneResources::PieceSources::Copy,
                                              Core::GeometryRetention::All,
                                              Core::ResourceRestoreMode::Deferred);
      CHECK(prepared.has_value(),
            prepared ? "an incomplete candidate owns an active editing scope"
                     : prepared.error().c_str());
      if (!prepared) { return; }
      std::string error;
      const bool emptied = state.Picture.Device.SetGeometryMesh(Render::MeshInput{}, error);
      CHECK(emptied, "the active candidate has not retained a resident subject mesh");
      if (!emptied) { return; }
    }
    const auto candidateBatches = state.Picture.Device.GeometryBatchCount();
    const auto candidatePushes = state.Picture.Device.GeometryUniformPushes();
    CHECK(candidateBatches == 0 && candidatePushes == 0,
          "the unfinished candidate differs from the published render counters");
    CHECK(engine.renderer().render({}), "the engine keeps rendering the published world");
    CHECK(Metric(engine, "batches the picture draws") == batches &&
              Metric(engine, "geometry draw records") > 0 &&
              Metric(engine, "vertex uniform pushes the geometry stages make") > pushes,
          "published frame diagnostics describe the world that actually drew");
    CHECK(state.Picture.Device.HasWorldCandidate() &&
              state.Picture.Device.GeometryBatchCount() == candidateBatches &&
              state.Picture.Device.GeometryUniformPushes() == candidatePushes,
          "statistics restore the candidate edit context without publishing or changing it");
    state.Picture.Device.WaitForGpu();
    std::vector<float> during;
    const auto read = engine.renderer().readPixels(Buffer::Linear, during);
    CHECK(read,
          read ? "the published native image reads after synchronization" : read.error().c_str());
    CHECK(read && during == before,
          "candidate preparation and corrected measurement scope preserve the actual pixels");
  }
  CHECK(!state.Picture.Device.HasWorldCandidate() &&
            state.Picture.Device.GeometryBatchCount() == batches && engine.renderer().render({}),
        "abandoning the candidate retains the published world and normal diagnostics");
  CHECK(Metric(engine, "batches the picture draws") == batches,
        "ordinary rendering remains correct after candidate abandonment");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the native engine case");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) { Exercise(); }
  SDL_Quit();
  return Report();
}
