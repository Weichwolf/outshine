#include "Check.h"
#include "WorldCandidate.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr Vec4f kBedColour = {{0.6f, 0.25f, 0.1f, 1}};
constexpr Vec4f kPaneColour = {{0.3f, 0.5f, 0.7f, 1}};
constexpr std::array<double, 3> kIndirectLight = {0.2, 0.2, 0.2};
constexpr double kPaneTransmission = 0.6;
constexpr double kKeyLux = 20;
constexpr double kKeyElevationDeg = 45;
constexpr double kNearM = 0.1;
constexpr double kFarM = 100;

struct Counts {
  unsigned Graphics = 0;
  size_t Buffers = 0;
  size_t BufferBytes = 0;
  size_t Submissions = 0;
  size_t Waits = 0;
};

class Counted final : public outshine::Test::Gpu::NativeDeviceProxy {
public:
  Counted(std::unique_ptr<Native::Device> device, Counts &counts)
      : NativeDeviceProxy(*device), Owned_(std::move(device)), Counts_(counts) {}

  static Native::Result<std::unique_ptr<Native::Device>> Create(void *context,
                                                                Native::DeviceOptions options) {
    auto made = Native::CreateDevice(options);
    if (!made) { return std::unexpected(made.error()); }
    return std::make_unique<Counted>(std::move(*made), *static_cast<Counts *>(context));
  }

  Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
  CreateGraphicsPipeline(const Native::GraphicsDescription &description) override {
    ++Counts_.Graphics;
    return Backing().CreateGraphicsPipeline(description);
  }

  Native::Result<std::unique_ptr<Native::Buffer>>
  CreateBuffer(Native::BufferDescription description, std::string_view label = {}) override {
    ++Counts_.Buffers;
    Counts_.BufferBytes += description.Bytes;
    return Backing().CreateBuffer(description, label);
  }

  Native::Result<Native::Completion> Submit(std::unique_ptr<Native::Commands> commands) override {
    auto submitted = Backing().Submit(std::move(commands));
    if (submitted) { ++Counts_.Submissions; }
    return submitted;
  }

  Native::Result<Native::CompletionStatus> Wait(Native::Completion completion,
                                                std::chrono::nanoseconds budget) override {
    ++Counts_.Waits;
    return Backing().Wait(completion, budget);
  }

private:
  std::unique_ptr<Native::Device> Owned_;
  Counts &Counts_;
};

Geometry Mesh(bool glass) {
  Geometry mesh;
  Material back;
  back.BaseColour = kBedColour;
  Material front;
  front.BaseColour = kPaneColour;
  front.Transmission = glass ? kPaneTransmission : 0;
  const auto bed = mesh.addSurface("bed", back);
  const auto pane = mesh.addSurface("pane", front);
  CHECK(bed && pane, "both material inputs are complete");
  if (!bed || !pane) { return mesh; }
  for (unsigned i = 0; i < 2; ++i) {
    const auto part = mesh.addPart(i == 0 ? "bed" : "front", i == 0 ? *bed : *pane);
    const float z = i == 0 ? -0.5f : 0;
    CHECK(part && mesh.setPositions(*part, std::array<float, 9>{-1, -1, z, 1, -1, z, 0, 1, z}) &&
              mesh.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
          "generated surfaces have valid triangles");
  }
  return mesh;
}

std::vector<float> Draw(Core::RuntimeScene &scene, SceneRenderer &renderer) {
  const auto published = renderer.PublishedWorld();
  std::string error;
  std::vector<float> pixels;
  CHECK(scene.Advance(error) && scene.Draw(error), error.c_str());
  CHECK(renderer.ReadSceneLinear(pixels) == ReadState::Ready && !pixels.empty(),
        "the actual published image is available");
  return pixels;
}

void Exercise(bool glass) {
  Counts counts;
  SceneRenderer renderer({.Context = &counts, .Create = Counted::Create});
  Core::Declaration declaration;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"sceneLinear"};
  declaration.Exposure = 1;
  declaration.KeyLux = kKeyLux;
  declaration.KeyElevationDeg = kKeyElevationDeg;
  declaration.IndirectLight = kIndirectLight;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), error.c_str());
  if (!scene) { return; }
  {
    Core::WorldCandidate first(renderer);
    CHECK(first.Prepare(*scene, nullptr).has_value(), "first candidate prepares");
    CHECK(first.SetGeometry(Mesh(glass), 0, error), error.c_str());
    CHECK(first.Publish(scene).has_value(), "generated world publishes");
  }
  Viewpoint eye;
  eye.EyeM = {{0, 0, 4}};
  eye.YfovRad = 1;
  eye.ZNearM = kNearM;
  eye.ZFarM = kFarM;
  scene->Eye(eye);
  CHECK(scene->DrivenParts() == 0 && renderer.Plan().Holds(Stage::TransmissiveGeometry) == glass,
        "the published generated world has its complete material plan and no driven parts");
  const auto before = Draw(*scene, renderer);
  const auto spec = renderer.Plan().Specification();
  for (const bool publish : {false, true}) {
    Core::WorldCandidate next(renderer);
    const auto graphics = counts.Graphics;
    const auto previous = counts;
    CHECK(next.Prepare(*scene,
                       nullptr,
                       SceneResources::PieceSources::Copy,
                       Core::SubjectGeometrySources::Driven,
                       Core::ResourceRestoreMode::Deferred)
              .has_value(),
          "empty next candidate prepares privately");
    CHECK(renderer.Plan().Specification() == spec,
          "incomplete input retains the complete published render plan");
    std::println("glass={} new_graphics_before_complete={}", glass, counts.Graphics - graphics);
    CHECK(counts.Graphics == graphics,
          "incomplete world preparation builds no replacement graphics pipelines");
    std::println("buffers={} bytes={} submits={} waits={}",
                 counts.Buffers - previous.Buffers,
                 counts.BufferBytes - previous.BufferBytes,
                 counts.Submissions - previous.Submissions,
                 counts.Waits - previous.Waits);
    CHECK(counts.Buffers == previous.Buffers && counts.BufferBytes == previous.BufferBytes,
          "unchanged terrain inputs allocate no replacement or staging buffers");
    CHECK(counts.Submissions == previous.Submissions && counts.Waits == previous.Waits,
          "unchanged terrain inputs neither submit uploads nor wait for the GPU");
    CHECK(Draw(*scene, renderer) == before,
          "published pixels stay exact while the candidate is incomplete");
    if (publish) {
      CHECK(next.SetGeometry(Mesh(!glass), 0, error), error.c_str());
      CHECK(next.Publish(scene).has_value(), "completed candidate publishes atomically");
      CHECK(renderer.Plan().Holds(Stage::TransmissiveGeometry) == !glass,
            "complete new materials can legitimately change the final plan");
      CHECK(!Draw(*scene, renderer).empty(),
            "completed replacement renders through its actual plan");
    }
  }
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
    Exercise(true);
    Exercise(false);
  }
  SDL_Quit();
  return Report();
}
