#include "ImpostorSurface.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <optional>
#include <print>
#include <string>
#include <vector>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

struct View {
  Vec3 Toward, Right, Up;
  Vec3f Normal;
};

constexpr size_t kCardByteLimit = size_t{1024} * 1024;
constexpr size_t kFramePixels = size_t{64} * 64;

struct NativeSamples {
  std::vector<float> Depth;
  std::vector<float> Normal;
};

void CheckNativeSamples(const NativeSamples &samples,
                        const View &view,
                        Render::CameraKind kind,
                        bool optical) {
  const bool perspective = kind == Render::CameraKind::Perspective;
  const float farDepth = perspective ? 1.5f / 19.0f : 12.0f / 19.0f;
  const float nearDepth = perspective ? 4.0f / 19.0f : 16.0f / 19.0f;
  std::array<size_t, 2> layers{};
  size_t invalidDepths = 0;
  size_t invalidNormals = 0;
  for (size_t at = 0; at < samples.Depth.size(); ++at) {
    if (samples.Depth[at] == 0) { continue; }
    const bool far = std::abs(samples.Depth[at] - farDepth) < 1e-5f;
    const bool near = std::abs(samples.Depth[at] - nearDepth) < 1e-5f;
    invalidDepths += static_cast<size_t>(!far && !near);
    layers[0] += static_cast<size_t>(far);
    layers[1] += static_cast<size_t>(near);
    for (size_t axis = 0; axis < 3; ++axis) {
      invalidNormals += static_cast<size_t>(
          !std::isfinite(samples.Normal[at * 4 + axis]) ||
          std::abs(samples.Normal[at * 4 + axis] - view.Normal[axis]) >= 0.025f);
    }
  }
  if (invalidDepths != 0 || invalidNormals != 0) {
    std::println("spatial y={} optical={} invalidDepths={} invalidNormals={}",
                 view.Toward[1],
                 optical,
                 invalidDepths,
                 invalidNormals);
  }
  CHECK(invalidDepths == 0, "reconstructed depths are the two original physical planes");
  CHECK(invalidNormals == 0, "tangent material normal reconstructs the independent world normal");
  CHECK(layers[0] > 500 && layers[1] > 500, "both physical layers remain visible");
}

void CheckView(const View &view, bool optical) {
  Material material;
  if (optical) {
    material.Scattering = ScatteringModel::DiffuseThinSheet;
    material.DiffuseTransmission = {{0.2f, 0.3f, 0.1f}};
  }
  std::vector<ImpostorAtlas::Texel> samples(64);
  for (size_t at = 0; at < samples.size(); ++at) {
    samples[at] = {.Normal = view.Normal, .Depth = at % 8 < 4 ? 0.25f : 0.75f, .Surface = 1};
  }
  std::string error;
  const auto atlas = ImpostorAtlas::Create(
      8,
      {},
      2,
      {material},
      {{.TowardEye = view.Toward, .Texels = std::move(samples), .Materials = {}}},
      error);
  CHECK(atlas, error.c_str());
  if (!atlas) { return; }
  const auto bytes = atlas->Encode("spatial-view-1", error);
  const auto restored =
      bytes ? ImpostorAtlas::Decode(*bytes, "spatial-view-1", error) : std::nullopt;
  CHECK(restored && restored->Views()[0].TowardEye == view.Toward,
        "atlas retains spatial directions");
  if (!restored) { return; }
  const auto cards = Render::PrepareImpostorCards(*restored, error);
  CHECK(cards && cards->Views[0].Direction == view.Toward, error.c_str());
  if (!cards) { return; }
  auto depthSurface =
      Render::BuildImpostorSurface(*restored, 0, Render::ImpostorSurfaceDetail::Depth);
  CHECK(depthSurface, "native depth patches are built explicitly");
  if (!depthSurface) { return; }
  ImpostorCards depthCards;
  depthCards.Centre = restored->CentreM();
  depthCards.HalfExtentM = restored->HalfExtentM();
  depthCards.Views.push_back({.Direction = view.Toward, .Surface = std::move(*depthSurface)});
  const auto encodedCards = depthCards.Encode("spatial-view-1", kCardByteLimit);
  const auto decodedCards =
      encodedCards ? ImpostorCards::Decode(*encodedCards, "spatial-view-1", kCardByteLimit)
                   : std::nullopt;
  CHECK(decodedCards && decodedCards->Views[0].Direction == view.Toward,
        "cached cards retain spatial directions");
  if (!decodedCards) { return; }
  Core::Declaration declaration;
  declaration.InitialGeometry = &decodedCards->Views[0].Surface;
  declaration.KeyLux = 100;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 64;
  declaration.Stages = {"opaqueGeometry"};
  declaration.Outputs = {"sceneDepth", "sceneShadingNormal"};
  Render::SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), error.c_str());
  if (!scene) { return; }
  Render::Viewpoint camera;
  camera.Kind = Render::CameraKind::Orthographic;
  camera.EyeM = view.Toward * 6.0;
  camera.Forward = view.Toward * -1.0;
  camera.Right = view.Right;
  camera.Up = view.Up;
  camera.XMagM = camera.YMagM = 3;
  camera.ZNearM = 1;
  camera.ZFarM = 20;
  camera.YfovRad = std::numbers::pi / 4;
  for (const auto kind : {Render::CameraKind::Orthographic, Render::CameraKind::Perspective}) {
    camera.Kind = kind;
    scene->Eye(camera);
    CHECK(scene->Draw(error), error.c_str());
    NativeSamples readback;
    const bool ready = renderer.ReadDepth(readback.Depth) == Render::ReadState::Ready &&
                       renderer.ReadShadingNormal(readback.Normal) == Render::ReadState::Ready;
    CHECK(ready && readback.Depth.size() == kFramePixels &&
              readback.Normal.size() == readback.Depth.size() * 4,
          "actual native depth and shading normal attachments are available");
    if (!ready || readback.Normal.size() != readback.Depth.size() * 4) { return; }
    CheckNativeSamples(readback, view, kind, optical);
  }
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  constexpr double diagonal = 1 / std::numbers::sqrt3;
  constexpr double right = 1 / std::numbers::sqrt2;
  const std::array views{
      View{.Toward = {{0, 1, 0}},
           .Right = {{1, 0, 0}},
           .Up = {{0, 0, -1}},
           .Normal = {{1.0f / 3, 2.0f / 3, 2.0f / 3}}},
      View{.Toward = {{0, -1, 0}},
           .Right = {{1, 0, 0}},
           .Up = {{0, 0, 1}},
           .Normal = {{1.0f / 3, -2.0f / 3, 2.0f / 3}}},
      View{.Toward = {{diagonal, diagonal, diagonal}},
           .Right = {{right, 0, -right}},
           .Up = {{-1 / std::sqrt(6.0), 2 / std::sqrt(6.0), -1 / std::sqrt(6.0)}},
           .Normal = {{2.0f / 3, 1.0f / 3, 2.0f / 3}}}};
  for (const auto &view : views) {
    for (const bool optical : {false, true}) { CheckView(view, optical); }
  }
  SDL_Quit();
  return Report();
}
