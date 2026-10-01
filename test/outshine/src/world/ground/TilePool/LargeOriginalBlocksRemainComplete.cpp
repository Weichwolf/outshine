#include "Check.h"
#include "NativeCog.h"
#include "ContentStore.h"
#include "CopernicusDem.h"
#include "SourceSet.h"
#include "TilePool.h"
#include "TileGeodesy.h"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Ground;

class OriginalBlocks final : public Transport {
public:
  explicit OriginalBlocks(Test::NativeCogProfile profile)
      : Bytes_(Test::NativeCog({47, 10}, false, profile)) {}

  FetchStart Begin(const std::string &) override {
    return std::unexpected(FetchFailureReason::InvalidRequest);
  }

  FetchStart Begin(const std::string &url, ByteRange range, std::string_view tag) override {
    const std::scoped_lock lock(Mutex_);
    CHECK(url.find("Copernicus_DSM_COG_10_N47_00_E010_00_DEM") != std::string::npos &&
              (tag.empty() || tag == "\"original-native\""),
          "built-in provider uses the original cell and pins subsequent ranges");
    LargestRange.store(std::max(LargestRange.load(), range.Length));
    const Ticket ticket{static_cast<uint64_t>(++Begins)};
    Requests_.emplace(ticket, range);
    return ticket;
  }

  Wire Collect(Ticket ticket) override {
    const std::scoped_lock lock(Mutex_);
    const auto found = Requests_.find(ticket);
    if (found == Requests_.end()) { return Wire::Working(); }
    const auto range = found->second;
    Requests_.erase(found);
    if (range.First > Bytes_.size() || range.Length > Bytes_.size() - range.First) {
      return Wire::Answered(416, {});
    }
    return Wire::Answered(
        std::vector<uint8_t>(Bytes_.begin() + static_cast<ptrdiff_t>(range.First),
                             Bytes_.begin() + static_cast<ptrdiff_t>(range.First + range.Length)),
        {.Bytes = range, .TotalBytes = Bytes_.size(), .EntityTag = "\"original-native\""});
  }

  void Cancel(Ticket ticket) override {
    const std::scoped_lock lock(Mutex_);
    Requests_.erase(ticket);
  }

  std::atomic<int> Begins{0};
  std::atomic<uint64_t> LargestRange{0};

private:
  std::mutex Mutex_;
  std::map<Ticket, ByteRange> Requests_;
  const std::vector<uint8_t> Bytes_;
};

TilePool::Reply Field(TilePool &pool,
                      TileId at,
                      std::shared_ptr<const TerrainField> &field,
                      std::optional<FetchFailure> &failure) {
  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  auto state = TilePool::Reply::Pending;
  while (std::chrono::steady_clock::now() < end) {
    state = pool.Field(at, &field, &failure);
    if (state != TilePool::Reply::Pending && state != TilePool::Reply::Deferred) { break; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return state;
}
}

int main() {
  using namespace outshine::Test;
  for (const auto profile :
       {NativeCogProfile::LargeBlock, NativeCogProfile::LargeBlockOverlapsHeader}) {
    ContentStore store({.Using = ContentStore::Use::Off});
    SourceSet sources(store);
    CHECK(sources.Add(std::make_unique<CopernicusDem>()) == SourceSet::Registration::Accepted,
          "large original native fixture registers through the public provider contract");
    OriginalBlocks http(profile);
    TilePool pool({.ByteBudget = 8u * 1024 * 1024, .DecodedBytes = 8u * 1024 * 1024, .Carriers = 1},
                  sources,
                  http);
    const TileId at{.Zoom = 14, .X = 8652, .Y = 5745};
    std::shared_ptr<const TerrainField> field;
    std::optional<FetchFailure> failure;
    CHECK(Field(pool, at, field, failure) == TilePool::Reply::Ready && field && !failure,
          "terrain accepts a valid block larger than libtiff's individual reads");
    CHECK(http.LargestRange > 1024u * 1024,
          "acquisition batches the complete missing encoded block interval");
    if (!field) { continue; }
    double largest = 0;
    for (uint32_t row = 0; row < field->Rows(); ++row) {
      for (uint32_t col = 0; col < field->Cols(); ++col) {
        const auto geo = TileFracToGeo({.X = at.X + static_cast<double>(col) / 128,
                                        .Y = at.Y + static_cast<double>(row) / 128},
                                       at.Zoom);
        largest = std::max(largest,
                           std::abs(static_cast<double>(field->AtM(row, col)) -
                                    NativeHeight(geo.LatitudeDeg, geo.LongitudeDeg)));
      }
    }
    CHECK(largest < 0.0001,
          "complete block decoding retains the independently defined geographic plane");
    const int before = http.Begins;
    std::shared_ptr<const TerrainField> warm;
    CHECK(Field(pool, at, warm, failure) == TilePool::Reply::Ready && warm && http.Begins == before,
          "resident decoded terrain starts no additional source IO");
  }
  return Report();
}
