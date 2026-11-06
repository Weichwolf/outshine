#include "Check.h"
#include "RoadGeometryPublication.h"
#include "RoadPageAsset.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

struct Fixture {
  Render::SceneRenderer Renderer;
  std::unique_ptr<Core::RuntimeScene> Scene;
  Render::GeometryPageResidency Pages;
  Generators::RoadPageManifest Manifest;
  std::string Error;

  bool Open() {
    Geometry base;
    const auto surface = base.addSurface("offscreen", Material{});
    if (!surface) { return false; }
    const auto part = base.addPart("offscreen", *surface);
    if (!part || !base.setPositions(*part, std::array<float, 9>{100, 0, 0, 101, 0, 0, 100, 1, 0}) ||
        !base.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2})) {
      return false;
    }
    Core::Declaration declaration;
    declaration.InitialGeometry = &base;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneDepth"};
    Manifest.PlanKey.assign(64, 'a');
    Manifest.MaterialKey.assign(64, 'b');
    Manifest.Anchor = {.LongitudeDeg = 8, .LatitudeDeg = 49};
    return Manifest.Materials.addSurface("streets", Material{}).has_value() &&
           Core::RuntimeScene::Open(Renderer, declaration, nullptr, Scene, Error);
  }

  Generators::RoadPageAsset Page(Generators::RoadPagePart part, float centre, bool empty = false) {
    Generators::RoadPageAsset page;
    page.Address.PlanKey = Manifest.PlanKey;
    page.Address.MaterialKey = Manifest.MaterialKey;
    page.Address.Anchor = Manifest.Anchor;
    page.Address.Clustered = true;
    page.Address.Parts = {part};
    page.Geometry.Parts = page.Address.Parts;
    if (!empty) {
      page.Geometry.Mesh.PositionM = {
          centre - .6f, -.6f, 0, centre + .6f, -.6f, 0, centre, 1.2f, 0};
      page.Geometry.Mesh.NormalM = {0, 0, 1, 0, 0, 1, 0, 0, 1};
      page.Geometry.Mesh.ColourRgba = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
      page.Geometry.Mesh.Index = {0, 1, 2};
      page.Geometry.Clusters = {{.SelfCenter = {centre, 0, 0},
                                 .SelfRadius = 1.5f,
                                 .ParentRadius = 4,
                                 .ParentErr = kDagRootErr,
                                 .First = 0,
                                 .Count = 3}};
    }
    Manifest.Pages.push_back({.Key = Generators::RoadPageKey(page.Address),
                              .Bounds = {},
                              .Parts = page.Address.Parts,
                              .Clustered = true});
    return page;
  }

  float Depth(size_t x) {
    Render::Viewpoint eye;
    eye.EyeM = {{0, 0, 4}};
    eye.Kind = Render::CameraKind::Orthographic;
    eye.XMagM = 4;
    eye.YMagM = 2;
    eye.ZNearM = .1;
    eye.ZFarM = 10;
    Scene->Eye(eye);
    if (!Scene->Advance(Error) || !Scene->Draw(Error)) { return -1; }
    Renderer.WaitForGpu();
    std::vector<float> depth;
    if (Renderer.ReadDepth(depth) != Render::ReadState::Ready || depth.size() != size_t{32} * 32u) {
      return -1;
    }
    return depth[size_t{16} * 32u + x];
  }
};

void CheckPublication(Fixture &fixture) {
  const auto left = fixture.Page({.Index = 0}, -1);
  const auto right = fixture.Page({.Index = 1}, 1);
  const auto empty =
      fixture.Page({.Kind = Generators::RoadPagePartKind::Junction, .Index = 0}, 0, true);
  auto publication = RoadGeometryPublication::Begin(
      fixture.Renderer, fixture.Pages, fixture.Manifest, TangentFrame::At(fixture.Manifest.Anchor));
  CHECK(publication, "road-native materials and source domain register");
  if (!publication) { return; }
  const auto leftKey = Generators::RoadPageKey(left.Address);
  const auto rightKey = Generators::RoadPageKey(right.Address);
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  const auto deferred = (*publication)->Advance(left, leftKey, std::chrono::steady_clock::now());
  CHECK(deferred && !*deferred && fixture.Pages.Handles().empty(),
        "packing yields without publishing a partial page");
  CHECK(!(*publication)->Advance(right, rightKey, until),
        "a pending page cannot be overwritten by another product");
  const auto accepted = (*publication)->Advance(left, leftKey, until);
  CHECK(accepted && *accepted && !(*publication)->Finish(),
        "one ready page cannot stand in for the complete source cut");
  const auto second = (*publication)->Advance(right, rightKey, until);
  const auto knownEmpty =
      (*publication)->Advance(empty, Generators::RoadPageKey(empty.Address), until);
  CHECK(second && *second && knownEmpty && *knownEmpty && fixture.Pages.Handles().size() == 2,
        "two mesh pages and a known-empty source preserve independent ownership");
  CHECK(fixture.Depth(12) == 0 && fixture.Depth(20) == 0,
        "staged pages stay hidden until the complete cut is activated");
  fixture.Manifest.Materials = {};
  CHECK((*publication)->Finish(), "all exact source partitions activate together");
  publication->reset();
  CHECK(fixture.Depth(12) > 0 && fixture.Depth(20) > 0,
        "native pages draw after their manifest and publication release the material table");
  Core::WorldCandidate candidate(fixture.Renderer);
  const auto prepared = candidate.Prepare(*fixture.Scene, nullptr);
  CHECK(prepared && candidate.Publish(fixture.Scene),
        "candidate publication restores native products");
  fixture.Pages.BindRenderer(&fixture.Renderer);
  CHECK(fixture.Pages.ValidateSources(fixture.Error) && fixture.Depth(12) > 0 &&
            fixture.Depth(20) > 0,
        "page handles, shared placements and material owners survive native scene restoration");
}

void CheckFrame(Fixture &fixture) {
  const auto page = fixture.Page({.Index = 0}, 0);
  auto target = fixture.Manifest.Anchor;
  target.LongitudeDeg += .00002;
  auto publication = RoadGeometryPublication::Begin(
      fixture.Renderer, fixture.Pages, fixture.Manifest, TangentFrame::At(target));
  CHECK(publication, "source-local road coordinates accept another world origin");
  if (!publication) { return; }
  const auto ready = (*publication)
                         ->Advance(page,
                                   Generators::RoadPageKey(page.Address),
                                   std::chrono::steady_clock::now() + std::chrono::seconds(5));
  CHECK(ready && *ready && (*publication)->Finish(), "the translated native source activates");
  CHECK(fixture.Depth(10) > 0 && fixture.Depth(16) == 0,
        "source ENU geometry uses its explicit world transform instead of ECEF-offset placement");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Fixture fixture;
    CHECK(fixture.Open(), "publication fixture opens");
    if (fixture.Scene) { CheckPublication(fixture); }
  }
  {
    Fixture fixture;
    CHECK(fixture.Open(), "frame fixture opens");
    if (fixture.Scene) { CheckFrame(fixture); }
  }
  SDL_Quit();
  return Report();
}
