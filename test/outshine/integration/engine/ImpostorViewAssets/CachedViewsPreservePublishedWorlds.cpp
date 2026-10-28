#include "AssetCache.h"
#include "Check.h"
#include "ImpostorAtlas.h"
#include "ImpostorInstances.h"
#include "ImpostorSurface.h"
#include "ImpostorViewAssets.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Content;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr size_t kBytesMost = size_t{1024} * 1024;
constexpr std::string_view kSource = "fixed-original-group/1";

std::optional<ImpostorAtlas> Source() {
  ImpostorAtlas::View view{.TowardEye = {{0, 0, 1}}, .Texels = {}, .Materials = {}};
  view.Texels.resize(64);
  for (size_t at = 24; at < 40; ++at) {
    view.Texels[at] = {.Normal = {{0, 0, 1}}, .Depth = .5f, .Surface = 1};
  }
  Material material;
  material.BaseColour = {{.8f, .4f, .2f, 1}};
  std::string error;
  return ImpostorAtlas::Create(8, {}, 1, {material}, {std::move(view)}, error);
}

std::vector<float> Read(SceneRenderer &renderer) {
  const auto published = renderer.PublishedWorld();
  CHECK(renderer.RenderFrame(), "the published frame submits while a replacement is prepared");
  renderer.WaitForGpu();
  std::vector<float> image;
  CHECK(renderer.ReadSceneLinear(image) == ReadState::Ready && image.size() == size_t{64} * 64 * 4,
        "the actual published linear image remains readable");
  return image;
}

bool Lit(const std::vector<float> &image) {
  double energy = 0;
  for (size_t at = 0; at < image.size(); ++at) {
    if (!std::isfinite(image[at])) { return false; }
    if (at % 4 != 3) { energy += image[at]; }
  }
  return energy > 0;
}

void Abandon(SceneRenderer &renderer,
             const Core::RuntimeScene &scene,
             ImpostorInstances &parent,
             const ImpostorCards &cards,
             const std::vector<float> &before) {
  std::string error;
  Core::WorldCandidate candidate(renderer);
  CHECK(candidate.Prepare(scene, nullptr), "a private candidate inherits the existing parent");
  if (!renderer.HasWorldCandidate()) { return; }
  {
    const auto edit = renderer.EditWorldCandidate();
    CHECK(parent.Update({}, {{0, 0, 10}}, error), "only the candidate hides the previous parent");
    const Mat4 model;
    auto next = ImpostorInstances::Create(renderer, cards, 1, error);
    CHECK(next && next->Update(std::span(&model, 1), {{0, 0, 10}}, error),
          "the replacement card is registered in the private candidate");
    CHECK(Read(renderer) == before, "candidate edits do not replace the published parent image");
  }
}

void Exercise(const std::string &path) {
  auto cache = AssetCache::Open(path);
  const auto atlas = Source();
  CHECK(cache && atlas, "source capture and native cache open");
  if (!cache || !atlas) { return; }
  const auto products = ImpostorViewAssets::Publish(
      **cache,
      kSource,
      {.Centre = {}, .HalfExtentM = 1, .Pixels = 8, .Directions = {{{0, 0, 1}}}},
      {.Min = {{-1, -1, -1}}, .Max = {{1, 1, 1}}});
  auto fine = BuildFractionalImpostorSurface(*atlas, 0);
  CHECK(products && fine && products->Publish({.View = 0, .Mip = 0}, std::move(*fine), kBytesMost),
        "the source-independent fine card is stored");
  if (!products) { return; }
  auto first = products->Load({.View = 0, .Mip = 0}, kBytesMost);
  CHECK(first && *first, "the initial product loads as native cached geometry");
  if (!first || !*first) { return; }
  SceneRenderer renderer;
  Core::Declaration declaration;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 64;
  declaration.Outputs = {"sceneLinear", "sceneDepth"};
  declaration.Stages = {"opaqueGeometry", "transmissiveGeometry", "compositeTransmission"};
  declaration.KeyLux = 100;
  declaration.KeyBearingDeg = 180;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        "the normal world and optical composition open");
  if (!scene) { return; }
  Viewpoint eye;
  eye.Kind = CameraKind::Orthographic;
  eye.EyeM = {{0, 0, 10}};
  eye.XMagM = eye.YMagM = 64;
  eye.ZNearM = .1;
  eye.ZFarM = 100;
  scene->Eye(eye);
  CHECK(scene->Draw(error), "the one-pixel projected group has a native camera");
  CHECK(!Lit(Read(renderer)), "the empty world is a genuinely dark negative control");
  auto parent = ImpostorInstances::Create(renderer, **first, 1, error);
  const Mat4 model;
  CHECK(parent && parent->Update(std::span(&model, 1), eye.EyeM, error),
        "the first cached card is an actual world piece");
  if (!parent) { return; }
  const auto before = Read(renderer);
  CHECK(Lit(before), "the positive control contains actual fractional lit source coverage");
  const auto missing = products->Load({.View = 0, .Mip = 3}, kBytesMost);
  CHECK(missing && !*missing && Read(renderer) == before,
        "a missing coarse product leaves the complete parent image untouched");
  auto coarse = BuildFractionalImpostorSurface(*atlas, 0, 3);
  CHECK(coarse && products->Publish({.View = 0, .Mip = 3}, std::move(*coarse), kBytesMost),
        "the requested coarse product becomes independently ready");
  auto next = products->Load({.View = 0, .Mip = 3}, kBytesMost);
  CHECK(next && *next, "the replacement loads without recapturing the source");
  if (!next || !*next) { return; }
  Abandon(renderer, *scene, *parent, **next, before);
  CHECK(Read(renderer) == before, "abandonment preserves all accepted world pixels");
  std::unique_ptr<ImpostorInstances> replacement;
  {
    Core::WorldCandidate candidate(renderer);
    CHECK(candidate.Prepare(*scene, nullptr), "a fresh private world prepares the replacement");
    {
      const auto edit = renderer.EditWorldCandidate();
      CHECK(parent->Update({}, eye.EyeM, error), "the old parent is hidden only in the candidate");
      replacement = ImpostorInstances::Create(renderer, **next, 1, error);
      CHECK(replacement && replacement->Update(std::span(&model, 1), eye.EyeM, error),
            "the selected one-pixel product is staged before publication");
    }
    CHECK(Read(renderer) == before, "staged ready replacement still leaves the old world visible");
    CHECK(candidate.Publish(scene), "the complete replacement world publishes atomically");
  }
  parent.reset();
  scene->Eye(eye);
  CHECK(scene->Draw(error), "the new published world draws with the same camera");
  const auto after = Read(renderer);
  CHECK(Lit(after), "the cached replacement keeps nonempty source coverage after publication");
  CHECK(after == before, "one-pixel mip replacement retains the accepted native colour image");
  std::vector<float> depth;
  CHECK(renderer.ReadDepth(depth) == ReadState::Ready &&
            std::ranges::all_of(depth, [](float value) { return value == 0; }),
        "an optical parent never becomes unqualified opaque world depth");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the native cached world replacement");
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-cached-view-world-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) { Exercise((root / "assets.sqlite").string()); }
  std::filesystem::remove_all(root);
  SDL_Quit();
  return Report();
}
