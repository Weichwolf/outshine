#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"

#include <SDL3/SDL.h>
#include <cassert>
#include <dlfcn.h>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace {
bool RefuseHeightTexture = false;
std::vector<uint32_t> HeightLayers;
}

extern "C" SDL_GPUTexture *SDLCALL SDL_CreateGPUTexture(SDL_GPUDevice *device,
                                                        const SDL_GPUTextureCreateInfo *info) {
  if (info->type == SDL_GPU_TEXTURETYPE_2D_ARRAY &&
      info->format == SDL_GPU_TEXTUREFORMAT_R32_FLOAT) {
    HeightLayers.push_back(info->layer_count_or_depth);
    if (RefuseHeightTexture) {
      RefuseHeightTexture = false;
      SDL_SetError("injected height atlas allocation failure");
      return nullptr;
    }
  }
  static const auto original =
      reinterpret_cast<decltype(&SDL_CreateGPUTexture)>(dlsym(RTLD_NEXT, "SDL_CreateGPUTexture"));
  assert(original != nullptr);
  return original(device, info);
}

namespace {
using namespace outshine;

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
  for (size_t at = 4; at + 3 < pixels.size(); at += 4) {
    for (size_t channel = 0; channel < 3; ++channel) {
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
    declaration.Fill = 0.6;
    declaration.KeyLux = 10000;
    declaration.KeyElevationDeg = 90;
    declaration.IndirectLight = {1, 1, 1};
    declaration.Haze = 0;
    Render::SceneRenderer renderer;
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
    CHECK(HeightLayers.empty(), "a scene without terrain allocates no height texture");
    const std::vector<float> nodes(Render::GroundLattice::kPageNodes, 3);
    const auto first = renderer.PlaceHeightPage(nodes);
    CHECK(first && HeightLayers == std::vector<uint32_t>{1}, "the first page allocates one layer");
    if (!first) { return Report(); }
    auto eye = *Render::Viewpoint::LookAt({.EyeM = {{0, 7, 8}}, .AimM = {{0, 3, 0}}}, 0.0);
    eye.YfovRad = 1;
    eye.ZNearM = 0.1;
    eye.ZFarM = 100;
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
    CHECK(HeightLayers.size() == 1, "only live page demand allocates texture layers");
    RefuseHeightTexture = true;
    const auto refused = renderer.PlaceHeightPage(nodes);
    CHECK(!refused &&
              refused.error().find("injected height atlas allocation failure") != std::string::npos,
          "failed growth returns the allocation error");
    std::vector<float> afterFailure;
    CHECK(ReadPageImage(renderer, *scene, *first, afterFailure, error) && afterFailure == baseline,
          "failed growth preserves the old page handle and its GPU samples");
    const auto next = renderer.PlaceHeightPage(nodes);
    CHECK(next && next->Slot == Render::GroundLattice::kPagesPerLayer && HeightLayers.back() == 2,
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
