#include "Check.h"
#include "RoadGeometryPublication.h"
#include "RoadPageAsset.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"
#include "test/harness/render/NativeUploadFaultDevice.h"
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
using namespace outshine::Test::Gpu;

struct Fixture {
  UploadFailure Upload;
  Render::SceneRenderer Renderer{{.Context = &Upload, .Create = NativeUploadFaultDevice::Create}};
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

void CheckBackpressure(Fixture &fixture) {
  const auto first = fixture.Page({.Index = 0}, -1);
  auto second = fixture.Page({.Index = 1}, 1);
  auto &mesh = second.Geometry.Mesh;
  constexpr size_t vertices = 600'000;
  mesh.PositionM.resize(vertices * 3);
  mesh.NormalM.resize(vertices * 3);
  mesh.ColourRgba.resize(vertices * 4, 1);
  auto publication = RoadGeometryPublication::Begin(
      fixture.Renderer, fixture.Pages, fixture.Manifest, TangentFrame::At(fixture.Manifest.Anchor));
  CHECK(publication, "the two-page route registers");
  if (!publication) { return; }
  const auto until = std::chrono::steady_clock::time_point::max();
  CHECK((*publication)->Advance(first, fixture.Manifest.Pages[0], until).value_or(false),
        "the first road page has staged crossings to submit");
  const auto accepted = fixture.Pages.Handles();
  auto &fault = fixture.Upload;
  fault.Point = UploadFailurePoint::Acquire;
  fault.Kind = Render::Native::ErrorKind::Busy;
  fault.Armed = true;
  const auto deferred = (*publication)->Advance(second, fixture.Manifest.Pages[1], until);
  CHECK(deferred && !*deferred && fault.Rejected == 1 && fixture.Pages.Handles() == accepted,
        "a full GPU queue keeps the packed page pending and preserves the accepted one");
  fault.Kind = Render::Native::ErrorKind::Driver;
  fault.Armed = true;
  const auto failed =
      (*publication)->Advance(second, fixture.Manifest.Pages[1], std::chrono::steady_clock::now());
  CHECK(!failed && failed.error() == fault.Message && fault.Rejected == 2,
        "driver failures remain errors on the same upload path");
  const auto resumed =
      (*publication)->Advance(second, fixture.Manifest.Pages[1], std::chrono::steady_clock::now());
  CHECK(resumed && *resumed && (*publication)->Finish() && fixture.Pages.Handles().size() == 2,
        "the already packed page uploads once and the complete route publishes");
}

void CheckPublication(Fixture &fixture) {
  const auto left = fixture.Page({.Index = 0}, -1);
  const auto right = fixture.Page({.Index = 1}, 1);
  const auto empty =
      fixture.Page({.Kind = Generators::RoadPagePartKind::Junction, .Index = 0}, 0, true);
  auto publication = RoadGeometryPublication::Begin(
      fixture.Renderer, fixture.Pages, fixture.Manifest, TangentFrame::At(fixture.Manifest.Anchor));
  CHECK(publication, "road-native materials and source domain register");
  if (!publication) { return; }
  const auto &leftRef = fixture.Manifest.Pages[0];
  const auto &rightRef = fixture.Manifest.Pages[1];
  const auto &emptyRef = fixture.Manifest.Pages[2];
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  const auto deferred = (*publication)->Advance(left, leftRef, std::chrono::steady_clock::now());
  CHECK(deferred && !*deferred && fixture.Pages.Handles().empty(),
        "packing yields without publishing a partial page");
  CHECK(!(*publication)->Advance(right, rightRef, until),
        "a pending page cannot be overwritten by another product");
  const auto accepted = (*publication)->Advance(left, leftRef, until);
  CHECK(accepted && *accepted && !(*publication)->Finish(),
        "one ready page cannot stand in for the complete source cut");
  const auto second = (*publication)->Advance(right, rightRef, until);
  const auto knownEmpty = (*publication)->Advance(empty, emptyRef, until);
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
                                   fixture.Manifest.Pages[0],
                                   std::chrono::steady_clock::now() + std::chrono::seconds(5));
  CHECK(ready && *ready && (*publication)->Finish(), "the translated native source activates");
  CHECK(fixture.Depth(10) > 0 && fixture.Depth(16) == 0,
        "source ENU geometry uses its explicit world transform instead of ECEF-offset placement");
}

void CheckRegionCrossing(Fixture &fixture) {
  const auto shared = fixture.Page({.Index = 0}, -1);
  const auto retired = fixture.Page({.Index = 1}, 1);
  auto &sharedRef = fixture.Manifest.Pages[0];
  sharedRef.ShapeKey = shared.Address.PlanKey;
  sharedRef.Sources = {240};
  const auto frame = TangentFrame::At(fixture.Manifest.Anchor);
  const auto until = std::chrono::steady_clock::time_point::max();
  auto first =
      RoadGeometryPublication::Begin(fixture.Renderer, fixture.Pages, fixture.Manifest, frame);
  CHECK(first, "the initial region starts loading its roads");
  if (!first) { return; }
  CHECK((*first)->Advance(shared, sharedRef, until).value_or(false) &&
            (*first)->Advance(retired, fixture.Manifest.Pages[1], until).value_or(false) &&
            (*first)->Finish(),
        "both roads are initially rendered");
  first->reset();

  Core::WorldCandidate candidate(fixture.Renderer);
  auto nextPages = fixture.Pages;
  const auto prepared =
      candidate.Prepare(*fixture.Scene, nullptr, Render::SceneResources::PieceSources::Reuse);
  CHECK(prepared, "the region crossing starts an independent world candidate");
  if (!prepared) { return; }
  fixture.Manifest.PlanKey.assign(64, 'c');
  sharedRef.Parts[0].Index = 7;
  fixture.Manifest.Pages.pop_back();
  const auto added = fixture.Page({.Index = 9}, 2);
  auto next = RoadGeometryPublication::Begin(fixture.Renderer, nextPages, fixture.Manifest, frame);
  CHECK(next, "roads can be rebound to the next manifest");
  if (!next) { return; }
  const auto reads = (*next)->PagesToLoad();
  CHECK(reads.size() == 1 && reads.front() == 1 && !(*next)->Finish(),
        "only the new road is loaded and the shared road cannot publish by itself");
  CHECK(fixture.Depth(12) > 0 && fixture.Depth(20) > 0 && fixture.Depth(24) == 0,
        "the published region keeps both roads while the next region loads");
  CHECK((*next)->Advance(added, fixture.Manifest.Pages[1], until).value_or(false) &&
            (*next)->Finish() && candidate.Publish(fixture.Scene),
        "the next manifest publishes its complete source coverage");
  CHECK(fixture.Depth(12) > 0 && fixture.Depth(20) == 0 && fixture.Depth(24) > 0,
        "the crossing retains the shared road, replaces the old road and draws the new road");
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
  {
    Fixture fixture;
    CHECK(fixture.Open(), "crossing fixture opens");
    if (fixture.Scene) { CheckRegionCrossing(fixture); }
  }
  {
    Fixture fixture;
    CHECK(fixture.Open(), "backpressure fixture opens");
    if (fixture.Scene) { CheckBackpressure(fixture); }
  }
  SDL_Quit();
  return Report();
}
