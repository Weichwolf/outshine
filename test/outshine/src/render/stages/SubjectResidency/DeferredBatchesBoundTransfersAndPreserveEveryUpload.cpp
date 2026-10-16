#include "Check.h"
#include "ReadbackTransfer.h"
#include "SubjectResidency.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOffline.h"
#include <chrono>
#include <utility>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  using namespace outshine::Render::Native;
  using namespace outshine::Test::Gpu;
  {
    auto device = Require(CreateDevice({.Validation = true}));
    SubjectResidency resident;
    Require(resident.StandsOn(device.get()));
    constexpr uint32_t blockBytes = 1024u * 1024u;
    constexpr uint32_t blocks = 64;
    constexpr uint32_t totalBytes = blockBytes * blocks;
    constexpr auto stream = SubjectResidency::Stream::Vertex;
    CHECK(resident.Grow(stream, {.Usage = BufferUse::Vertex, .Bytes = totalBytes}),
          "all destination ranges have storage before deferred writes");
    std::vector<uint32_t> source(blockBytes / sizeof(uint32_t));
    uint64_t peak = 0;
    for (uint32_t block = 0; block < blocks; ++block) {
      std::ranges::fill(source, block + 1);
      SubjectResidency::Crossing crossing{.Which = stream,
                                          .Usage = BufferUse::Vertex,
                                          .From = source.data(),
                                          .Bytes = blockBytes,
                                          .Offset = block * blockBytes};
      CHECK(RetryOffline([&] { return resident.Cross(std::span(&crossing, 1), true); },
                         std::chrono::seconds(5)),
            "each deferred write is accepted");
      peak = std::max(peak, resident.Allocations().TransferBytes);
    }
    CHECK(resident.RecordedCrossings() > 0,
          "large deferred workloads submit work before the final explicit flush");
    CHECK(peak < totalBytes / 2,
          "transfer residency stays below half this workload instead of retaining every source");
    CHECK(RetryOffline([&] { return resident.SubmitPendingUploads(); }, std::chrono::seconds(5)),
          "remaining writes submit");
    auto transfer = Require(RetryOffline(
        [&] {
          return ReadbackTransfer::Start(*device, *resident.Buffer(stream), {.Bytes = totalBytes});
        },
        std::chrono::seconds(5)));
    const auto read = Require(transfer.Wait(*device, std::chrono::seconds(5)));
    CHECK(read, "completed upload batches can be read independently");
    if (read) {
      std::vector<uint32_t> copied(totalBytes / sizeof(uint32_t));
      std::memcpy(copied.data(), read->data(), totalBytes);
      const auto *words = copied.data();
      bool identical = true;
      for (uint32_t at = 0; at < totalBytes / sizeof(uint32_t); ++at) {
        identical &= words[at] == at / source.size() + 1;
      }
      CHECK(identical, "every word of every source survives batching and transfer reuse");
    }
  }
  return Report();
}
