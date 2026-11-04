#include "PageIndexAsset.h"
#include "Check.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

void Exercise(const std::string &path) {
  auto cache = AssetCache::Open(path);
  CHECK(cache, "the independent metadata store opens");
  if (!cache) { return; }
  const auto parent = Sha256Hex("independent-index", 17);
  const std::array bounds{Box{.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                          Box{.Min = {{10, 0, 0}}, .Max = {{11, 1, 1}}}};
  const auto size = PageIndexAsset::EncodedBytes(bounds.size());
  CHECK(size && *size == 16 + 3 * 64 + 2 * 4, "the exact bound follows the encoded tree layout");
  if (!size) { return; }
  CHECK(!ResolvePageIndex(**cache, parent, bounds, *size - 1),
        "an insufficient reservation refuses tree construction before publication");
  auto created = ResolvePageIndex(**cache, parent, bounds, *size);
  CHECK(created && created->ByteCount == *size, "the exact reservation admits one tree");
  if (!created) { return; }
  auto hit = LoadPageIndex(**cache, parent, *size);
  CHECK(hit && *hit && (**hit).From == PageIndexAsset::Origin::Stored &&
            (**hit).Hierarchy.SourceOrder().size() == bounds.size(),
        "a tree restores without the original bounds or page payloads");
  const auto tooSmall = LoadPageIndex(**cache, parent, *size - 1);
  CHECK(!tooSmall && tooSmall.error() == AssetCacheError::CapacityExceeded,
        "stored metadata remains subject to the caller's read reservation");
  const auto stored = (*cache)->Load(created->Key, *size);
  CHECK(stored && *stored, "the encoded native tree remains accessible for corruption probes");
  if (!stored || !*stored) { return; }
  const auto record = (**stored).Record();
  const auto original = (**stored).Bytes();
  const auto reject = [&](std::vector<uint8_t> bytes, AssetRecord metadata) {
    metadata.Package = Sha256Hex(bytes.data(), bytes.size());
    CHECK((*cache)->Publish(std::span(&metadata, 1), bytes),
          "the malformed native fixture has a valid storage checksum");
    const auto invalid = LoadPageIndex(**cache, parent, *size);
    CHECK(!invalid && invalid.error() == AssetCacheError::Storage,
          "a native-format violation never becomes a usable hierarchy");
  };
  std::vector<uint8_t> duplicate(original.begin(), original.end());
  constexpr size_t order = 16 + 3 * 64;
  std::copy_n(duplicate.begin() + order, 4, duplicate.begin() + order + 4);
  reject(std::move(duplicate), record);
  std::vector<uint8_t> outside(original.begin(), original.end());
  constexpr size_t firstLeafOrdinal = 16 + 64 + 6 * sizeof(double);
  std::fill_n(outside.begin() + firstLeafOrdinal, 4, uint8_t{255});
  reject(std::move(outside), record);
  auto wrongBounds = record;
  wrongBounds.Bounds.Min[0] -= 1;
  reject({original.begin(), original.end()}, std::move(wrongBounds));
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-tests.independent-page-index-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  Exercise((root / "assets.sqlite").string());
  std::filesystem::remove_all(root);
  return outshine::Test::Report();
}
