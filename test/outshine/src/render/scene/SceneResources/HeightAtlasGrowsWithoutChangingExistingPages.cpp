#include "Check.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"

#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
namespace Native = Render::Native;
constexpr double kFill = 0.6;
constexpr double kKeyLux = 10000;
constexpr double kKeyElevationDeg = 90;
constexpr double kNearM = 0.1;
constexpr double kFarM = 100;

struct HeightAtlasFaults {
  bool RefuseTexture = false;
  std::vector<uint32_t> Layers;

  class Observed final : public Test::Gpu::NativeDeviceProxy {
  public:
    Observed(std::unique_ptr<Native::Device> actual, HeightAtlasFaults &faults)
        : NativeDeviceProxy(*actual), Actual_(std::move(actual)), Faults_(faults) {}

    Native::Result<std::unique_ptr<Native::Texture>>
    CreateTexture(Native::TextureDescription description, std::string_view label) override {
      if (description.Type == Native::TextureType::Image2DArray &&
          description.Texels == Native::Format::R32Float) {
        Faults_.Layers.push_back(description.Layers);
        if (std::exchange(Faults_.RefuseTexture, false)) {
          return std::unexpected(
              Native::Error{.Kind = Native::ErrorKind::OutOfMemory,
                            .Message = "injected height atlas allocation failure"});
        }
      }
      return Backing().CreateTexture(description, label);
    }

  private:
    std::unique_ptr<Native::Device> Actual_;
    HeightAtlasFaults &Faults_;
  };

  Render::GpuSubmission Submission() {
    return {.Context = this,
            .Create = [](void *context, Native::DeviceOptions options)
                -> Native::Result<std::unique_ptr<Native::Device>> {
              options.Validation = true;
              auto actual = Native::CreateDevice(options);
              if (!actual) { return std::unexpected(actual.error()); }
              return std::make_unique<Observed>(std::move(*actual),
                                                *static_cast<HeightAtlasFaults *>(context));
            }};
  }
};

bool ReadPageImage(Render::SceneRenderer &renderer,
                   Core::RuntimeScene &scene,
                   Render::HeightPageHandle page,
                   std::vector<float> &pixels,
                   std::string &error) {
  Render::TerrainTile tile;
  tile.Row = {{1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1}};
  tile.Corners = {{-1, 1, 1, 1, -1, -1, 1, -1}};
  tile.Page = page;
  tile.LowM = 1;
  tile.HighM = 3;
  tile.StepE = tile.StepN = 2.0f / static_cast<float>(Render::GroundLattice::kSide - 1);
  return renderer.SetTerrainTiles({&tile, 1}, {}, error) && scene.Advance(error) &&
         scene.Draw(error) && renderer.ReadSceneLinear(pixels) == Render::ReadState::Ready;
}

