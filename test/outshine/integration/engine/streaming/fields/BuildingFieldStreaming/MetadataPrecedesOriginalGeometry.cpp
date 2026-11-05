#include "test/outshine/src/engine/streaming/StructureBuildQueue/BuildingQueueFixture.h"
#include "BuildingFieldStreaming.h"
#include "AssetPipeline.h"
#include "GeometryPageResidency.h"
#include "SceneRenderer.h"
#include "Check.h"
#include "LogSinks.h"
#include "Log.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <cstddef>
#include <optional>
#include <print>
#include <string>
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

void PrepareWithoutOriginals(BuildingFieldStreaming &fields,
                             BuildingFieldStreaming::Context &context) {
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
  context.Camera.EyeM = {{0, 30, 1000}};
  context.Camera.Forward = {{0, 0, 1}};
  context.Camera.Right = {{-1, 0, 0}};
  context.Camera.YfovRad = 1;
  context.Camera.ZNearM = .1;
  context.Camera.ZFarM = 240000;
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
  CHECK(counts.Active == 0 && counts.SelectedProducts == 0,
        "preparation alone cannot publish an incomplete main or shadow source cut");
  CHECK(after.GeometryReadBytes == before.GeometryReadBytes &&
            after.GeometryPreparations == before.GeometryPreparations,
        "early field preparation never loads or generates the original render product");
  size_t visible = 0;
  context.Pages.ForEachDigest([&](const auto &) { ++visible; });
  CHECK(visible == 0, "a prepared field stays unpublished without fallback coverage");
  context.Pages.Clear();
}

int Exercise() {
  using namespace outshine;
  using namespace outshine::Test;
  BuildingQueueFixture fixture;
  if (!fixture.Open(fixture.Buildings, (fixture.Cache.Path / "assets").string())) {
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
  PrepareWithoutOriginals(fields, context);
  return Report();
}
}

int main() {
  outshine::TextLogSink sink{outshine::TextTarget(outshine::TextStream::Stderr)};
  const outshine::LogThreadSinkScope logging(&sink);
  CHECK(SDL_Init(SDL_INIT_VIDEO), "early field preparation opens the native video backend");
  const int result = Exercise();
  SDL_Quit();
  return result;
}
