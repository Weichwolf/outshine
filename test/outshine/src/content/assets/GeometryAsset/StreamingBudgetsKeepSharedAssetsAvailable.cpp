#include "AssetPipeline.h"
#include "BinaryValueArchive.h"
#include "Check.h"
#include "GeometryAssetRequest.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kBytes = 65536;

auto Await(GeometryAssetRequest &request) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  for (;;) {
    auto ready = request.Poll();
    if (!ready || *ready || std::chrono::steady_clock::now() >= until) { return ready; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void RejectFalseMetrics(AssetCache &cache,
                        const GeometryAsset::Description &layout,
                        const std::string &key,
                        const std::string &parent) {
  const auto records = cache.Select({.Bounds = layout.Bounds,
                                     .Kind = "native-geometry-info",
                                     .Level = std::nullopt,
                                     .Radius = std::nullopt,
                                     .Planes = {},
                                     .Parent = parent});
  CHECK(records && records->size() == 1, "the object's descriptor is independently addressable");
  if (!records || records->size() != 1) { return; }
  BinaryValueWriter changed(kBytes);
  CHECK(changed(uint64_t{0x3454455353414547ULL},
                layout.Bounds.Min.Axis,
                layout.Bounds.Max.Axis,
                layout.WorldFromLocal.Column,
                uint32_t{0},
                false,
                uint64_t{3},
                uint64_t{3},
                uint64_t{60},
                uint64_t{0},
                uint64_t{0}),
        "the fixture isolates an understated texture size in otherwise valid metadata");
  auto record = records->front();
  record.Package.clear();
  record.OffsetBytes = 0;
  record.ByteCount = changed.Out.Bytes().size();
  CHECK(cache.Remove(record.Key) && cache.Publish(std::span(&record, 1), changed.Out.Bytes()),
        "a damaged size descriptor can coexist with an intact shared payload");
  const auto loaded = GeometryAsset::Load(cache, key, parent, kBytes, 4096);
  CHECK(!loaded && loaded.error() == AssetCacheError::Storage,
        "understated sizes never turn corrupt cache metadata into a valid game asset");
  GeometryAsset::ContentMetrics overflow;
  overflow.Indices = std::numeric_limits<uint64_t>::max();
  CHECK(!overflow.BufferBytes(), "corrupt element counts cannot wrap the admission size");
}

void Exercise(const std::filesystem::path &root) {
  auto opened = AssetCache::Open((root / "assets.sqlite").string());
  CHECK(opened, "the shared game asset store opens");
  if (!opened) { return; }
  const std::shared_ptr<AssetCache> cache(std::move(*opened));
  Geometry mesh;
  const auto material = mesh.addSurface("wall", Material{});
  CHECK(material, "the object's wall material exists");
  if (!material) { return; }
  const auto part = mesh.addPart("wall", *material);
  const std::vector<uint8_t> image(size_t{64} * 64 * 4, 127);
  const auto imageIndex = mesh.addImage(64, 64, image);
  Material textured;
  if (imageIndex) { textured.BaseColourMap.Image = *imageIndex; }
  CHECK(part && imageIndex && mesh.setSurface(*material, textured) &&
            mesh.setPositions(*part, std::array<float, 9>{0, 0, 0, 1, 0, 0, 0, 1, 0}) &&
            mesh.setTexture(*part, std::array<float, 6>{0, 0, 1, 0, 0, 1}) &&
            mesh.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "a textured object is prepared once");
  if (!part) { return; }
  const std::string key(64, '5');
  const std::string parent(64, '6');
  const GeometryAsset::Description layout{.Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 0}}},
                                          .WorldFromLocal = {}};
  CHECK(GeometryAsset::Publish(*cache, key, parent, layout, mesh, kBytes),
        "the object and its measured content publish together");
  const auto description = GeometryAsset::Describe(*cache, key, parent, kBytes);
  CHECK(description && *description && (**description).Metrics &&
            (**description).Metrics->ImageBytes == image.size() &&
            (**description).Metrics->BufferBytes() == image.size() + 18 * sizeof(float),
        "asset metadata accounts for mesh buffers and its stored texture");
  if (!description || !*description) { return; }
  Tasks workers(1);
  AssetPipeline pipeline(workers,
                         {.JobsMost = 2,
                          .ConsumersMost = 4,
                          .TicketsMost = 4,
                          .InputBytesMost = 4 * kBytes,
                          .ReceiptBytesMost = 16384});
  GeometryAssetRequest::Budget budget{
      .ConsumerBytes = 4096, .EncodedBytesMost = kBytes, .ReceiptBytesMost = 4096};
  auto small = GeometryAssetRequest::Start(pipeline, workers, cache, key, parent, budget);
  budget.ConsumerBytes = kBytes;
  auto large = GeometryAssetRequest::Start(pipeline, workers, cache, key, parent, budget);
  CHECK(small && large, "two streaming consumers can reserve different memory budgets");
  if (!small || !large) { return; }
  const auto refused = Await(*small);
  CHECK(!refused &&
            refused.error() == "geometry buffers exceed consumer admission before payload decode",
        "memory pressure rejects the undersized consumer before decoding its object");
  const auto ready = Await(*large);
  CHECK(ready && *ready && (*ready)->Mesh.images() == 1 &&
            (*ready)->Layout.Metrics == (**description).Metrics,
        "the independent admitted consumer still restores the complete cached object");
  CHECK(cache->Find(key) && *cache->Find(key),
        "one consumer's memory limit never evicts or invalidates the shared asset");
  RejectFalseMetrics(*cache, (**description), key, parent);
}
}

int main() {
  const auto root = PlantedPath("geometry-streaming-budgets");
  Exercise(root);
  std::filesystem::remove_all(root);
  return Report();
}