bool Varies(std::span<const float> pixels) {
  for (std::size_t at = 4; at + 3 < pixels.size(); at += 4) {
    for (std::size_t channel = 0; channel < 3; ++channel) {
      if (pixels[at + channel] != pixels[channel]) { return true; }
    }
  }
  return false;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the real height atlas");
  {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneLinear"};
    declaration.Fill = kFill;
    declaration.KeyLux = kKeyLux;
    declaration.KeyElevationDeg = kKeyElevationDeg;
    declaration.IndirectLight = {1, 1, 1};
    declaration.Haze = 0;
    HeightAtlasFaults faults;
    Render::SceneRenderer renderer(faults.Submission());
    const auto uploads = renderer.OfflineUploads(std::chrono::seconds(5));
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "fixture opens");
    if (!scene) { return Report(); }
    Geometry geometry;
    const auto surface = geometry.addSurface("terrain", Material{});
    CHECK(surface.has_value(), "ground material exists");
    if (!surface) { return Report(); }
    const auto part = geometry.addPart("material-carrier", *surface);
    CHECK(part.has_value(), "ground material has a native slot");
    if (!part) { return Report(); }
    CHECK(geometry.setPositions(*part, std::array<float, 9>{-1, 0, -1, 1, 0, -1, -1, 0, 1}),
          "material carrier is beneath the terrain");
    CHECK(geometry.setNormals(*part, std::array<float, 9>{0, 1, 0, 0, 1, 0, 0, 1, 0}),
          "material carrier has upward normals");
    CHECK(geometry.setTexture(*part, std::array<float, 6>{-1, -1, 1, -1, -1, 1}),
          "ground material uses metric coordinates");
    CHECK(geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 2, 1}),
          "material carrier has a native triangle");
    Core::WorldCandidate candidate(renderer);
    const auto prepared = candidate.Prepare(*scene, nullptr);
    CHECK(prepared.has_value(), "ground world prepares");
    if (!prepared) { return Report(); }
    candidate.GroundIs(surface->index());
    CHECK(candidate.SetGeometry(std::move(geometry), 0, Material{}, error),
          "ground material binds to the scene");
    CHECK(candidate.Publish(scene).has_value(), "ground world publishes");
    CHECK(faults.Layers.empty(), "a scene without terrain allocates no height texture");
    const std::vector<float> nodes(Render::GroundLattice::kPageNodes, 3);
    const auto first = renderer.PlaceHeightPage(nodes);
    CHECK(first && faults.Layers == std::vector<uint32_t>{1}, "the first page allocates one layer");
    if (!first) { return Report(); }
    const auto viewpoint =
        Render::Viewpoint::LookAt({.EyeM = {{0, 7, 8}}, .AimM = {{0, 3, 0}}}, 0.0);
    CHECK(viewpoint.has_value(), "the fixture has a valid camera");
    if (!viewpoint) { return Report(); }
    auto eye = *viewpoint;
    eye.YfovRad = 1;
    eye.ZNearM = kNearM;
    eye.ZFarM = kFarM;
    scene->Eye(eye);
    std::vector<float> baseline;
    CHECK(ReadPageImage(renderer, *scene, *first, baseline, error) && Varies(baseline),
          "a visible terrain page produces a nonuniform baseline image");
    std::optional<Render::HeightPageHandle> last;
    const std::vector<float> lowerNodes(Render::GroundLattice::kPageNodes, 1);
    for (uint32_t page = 1; page < Render::GroundLattice::kPagesPerLayer; ++page) {
      const bool final = page + 1 == Render::GroundLattice::kPagesPerLayer;
      const auto placed = renderer.PlaceHeightPage(final ? lowerNodes : nodes);
      CHECK(placed.has_value(), "the first layer fills without growth");
      if (final && placed) { last = *placed; }
    }
    std::vector<float> lowerImage;
    CHECK(last && ReadPageImage(renderer, *scene, *last, lowerImage, error) && Varies(lowerImage) &&
              lowerImage != baseline,
          "the last atlas row and column render their distinct uploaded heights");
    CHECK(faults.Layers.size() == 1, "only live page demand allocates texture layers");
    faults.RefuseTexture = true;
    const auto refused = renderer.PlaceHeightPage(nodes);
    CHECK(!refused &&
              refused.error().find("injected height atlas allocation failure") != std::string::npos,
          "failed growth returns the allocation error");
    std::vector<float> afterFailure;
    CHECK(ReadPageImage(renderer, *scene, *first, afterFailure, error) && afterFailure == baseline,
          "failed growth preserves the old page handle and its GPU samples");
    const auto next = renderer.PlaceHeightPage(nodes);
    CHECK(next && next->Slot == Render::GroundLattice::kPagesPerLayer && faults.Layers.back() == 2,
          "growth retries into the next layer without losing a page reservation");
    std::vector<float> afterGrowth;
    CHECK(ReadPageImage(renderer, *scene, *first, afterGrowth, error) && afterGrowth == baseline,
          "GPU-to-GPU growth preserves every sample of the published page");
    CHECK(last && ReadPageImage(renderer, *scene, *last, afterGrowth, error) &&
              afterGrowth == lowerImage,
          "growth preserves the distinct heights at the opposite atlas corner");
    if (next) {
      CHECK(ReadPageImage(renderer, *scene, *next, afterGrowth, error) && afterGrowth == baseline,
            "the new layer uses the same page address and height sampling contract");
    }
  }
  SDL_Quit();
  return Report();
}
