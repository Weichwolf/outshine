#include "Check.h"
#include "RawSourceCache.h"
#include "SourceCacheTrim.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Data;
constexpr auto kPayloadKey = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr auto kMissingKey = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
constexpr auto kTruncatedKey = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
constexpr auto kAmbiguousKey = "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd";

void CheckRecords(const std::filesystem::path &root) {
  RawSourceCache store({.Directory = root.string()});
  CHECK(store.Lookup(kPayloadKey).Where == RawSourceCache::Presence::Unknown,
        "a missing payload remains unknown");
  CHECK(store.KeepEmpty(kPayloadKey), "explicitly complete zero bytes have an owned record");
  const auto empty = store.Lookup(kPayloadKey);
  CHECK(empty.Where == RawSourceCache::Presence::Bytes && empty.Bytes.empty(),
        "a complete empty payload is neither missing nor source absence");
  CHECK(store.KeepAbsent(kMissingKey) &&
            store.Lookup(kMissingKey).Where == RawSourceCache::Presence::Absent,
        "the existing confirmed-absence record keeps its meaning");
  CHECK(store.KeepEmpty(kMissingKey) &&
            store.Lookup(kMissingKey).Where == RawSourceCache::Presence::Bytes,
        "an explicitly stored empty payload replaces an absence marker without inventing bytes");
  { const std::ofstream truncated(root / kTruncatedKey, std::ios::binary); }
  CHECK(store.Lookup(kTruncatedKey).Where == RawSourceCache::Presence::Unknown,
        "a zero-length raw file cannot impersonate a verified empty payload");
  CHECK(store.KeepEmpty(kAmbiguousKey), "a second explicit empty record is available");
  { const std::ofstream truncated(root / kAmbiguousKey, std::ios::binary); }
  CHECK(store.Lookup(kAmbiguousKey).Where == RawSourceCache::Presence::Unknown,
        "a conflicting unreadable raw file cannot fall through to an old status record");
  const std::array<uint8_t, 2> bytes{1, 2};
  CHECK(store.Keep(kPayloadKey, bytes.data(), bytes.size()),
        "an explicit nonempty write clears the old empty status");
  CHECK(store.Lookup(kPayloadKey).Bytes == std::vector<uint8_t>(bytes.begin(), bytes.end()),
        "the current positive payload remains exact");
  CHECK(store.KeepEmpty(kPayloadKey), "a marker may coexist without deleting owned payload bytes");
  CHECK(store.Lookup(kPayloadKey).Bytes == std::vector<uint8_t>(bytes.begin(), bytes.end()),
        "existing complete bytes take precedence over a conflicting status");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  const auto root = std::filesystem::path(PlantedPath("verified-empty-source"));
  std::error_code error;
  std::filesystem::create_directories(root, error);
  CHECK(!error, "the isolated source cache exists");
  CheckRecords(root);
  {
    const RawSourceCache reopened({.Directory = root.string()});
    const auto empty = reopened.Lookup(kMissingKey);
    CHECK(empty.Where == RawSourceCache::Presence::Bytes && empty.Bytes.empty(),
          "a fresh owner restores the complete empty input without network access");
  }
  const auto trimmed =
      TrimSourceCache(root, {.BudgetBytes = std::numeric_limits<uint64_t>::max(), .Apply = true});
  CHECK(trimmed && trimmed->EntriesRemoved == 0,
        "cache maintenance retains shared payload/status keys without duplicate index rows");
  std::filesystem::remove_all(root, error);
  CHECK(!error, "only the closed test-owned cache is removed");
  return Report();
}
