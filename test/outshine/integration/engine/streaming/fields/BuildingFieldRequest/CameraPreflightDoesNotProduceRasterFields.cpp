#include "test/harness/render/BuildingFieldSources.h"
#include "test/harness/engine/NativeBuildingSources.h"
#include "BuildingFieldRequest.h"
#include "AssetPipeline.h"
#include "AssetCache.h"
#include "Check.h"
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Test::BuildingSources;
constexpr size_t kMiB = size_t{1024} * 1024;

std::expected<std::shared_ptr<const BuildingFieldRequest::Product>, std::string>
AwaitMetadata(BuildingFieldRequest &request) {
  const auto deadline = std::chrono::steady_clock::now() + kDeadline;
  do {
    auto ready = request.PollMetadata();
    if (!ready || *ready) { return ready; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < deadline);
  return std::unexpected("building camera preflight timed out");
}

void Exercise(const Fixture &fixture, const BuildingSourceDemand::Source &source) {
  auto opened = AssetCache::Open((fixture.Directory.Path / "camera-preflight.sqlite").string());
  CHECK(opened, "the LOD candidate uses a fresh asset store");
  if (!opened) { return; }
  const std::shared_ptr<AssetCache> cache(std::move(*opened));
  Tasks workers(1);
  AssetPipeline pipeline(workers,
                         {.JobsMost = 2,
                          .ConsumersMost = 1,
                          .TicketsMost = 8,
                          .InputBytesMost = 8 * kMiB,
                          .ReceiptBytesMost = 16384});
  BuildingFieldRequest::Demand demand{.Directions = {{{0, 0, -1}}}, .Pixels = 32, .View = {0}};
  const BuildingFieldRequest::Budget budget{
      .ProducerBytes = kMiB,
      .ResidentBytes = kMiB,
      .FieldBytesMost = kMiB,
      .RasterBytesMost = kMiB,
      .Capture = {.Samples = 1024, .Sources = 100, .Faces = 1000, .TriangleAttempts = 100000}};
  const auto assets = fixture.Stack.BuildingAssets();
  auto openedConsumer = pipeline.OpenConsumer({.Identity = cache});
  CHECK(openedConsumer, "one streaming component owns one request scope");
  if (!openedConsumer) { return; }
  const auto consumer = std::make_shared<AssetPipeline::Consumer>(std::move(*openedConsumer));
  const auto before = assets->Costs();
  auto description = BuildingFieldRequest::Start(
      pipeline, workers, cache, assets, source, demand, budget, {}, consumer);
  CHECK(description, "the candidate's descriptor is requested before replacement");
  if (!description) { return; }
  const auto metadata = AwaitMetadata(*description);
  CHECK(metadata && *metadata && (**metadata).Metadata && !(**metadata).Field,
        "metadata preparation does not start raster generation");
  if (!metadata || !*metadata) { return; }
  const auto &shape = (**metadata).Metadata->Metadata();
  for (const double distance : {240000., shape.HalfExtentM}) {
    const auto camera = Render::Viewpoint::LookAt(
        {.EyeM = shape.Centre + shape.Directions[0] * distance, .AimM = shape.Centre},
        Vec3{{0, 1, 0}});
    CHECK(camera, "approaching the building has a valid camera");
    if (!camera) { return; }
    demand.GeometricView = {
        .Camera = *camera,
        .Optics = {.WidePx = 1280, .HighPx = 720, .FovDeg = 55, .NearM = .1f, .FarM = 1000000},
        .GeometryErrorMostPx = 1,
        .Domain = BuildingFieldRequest::ViewDomain::Panoramic};
    auto request = BuildingFieldRequest::Start(
        pipeline, workers, cache, assets, source, demand, budget, {}, consumer);
    CHECK(request, "each camera position preflights the same persisted candidate");
    if (!request) { return; }
    const auto ready = AwaitMetadata(*request);
    if (distance == 240000.) {
      CHECK(ready && *ready && !(**ready).Field && (**ready).GeometricSupport,
            "the distant candidate qualifies before its raster is requested");
      request->Cancel();
      CHECK(!request->Poll(), "cancelling the replacement keeps raster production stopped");
    } else {
      CHECK(!ready && !request->Poll(),
            "the approaching camera cannot bypass the rejected LOD on a later poll");
    }
  }
  AssetQuery query{};
  query.Bounds = source.Bounds;
  const auto records = cache->Select(query);
  CHECK(records && records->size() == 1 && records->front().Kind == "fractional-impostor-views-1",
        "cancelled or unsuitable replacements leave only small metadata, no raster or mesh");
  const auto after = assets->Costs();
  CHECK(after.SurfaceReadBytes == before.SurfaceReadBytes &&
            after.GeometryPreparations == before.GeometryPreparations,
        "camera preflight never decodes fine source forms or generates original geometry");
}
}

int main() {
  Fixture fixture;
  const bool prepared = fixture.Open() && fixture.Generate().has_value();
  CHECK(prepared, "native source assets are available");
  if (!prepared) { return Report(); }
  BuildingSourceDemand sources;
  CHECK(WaitFor(sources, fixture, BuildingSourceDemand::Phase::Ready),
        "the regular World descriptor loads");
  const auto entries = sources.Entries();
  if (entries.size() == 1) {
    const auto &source = entries[0].Value;
    if (source) { Exercise(fixture, *source); }
  }
  return Report();
}
