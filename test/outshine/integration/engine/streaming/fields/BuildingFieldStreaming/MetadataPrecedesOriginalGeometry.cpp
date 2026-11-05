#include "test/outshine/src/engine/streaming/StructureBuildQueue/BuildingQueueFixture.h"
#include "BuildingFieldStreaming.h"
#include "AssetPipeline.h"
#include "GeometryPageResidency.h"
#include "SceneRenderer.h"
#include "Lens.h"
#include "Check.h"
#include "LogSinks.h"
#include "Log.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <array>
#include <cstddef>
#include <optional>
#include <print>
#include <string>
#include <span>
#include <thread>

namespace {
using namespace outshine;
using namespace outshine::Test;

bool Collect(BuildingQueueFixture &fixture, StructureBuildQueue::HeightRequirement heights) {
  auto &prints = fixture.Stack.Footprints();
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    (void)fixture.Queue.Posts(fixture.Stack,
                              prints,
                              fixture.Eye,
                              fixture.Source,
                              1,
                              heights,
                              std::nullopt,
                              StructureBuildQueue::BuildPurpose::TerrainContacts);
    auto ready = fixture.Queue.NextLandings(fixture.Stack,
                                            prints,
                                            fixture.Eye,
                                            fixture.Source,
                                            1,
                                            heights,
                                            std::nullopt,
                                            StructureBuildQueue::BuildPurpose::TerrainContacts);
    CHECK(ready, "contact-only preparation has a terminal result");
    if (!ready) { return false; }
    if (!ready->empty()) { fixture.Queue.CommitsLandings(prints, *ready); }
    if (!ready->empty() || StructureBuildQueue::QualifiedSourceKey(prints, 0)) { return true; }
    (void)fixture.Queue.AwaitSlice(0.02);
  } while (std::chrono::steady_clock::now() < deadline);
  return false;
}

void PublishWithoutOriginals(BuildingFieldStreaming &fields,
                             BuildingFieldStreaming::Context &context) {
  context.Renderer.SetCamera(
      context.Camera,
      {.WidePx = 32, .HighPx = 32, .FovDeg = 57.29578f, .NearM = .1f, .FarM = 240000});
  const auto before = context.Surface.BuildingAssets()->Costs();
  std::string error;
  CHECK(fields.Advance(context, error) && error.empty(),
        "a known empty shadow demand permits a complete field-only source cut");
  const auto source = StructureBuildQueue::QualifiedSourceKey(context.Surface.Footprints(), 0);
  CHECK(source && fields.ProductsReady({.Tile = 0, .Key = *source}) &&
            fields.OriginalCells({.Tile = 0, .Key = *source}) == 0,
        "a qualified field completes the source without requesting original cells");
  CHECK(fields.Read().Active == 1 && fields.Read().FieldOnlyCells == 1,
        "field publication records the cell needing no original product");
  const auto after = context.Surface.BuildingAssets()->Costs();
  CHECK(after.GeometryReadBytes == before.GeometryReadBytes &&
            after.GeometryPreparations == before.GeometryPreparations,
        "the complete field-only cut performs no original payload IO or generation");
}

void PointCamera(Render::Viewpoint &camera) {
  camera.EyeM = {{0, 30, 1000}};
  camera.Forward = {{0, 0, 1}};
  camera.Right = {{-1, 0, 0}};
  camera.YfovRad = 1;
  camera.ZNearM = .1;
  camera.ZFarM = 240000;
}

