#include "AssetCache.h"
#include "AssetPackageCodec.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

void CheckStorage(size_t size, bool compressible) {
  std::vector<uint8_t> input(size, 23);
  if (!compressible) {
    uint32_t state = 0x9e3779b9;
    for (auto &byte : input) {
      state ^= state << 13u;
      state ^= state >> 17u;
      state ^= state << 5u;
      byte = static_cast<uint8_t>(state);
    }
  }
  BeginCppAllocationProbe();
  const auto plan = AssetCache::PublicationBufferBytes(size);
  const auto scan = EndCppAllocationProbe();
  CHECK(plan && scan.Calls == 0, "publication workspace is planned without allocating a codec");
  if (!plan) { return; }
  BeginCppAllocationProbe();
  auto compressed = AssetSql::Compress(input);
  const auto allocation = EndCppAllocationProbe();
  CHECK(compressed && allocation.Bytes <= *plan,
        "all output and static Zstd workspace allocations fit their declared reservation");
  if (!compressed) { return; }
  if (size < 64) {
    CHECK(compressed->empty() && allocation.Calls == 0,
          "tiny native packages retain their uncompressed zero-workspace path");
    return;
  }
  CHECK(allocation.Calls == 2, "compression owns one aligned workspace and one output buffer");
  if (!compressible) {
    CHECK(compressed->empty(), "incompressible bytes keep the original owned package");
    return;
  }
  CHECK(!compressed->empty(), "repeated bytes produce a compressed native package");
  const auto stored = compressed->size();
  BeginCppAllocationProbe();
  const auto readPlan = AssetSql::DecompressionBytes(stored, 1, size);
  const auto readPlanning = EndCppAllocationProbe();
  CHECK(readPlan && readPlanning.Calls == 0,
        "decompression plans stored bytes, native bytes and context without allocations");
  if (!readPlan) { return; }
  BeginCppAllocationProbe();
  const auto restored = AssetSql::Decompress(std::move(*compressed), 1, size);
  const auto readAllocation = EndCppAllocationProbe();
  CHECK(readAllocation.Calls == 2 && readAllocation.Bytes == *readPlan - stored,
        "decompression allocates exactly its aligned static context and native output");
  CHECK(restored && std::ranges::equal(*restored, input),
        "static-workspace compression preserves the complete original bytes");
}
}

int main() {
  for (const size_t size : std::array<size_t, 10>{
           0, 63, 64, 300, 1024, 16384, 65536, 131072, 1048576, size_t{64} * 1024 * 1024}) {
    CheckStorage(size, true);
    CheckStorage(size, false);
  }
  BeginCppAllocationProbe();
  const auto overflow = AssetCache::PublicationBufferBytes(std::numeric_limits<size_t>::max());
  const auto rejected = EndCppAllocationProbe();
  CHECK(!overflow && overflow.error() == AssetCacheError::CapacityExceeded && rejected.Calls == 0,
        "unrepresentable codec reservations fail before allocating workspace or output");
  BeginCppAllocationProbe();
  const auto readOverflow = AssetSql::DecompressionBytes(1, 1, std::numeric_limits<size_t>::max());
  const auto invalid = AssetSql::DecompressionBytes(8, 0, 9);
  const auto unknown = AssetSql::DecompressionBytes(8, 2, 16);
  const auto malformed = EndCppAllocationProbe();
  CHECK(!readOverflow && readOverflow.error() == AssetCacheError::CapacityExceeded && !invalid &&
            invalid.error() == AssetCacheError::InvalidInput && !unknown &&
            unknown.error() == AssetCacheError::InvalidInput && malformed.Calls == 0,
        "overflow and inconsistent codec metadata fail before allocating read buffers");
  return Report();
}
