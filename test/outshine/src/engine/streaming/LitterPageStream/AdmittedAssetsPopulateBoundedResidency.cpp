#include "LitterPageStream.h"
#include "LitterPageNoise.h"
#include "ReadbackTransfer.h"
#include "Tasks.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <semaphore>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr FieldPageAddress kA{.X = -65, .Y = -64, .Mip = 4};
constexpr FieldPageAddress kB{.X = 6, .Y = 11, .Mip = 3};
constexpr FieldPageAddress kC{.X = 7, .Y = 12, .Mip = 2};

std::shared_ptr<AssetCache> OpenCache(const std::string &path) {
  auto cache = AssetCache::Open(path);
  CHECK(cache, "the native asset store opens");
  if (!cache) { Failed(); }
  return {std::move(*cache)};
}

R8PageAtlas Atlas(Native::Device &device) {
  const R8PageAtlasOptions options{
      .Layout = {.CoreTexels = 64, .Columns = 2, .Entries = 16, .Probes = 16},
      .Slots = 2,
      .EditsPerBatch = 2,
      .PendingBatches = 2};
  auto atlas =
      Require(R8PageAtlas::Create(device, options, Require(R8PageAtlasStorageFor(options))));
  if (const auto last = atlas.LastUpload()) {
    CHECK(Require(device.Wait(*last, std::chrono::seconds(5))) == Native::CompletionStatus::Ready,
          "initial atlas publication completes before the controlled stream test");
  }
  Require(atlas.Collect());
  return atlas;
}

template <class Done> void AdvanceUntil(LitterPageStream &stream, Done done) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!done() && std::chrono::steady_clock::now() < until) {
    Require(stream.Advance());
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(done(), "admitted asynchronous progress completes within the test deadline");
}

void PixelsMatch(Native::Device &device, const R8PageAtlas &atlas, FieldPageAddress address) {
  const auto slot = atlas.Find(address);
  CHECK(slot, "the requested page has a published slot");
  if (!slot) { return; }
  constexpr uint32_t side = 66;
  std::vector<uint8_t> expected(size_t{side} * side);
  CHECK(Generators::GenerateLitterPage(
            {.Tile = {address.X, address.Y}, .Level = address.Mip, .CoreTexels = 64}, expected),
        "the independent upload expectation uses the selected spatial mip");
  auto transfer = Require(ReadbackTransfer::Start(
      device, atlas.Image(), {.X = *slot * side, .Width = side, .Height = side}));
  const auto bytes = Require(transfer.Wait(device, std::chrono::seconds(5)));
  CHECK(bytes, "native texture readback completes");
  if (!bytes) { return; }
  bool same = true;
  for (uint32_t row = 0; row < side; ++row) {
    same = same &&
           std::ranges::equal(bytes->subspan(transfer.RowBytes() * row, side),
                              std::as_bytes(std::span(expected).subspan(size_t{row} * side, side)));
  }
  CHECK(same, "native asset payloads reach the GPU without address, row or mip changes");
}