void PrepareWithoutOriginals(BuildingFieldStreaming &fields,
                             BuildingFieldStreaming::Context &context,
                             size_t expectedGroups) {
  const auto plan = Render::ExecutionPlan::Compile(
      {.Outputs = {Render::Resource::SceneLinear, Render::Resource::SceneDepth},
       .Content = {Render::Stage::OpaqueGeometry},
       .Exposure = {},
       .Display = {},
       .Precision = {}});
  CHECK(plan, "early field preparation has a native execution plan");
  if (!plan) { return; }
  const auto initialized = context.Renderer.Init({.WidthPx = 32, .HeightPx = 32}, *plan);
  CHECK(initialized, "early field preparation has a native target");
  if (!initialized) { return; }
  const auto offline = context.Renderer.OfflineUploads(std::chrono::seconds(5));
  context.Pages.BindRenderer(&context.Renderer);
  context.Pages.SetFrame(context.Frame);
  PointCamera(context.Camera);
  const auto before = context.Surface.BuildingAssets()->Costs();
  std::string error;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  do {
    CHECK(fields.Advance(context, error), "field preparation proceeds without original pages");
    if (!error.empty() || fields.Read().Ready != 0 || fields.Read().Rejected != 0) { break; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < deadline);
  const auto after = context.Surface.BuildingAssets()->Costs();
  const auto counts = fields.Read();
  if (!error.empty() || counts.Ready != 1 || counts.Pending != 0 || counts.Rejected != 0) {
    std::println("early fields: ready={} pending={} rejected={} error={}",
                 counts.Ready,
                 counts.Pending,
                 counts.Rejected,
                 error);
  }
  CHECK(error.empty() && counts.Ready == 1 && counts.Pending == 0 && counts.Rejected == 0,
        "a field behind the camera is ready before any original geometry is loaded");
  CHECK(counts.ReadyGroups == expectedGroups,
        "an ineligible coarse field descends to its complete eligible subgroup family");
  CHECK(counts.Active == 0 && counts.SelectedProducts == 0,
        "preparation alone cannot publish an incomplete main or shadow source cut");
  CHECK(after.GeometryReadBytes == before.GeometryReadBytes &&
            after.GeometryPreparations == before.GeometryPreparations,
        "early field preparation never loads or generates the original render product");
  size_t visible = 0;
  context.Pages.ForEachDigest([&](const auto &) { ++visible; });
  CHECK(visible == 0, "a prepared field stays unpublished without fallback coverage");
  PublishWithoutOriginals(fields, context);
  context.Pages.Clear();
}

int Exercise(bool subgroups) {
  using namespace outshine;
  using namespace outshine::Test;
  BuildingQueueFixture fixture;
  std::array buildings{fixture.Buildings[0], fixture.Buildings[0]};
  for (size_t at = 1; at < buildings[1].LatLon.size(); at += 2) {
    buildings[1].LatLon[at] -= .0006;
  }
  if (!fixture.Open(std::span(buildings).first(subgroups ? 2 : 1),
                    (fixture.Cache.Path / "assets").string())) {
    return Report();
  }
  const bool initial = Collect(fixture, StructureBuildQueue::HeightRequirement::AllowFallback);
  CHECK(initial, "initial native contacts complete without render pages");
  if (!initial) { return Report(); }
  fixture.Stack.Footprints().BeginRefinement();
  const bool qualified = Collect(fixture, StructureBuildQueue::HeightRequirement::FineOnly);
  CHECK(qualified && StructureBuildQueue::QualifiedSourceKey(fixture.Stack.Footprints(), 0),
        "native source contacts qualify without any render page");
  if (!qualified) { return Report(); }
  Render::SceneRenderer renderer;
  Render::GeometryPageResidency pages;
  Tasks workers(1);
  AssetPipeline pipeline(workers,
                         {.JobsMost = 2,
                          .ConsumersMost = 2,
                          .TicketsMost = 4,
                          .InputBytesMost = size_t{8} * 1024 * 1024,
                          .ReceiptBytesMost = 16384});
  BuildingFieldStreaming fields;
  BuildingFieldStreaming::Context context{.Workers = workers,
                                          .Pipeline = pipeline,
                                          .Surface = fixture.Stack,
                                          .Sources = fixture.Queue,
                                          .Pages = pages,
                                          .Renderer = renderer,
                                          .Camera = {},
                                          .Frame = TangentFrame::At(fixture.Eye)};
  if (subgroups) { PointCamera(context.Camera); }
  const auto before = fixture.Stack.BuildingAssets()->Costs();
  std::string error;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    CHECK(fields.Advance(context, error), "metadata discovery needs no renderer initialization");
    if (!error.empty() || fields.Read().Sources != 0) { break; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < deadline);
  const auto after = fixture.Stack.BuildingAssets()->Costs();
  const auto counts = fields.Read();
  CHECK(counts.Sources == 1 && counts.Pending == 0,
        "source hierarchy selection completes before original geometry exists");
  CHECK(counts.Ready == 0 && counts.Active == 0 && pipeline.Counts().Jobs == 0,
        "an unconfigured view admits neither field generation nor publication");
  CHECK(after.GeometryPreparations == before.GeometryPreparations &&
            after.SurfacePreparations == before.SurfacePreparations &&
            after.GeometryReadBytes == before.GeometryReadBytes &&
            after.SurfaceReadBytes == before.SurfaceReadBytes,
        "early discovery does not generate render meshes or decode surface forms");
  PrepareWithoutOriginals(fields, context, subgroups ? 2 : 1);
  return Report();
}
}

int main() {
  outshine::TextLogSink sink{outshine::TextTarget(outshine::TextStream::Stderr)};
  const outshine::LogThreadSinkScope logging(&sink);
  CHECK(SDL_Init(SDL_INIT_VIDEO), "early field preparation opens the native video backend");
  const int single = Exercise(false);
  const int result = Exercise(true) | single;
  SDL_Quit();
  return result;
}
