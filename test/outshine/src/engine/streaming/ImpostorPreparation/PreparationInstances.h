#ifndef OUTSHINE_TEST_PREPARATIONINSTANCES_H
#define OUTSHINE_TEST_PREPARATIONINSTANCES_H
#include "PreparationCommon.h"
#include "PreparationOptical.h"
#include "ImpostorInstances.h"
#include "VegetationStreaming.h"
#include "FrameCapture.h"
#include "RuntimeScene.h"
#include "math/Units.h"
#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <print>
#include <thread>

namespace outshine::Test::CrownFixture {
constexpr double kInstanceFarExtents = 15;
constexpr uint64_t kTreeBody = 17;
constexpr uint64_t kAbsentBody = 18;

struct InstanceScene {
  Render::SceneRenderer Renderer;
  std::unique_ptr<Core::RuntimeScene> Live;
  std::string Error;
};

inline bool OpenInstances(InstanceScene &scene, const Content::ImpostorAtlas &atlas) {
  const auto card = Render::BuildImpostorSurface(atlas, 0);
  CHECK(card.has_value(), "crown piece fixture starts from the native card reference");
  if (!card) { return false; }
  Core::Declaration declaration;
  declaration.InitialGeometry = &*card;
  declaration.SurfaceWidthPx = kInstanceExtent.WidthPx;
  declaration.SurfaceHeightPx = kInstanceExtent.HeightPx;
  declaration.Outputs = {"sceneLinear", "sceneDepth", "sceneShadingNormal"};
  declaration.KeyLux = kLightLux;
  declaration.KeyBearingDeg = kBearings[0];
  declaration.KeyElevationDeg = kElevationDeg;
  CHECK(Core::RuntimeScene::Open(scene.Renderer, declaration, nullptr, scene.Live, scene.Error),
        "crown piece Live opens with the reference lighting");
  if (!scene.Live) { return false; }
  Render::SubjectMesh empty;
  empty.Anchor = {{kWgs84A, 0, 0}};
  CHECK(scene.Renderer.SetSubjectMesh(empty, scene.Error),
        "reference mesh leaves while the Live coordinate anchor remains");
  return true;
}

inline bool Screenshot(InstanceScene &scene, const std::string &path) {
  return Core::SaveFrame(scene.Renderer,
                         {.WidthPx = kInstanceExtent.WidthPx, .HeightPx = kInstanceExtent.HeightPx},
                         path,
                         scene.Error);
}

inline std::array<Mat4, 2> Models(const Content::ImpostorAtlas &atlas, Vec3 toward) {
  const Vec3 right{{toward[2], 0, -toward[0]}};
  std::array<Mat4, 2> models{};
  models[0][0] = models[0][10] = -1;
  for (size_t instance = 0; instance < models.size(); ++instance) {
    const Vec3 shift = atlas.CentreM() + right * ((instance == 0 ? -2 : 2) * atlas.HalfExtentM()) -
                       models[instance].TransformPoint(atlas.CentreM());
    for (size_t axis = 0; axis < 3; ++axis) { models[instance][12 + axis] = shift[axis]; }
  }
  return models;
}

struct Attachments {
  std::vector<float> Depth;
  std::vector<float> Normals;
};

inline bool ReadAttachments(InstanceScene &scene, Attachments &data) {
  const bool ready = scene.Renderer.ReadDepth(data.Depth) == Render::ReadState::Ready &&
                     scene.Renderer.ReadShadingNormal(data.Normals) == Render::ReadState::Ready &&
                     data.Depth.size() == size_t{kInstanceExtent.WidthPx} * kPixels &&
                     data.Normals.size() == size_t{kInstanceExtent.WidthPx} * kPixels * kChannels;
  CHECK(ready, "shared crown attachments are complete");
  return ready;
}

struct PixelLocation {
  size_t X;
  size_t Y;
  size_t Instance;
};

struct PixelResult {
  bool Mismatch = false;
  double NormalError = 0;
};

inline PixelResult InspectPixel(const Content::ImpostorAtlas &atlas,
                                size_t view,
                                const Attachments &data,
                                const Mat4 &model,
                                PixelLocation at) {
  const size_t source =
      (view + (at.Instance == 0 ? atlas.Views().size() / 2 : 0)) % atlas.Views().size();
  const auto &texel = atlas.Views()[source].Texels[at.Y * kPixels + at.X];
  const size_t pixel = at.Y * kInstanceExtent.WidthPx + at.X + at.Instance * 2 * kPixels;
  PixelResult result{.Mismatch = (data.Depth[pixel] > 0) != (texel.Surface > 0)};
  if (data.Depth[pixel] <= 0 || texel.Surface == 0) { return result; }
  const Vec3 expected =
      model.TransformDirection({{texel.Normal[0], texel.Normal[1], texel.Normal[2]}});
  const Vec3 actual{{data.Normals[pixel * kChannels],
                     data.Normals[pixel * kChannels + 1],
                     data.Normals[pixel * kChannels + 2]}};
  result.NormalError = NormalDifference(actual, expected);
  return result;
}

inline void CheckInstancePixels(const Content::ImpostorAtlas &atlas,
                                size_t view,
                                const Attachments &data,
                                const std::array<Mat4, 2> &models) {
  size_t mismatches = 0;
  size_t middle = 0;
  double normalError = 0;
  for (size_t y = 0; y < kPixels; ++y) {
    for (size_t x = 0; x < kPixels; ++x) {
      middle += static_cast<size_t>(data.Depth[y * kInstanceExtent.WidthPx + x + kPixels] > 0);
      for (size_t instance = 0; instance < models.size(); ++instance) {
        const auto pixel = InspectPixel(
            atlas, view, data, models[instance], {.X = x, .Y = y, .Instance = instance});
        mismatches += static_cast<size_t>(pixel.Mismatch);
        normalError = std::max(normalError, pixel.NormalError);
      }
    }
  }
  std::println("crown pieces view {} coverage mismatch={} normal error={} middle={}",
               view,
               mismatches,
               normalError,
               middle);
  CHECK(mismatches == 0 && middle == 0,
        "selected crown views retain source coverage with no unplaced prototype in the gap");
  CHECK(normalError <= 4 * std::sqrt(3.0) / kByteSteps,
        "shared tangent frames preserve rotated captured crown normals");
}

inline void CheckInstanceViews(InstanceScene &scene,
                               const Content::ImpostorAtlas &atlas,
                               Render::ImpostorInstances &crowns) {
  const double extent = atlas.HalfExtentM();
  for (size_t view = 0; view < atlas.Views().size(); ++view) {
    const Vec3 toward = atlas.Views()[view].TowardEye;
    auto camera = Render::Viewpoint::LookAt(
        {.EyeM = atlas.CentreM() + toward * (9 * extent), .AimM = atlas.CentreM()}, 0.0);
    CHECK(camera.has_value(), "shared crown camera has a valid basis");
    if (!camera) { continue; }
    camera->Kind = Render::CameraKind::Orthographic;
    camera->XMagM = 3 * extent;
    camera->YMagM = extent;
    camera->ZNearM = extent;
    camera->ZFarM = kInstanceFarExtents * extent;
    scene.Live->Eye(*camera);
    const auto models = Models(atlas, toward);
    CHECK(crowns.Update(models, camera->EyeM, scene.Error) && scene.Live->Draw(scene.Error),
          "crown view selection draws rotated and translated instances");
    scene.Renderer.WaitForGpu();
    Attachments data;
    if (!ReadAttachments(scene, data)) { continue; }
    CheckInstancePixels(atlas, view, data, models);
    CHECK(scene.Renderer.PieceTriangles() == 2 * atlas.Views().size(),
          "view switching retains the same prototype triangle count");
    CHECK(Screenshot(scene, OutputDirectory() + "/pieces-" + std::to_string(view) + ".png"),
          "shared crown PNG is written");
    CHECK(crowns.Update({}, camera->EyeM, scene.Error) && scene.Live->Draw(scene.Error),
          "empty instance groups deactivate every view");
    scene.Renderer.WaitForGpu();
    CHECK(scene.Renderer.ReadDepth(data.Depth) == Render::ReadState::Ready &&
              std::ranges::all_of(data.Depth, [](const float value) { return value == 0; }),
          "empty crown groups draw no remaining instances");
  }
}

inline uint32_t FindBirch(Generators::Shipping &catalogue, const TreeSpecies &species) {
  uint32_t birch = 0;
  while (const auto *tree = catalogue.TreeFor(Generators::ClusterId{birch})) {
    if (tree->Definition() == species.Definition()) { break; }
    ++birch;
  }
  CHECK(catalogue.TreeFor(Generators::ClusterId{birch}) != nullptr,
        "the loaded atlas resolves to its actual catalogue cluster");
  return birch;
}

inline void CheckWorldCoverage(InstanceScene &scene, const Content::ImpostorAtlas &atlas) {
  std::vector<float> depth;
  CHECK(scene.Renderer.ReadDepth(depth) == Render::ReadState::Ready &&
            depth.size() == size_t{kInstanceExtent.WidthPx} * kPixels,
        "world crown depth is complete");
  size_t mismatch = 0;
  if (depth.size() == size_t{kInstanceExtent.WidthPx} * kPixels) {
    for (size_t y = 0; y < kPixels; ++y) {
      for (size_t x = 0; x < static_cast<size_t>(kInstanceExtent.WidthPx); ++x) {
        const bool expected =
            x >= kPixels && x < 2 * size_t{kPixels} &&
            atlas.Views()[0].Texels[y * size_t{kPixels} + x - kPixels].Surface > 0;
        mismatch += static_cast<size_t>((depth[y * kInstanceExtent.WidthPx + x] > 0) != expected);
      }
    }
  }
  CHECK(mismatch == 0,
        "world consumer preserves the captured crown coverage at its geographic root");
  CHECK(Screenshot(scene, OutputDirectory() + "/world.png"), "world crown consumer PNG is written");
  CHECK(scene.Renderer.PieceTriangles() == 2 * kViews,
        "one geographic tree retains only the four shared view prototypes");
}

inline void CheckMissing(InstanceScene &scene,
                         Tasks &worker,
                         Generators::Shipping &catalogue,
                         std::span<const WorldInstance> placements,
                         const TangentFrame &frame,
                         VegetationStreaming::Config config) {
  config.Shape.Views = kViews + 1;
  const auto began = std::chrono::steady_clock::now();
  {
    auto absent = VegetationStreaming::Create(
        scene.Renderer, worker, catalogue, placements, frame, config, scene.Error);
    CHECK(absent != nullptr, "an absent capture shape can request its artifact");
    if (absent) {
      for (size_t attempt = 0; attempt < kMissingPolls; ++attempt) {
        CHECK(
            absent->Step({}, false, VegetationStreaming::ResourcePublication::Allowed, scene.Error),
            "realtime polling defers a missing crown instead of baking it");
        std::this_thread::sleep_for(kPollInterval);
      }
      CHECK(!absent->Ready() && absent->Resident() == 0 && scene.Renderer.PiecesStanding() == 0,
            "missing crown remains explicitly nonresident");
    }
  }
  CHECK(std::chrono::steady_clock::now() - began < std::chrono::seconds(1),
        "missing crown destruction drains IO without waiting for an unrequested expensive bake");
}

inline void CheckWorld(InstanceScene &scene,
                       const Content::ImpostorAtlas &atlas,
                       const TreeSpecies &species,
                       Tasks &worker) {
  outshine::Ground::GroundMaterials materials;
  outshine::Ground::VegetationTemplates vegetation;
  Generators::Shipping catalogue;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials) &&
            catalogue.EnsureCatalogue(vegetation, "src/assets/world/species", scene.Error),
        "world crown catalogue loads through the production species path");
  const auto birch = FindBirch(catalogue, species);
  const std::array<WorldInstance, 2> placements{
      {{.Body = kTreeBody, .Cluster = birch, .Where = {}},
       {.Body = kAbsentBody, .Cluster = ~0u, .Where = {}}}};
  const VegetationStreaming::Config config{.Cache = {.Directory = OutputDirectory() + "/cache"},
                                           .Shape = {.Pixels = kPixels, .Views = kViews}};
  const auto frame = TangentFrame::At({});
  auto world = VegetationStreaming::Create(
      scene.Renderer, worker, catalogue, placements, frame, config, scene.Error);
  CHECK(world && world->Wanted() == 1 && !world->Ready(),
        "only tree clusters request resident crown prototypes");
  if (world) {
    const double extent = atlas.HalfExtentM();
    auto camera = Render::Viewpoint::LookAt(
        {.EyeM = atlas.CentreM() + Vec3{{0, 0, 9 * extent}}, .AimM = atlas.CentreM()}, 0.0);
    CHECK(camera.has_value(), "world crown camera has a valid basis");
    if (camera) {
      camera->Kind = Render::CameraKind::Orthographic;
      camera->XMagM = 3 * extent;
      camera->YMagM = extent;
      camera->ZNearM = extent;
      camera->ZFarM = kInstanceFarExtents * extent;
      scene.Live->Eye(*camera);
      bool stepped = true;
      const auto began = std::chrono::steady_clock::now();
      while (!world->Ready() && std::chrono::steady_clock::now() - began < kLoadTimeout) {
        if (!world->Step(camera->EyeM,
                         false,
                         VegetationStreaming::ResourcePublication::Allowed,
                         scene.Error)) {
          stepped = false;
          break;
        }
        std::this_thread::sleep_for(kPollInterval);
      }
      CHECK(stepped && world->Ready() && world->Resident() == 1,
            "world crown consumer loads the matching artifact without preparation");
      CHECK(scene.Live->Draw(scene.Error), "geographically placed resident crown draws");
      scene.Renderer.WaitForGpu();
      CheckWorldCoverage(scene, atlas);
    }
  }
  world.reset();
  CHECK(scene.Renderer.PiecesStanding() == 0,
        "world crown destruction releases all resident prototypes");
  CheckMissing(scene, worker, catalogue, placements, frame, config);
}

inline void
CheckInstances(const Content::ImpostorAtlas &atlas, const TreeSpecies &species, Tasks &worker) {
  InstanceScene scene;
  if (!OpenInstances(scene, atlas)) { return; }
  CHECK(!Render::ImpostorInstances::Create(scene.Renderer, atlas, 0, scene.Error),
        "crown capacity must be positive");
  auto crowns = Render::ImpostorInstances::Create(scene.Renderer, atlas, 2, scene.Error);
  CHECK(crowns && scene.Renderer.PieceTriangles() == 2 * atlas.Views().size(),
        "one two-triangle prototype stands per captured view");
  if (!crowns) { return; }
  const std::array<Mat4, 3> excessive{};
  CHECK(!crowns->Update(excessive, {}, scene.Error),
        "crown grouping refuses more instances than its capacity");
  scene.Error.clear();
  CheckInstanceViews(scene, atlas, *crowns);
  crowns.reset();
  CHECK(scene.Renderer.PiecesStanding() == 0,
        "destroying the crown owner releases every view prototype");
  CheckWorld(scene, atlas, species, worker);
}
}
#endif