void ColdAndResidency(Native::Device &device, const std::shared_ptr<AssetCache> &cache) {
  Tasks workers(1);
  AssetPipeline pipeline(workers,
                         {.JobsMost = 2,
                          .ConsumersMost = 2,
                          .TicketsMost = 2,
                          .InputBytesMost = 8u << 20u,
                          .ReceiptBytesMost = 8192});
  auto atlas = Atlas(device);
  auto stream = Require(
      LitterPageStream::Create(pipeline, cache, atlas, {.DemandMost = 4, .PendingMost = 2}));
  std::binary_semaphore occupied(0);
  std::binary_semaphore release(0);
  const auto blocker = workers.Post([&] {
    occupied.release();
    release.acquire();
  });
  CHECK(blocker != Tasks::kNoTask, "a worker blocker is admitted");
  const bool blocked = occupied.try_acquire_for(std::chrono::seconds(5));
  CHECK(blocked, "the worker is held before camera feedback arrives");
  if (!blocked) {
    release.release();
    return;
  }
  Require(stream->Observe(std::array{kA, kB, kA}));
  Require(stream->Advance());
  CHECK(stream->Counts().Wanted == 2 && stream->Counts().Pending == 2 &&
            stream->Counts().Started == 2 && !atlas.Find(kA) && !atlas.Find(kB),
        "duplicate demand starts only bounded tickets and never blocks on unavailable workers");
  const auto bad = stream->Observe(std::array{FieldPageAddress{.Mip = 6}});
  CHECK(!bad && stream->Counts().Wanted == 2,
        "invalid feedback leaves the last valid demand intact");
  const std::array<FieldPageAddress, 5> oversized{};
  CHECK(!stream->Observe(oversized) && stream->Counts().Wanted == 2,
        "oversized feedback is rejected before replacing the bounded working set");
  release.release();
  CHECK(workers.AwaitCompletion(blocker, 5), "the blocked worker is released");
  (void)workers.TakeCompletion(blocker);
  AdvanceUntil(*stream,
               [&] { return atlas.Find(kA) && atlas.Find(kB) && stream->Counts().Pending == 0; });
  CHECK(stream->Counts().Published == 2 && stream->Counts().CacheHits == 0,
        "cold pages are generated and published once through the native asset pipeline");
  PixelsMatch(device, atlas, kA);
  PixelsMatch(device, atlas, kB);
  Require(stream->Observe(std::array{kA, kC}));
  AdvanceUntil(*stream, [&] { return atlas.Find(kC) && stream->Counts().Pending == 0; });
  CHECK(atlas.Find(kA) && !atlas.Find(kB),
        "replacement preserves a page used by current feedback and retires the older page");
  PixelsMatch(device, atlas, kC);
  Require(stream->Observe(std::array{kA, kC, kB}));
  const auto deferred = stream->Counts().Deferred;
  AdvanceUntil(*stream, [&] { return stream->Counts().Deferred > deferred; });
  CHECK(atlas.Find(kA) && atlas.Find(kC) && !atlas.Find(kB) && stream->Counts().Pending == 1,
        "a full current working set defers optional pages without evicting visible residents");
  Require(stream->Observe(std::array{kB}));
  AdvanceUntil(*stream, [&] { return atlas.Find(kB) && stream->Counts().Pending == 0; });
  CHECK(stream->Counts().CacheHits == 1 && stream->Counts().Started == 4 &&
            stream->Counts().BookkeepingBytes < (64u << 10u),
        "evicted native assets reload from the DB with bounded owner metadata");
  PixelsMatch(device, atlas, kB);
}

void FreshOwner(Native::Device &device, const std::string &path) {
  const auto cache = OpenCache(path);
  Tasks workers(1);
  AssetPipeline pipeline(workers,
                         {.JobsMost = 2,
                          .ConsumersMost = 1,
                          .TicketsMost = 2,
                          .InputBytesMost = 8u << 20u,
                          .ReceiptBytesMost = 8192});
  auto atlas = Atlas(device);
  auto stream = Require(
      LitterPageStream::Create(pipeline, cache, atlas, {.DemandMost = 4, .PendingMost = 2}));
  Require(stream->Observe(std::array{kA, kC}));
  AdvanceUntil(*stream, [&] { return stream->Counts().Published == 2; });
  CHECK(stream->Counts().CacheHits == 2 && stream->Counts().Pending == 0,
        "a fresh store owner and empty GPU residency reuse the persisted native products");
  PixelsMatch(device, atlas, kA);
  PixelsMatch(device, atlas, kC);
}
}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-tests.litter-stream-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  CHECK(!error, "the private asset directory opens");
  if (error) { return Report(); }
  const auto path = (directory / "assets.sqlite").string();
  {
    auto device = Require(Native::CreateDevice({.Validation = true}));
    {
      const auto cache = OpenCache(path);
      ColdAndResidency(*device, cache);
    }
    FreshOwner(*device, path);
  }
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "private native products are removed after their last consumer");
  return Report();
}
