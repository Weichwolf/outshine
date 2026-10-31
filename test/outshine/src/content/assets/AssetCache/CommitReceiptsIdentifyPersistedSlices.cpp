#include "AssetGeneration.h"
#include "AssetCache.h"
#include "Sha256.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace {
using namespace outshine;

std::string Key(const char *name) {
  return Sha256Hex(name);
}

GeneratedAssetPackage Package() {
  return {.Records = {{.Key = Key("first"),
                       .Kind = "first",
                       .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                       .Package = {},
                       .OffsetBytes = 0,
                       .ByteCount = 3,
                       .Parent = {},
                       .RequestKey = Key("request")},
                      {.Key = Key("second"),
                       .Kind = "second",
                       .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                       .Package = {},
                       .OffsetBytes = 3,
                       .ByteCount = 3,
                       .Parent = {},
                       .RequestKey = {}}},
          .Bytes = {1, 2, 3, 4, 5, 6}};
}

void CheckReceipt(const std::string &path) {
  using namespace outshine::Test;
  auto opened = AssetCache::Open(path);
  CHECK(opened.has_value(), "the existing native asset store opens");
  if (!opened) { return; }
  const auto package = Package();
  const auto missing = CommitAsset(**opened, Key("missing"), package.Bytes.size(), package);
  CHECK(!missing && !(*opened)->Find(Key("first"))->has_value(),
        "a commit cannot publish unrelated records without the demanded target");
  const auto over = CommitAsset(**opened, Key("second"), package.Bytes.size() - 1, package);
  CHECK(!over && !(*opened)->Find(Key("first"))->has_value(),
        "whole-package admission is checked before publication");
  const auto receipt = CommitAsset(**opened, Key("second"), package.Bytes.size(), package);
  CHECK(receipt && receipt->Key == Key("second") && receipt->Kind == "second" &&
            receipt->Package == Sha256Hex(package.Bytes.data(), package.Bytes.size()) &&
            receipt->OffsetBytes == 3 && receipt->ByteCount == 3 && !receipt->RequestKey,
        "the receipt identifies the committed normalized record and its actual package slice");
  CHECK(package.Records[0].Package.empty() && package.Records[1].Package.empty(),
        "normalization does not mutate or return the caller's uncommitted record");
  const auto alias = (*opened)->FindRequest(Key("request"));
  CHECK(alias && *alias && (**alias).Key == Key("first") && (**alias).RequestKey == Key("request"),
        "the same atomic commit preserves durable request bindings");
  const auto firstBytes = (*opened)->PlanRead(Key("first"), package.Bytes.size());
  const auto secondBytes = (*opened)->PlanRead(Key("second"), package.Bytes.size());
  CHECK(firstBytes && *firstBytes && (**firstBytes).PackageBytesMost == package.Bytes.size() &&
            (**firstBytes).BufferBytesMost == package.Bytes.size() && secondBytes && *secondBytes &&
            (**secondBytes).PackageBytesMost == package.Bytes.size(),
        "metadata admission measures the whole shared package rather than either slice");
  if (!firstBytes || !*firstBytes || !secondBytes || !*secondBytes) { return; }
  const auto small = (*opened)->PlanRead(Key("second"), 3);
  const auto absent = (*opened)->PlanRead(Key("absent"), package.Bytes.size());
  CHECK(!small && small.error() == AssetCacheError::CapacityExceeded && absent && !*absent,
        "metadata rejects a whole-package overflow and keeps absence distinct from empty data");
  opened->reset();
  auto fresh = AssetCache::Open(path);
  CHECK(fresh.has_value(), "a fresh cache connection sees the committed package");
  if (!fresh) { return; }
  const auto loaded = (*fresh)->Load(Key("second"), package.Bytes.size());
  CHECK(loaded && *loaded && (**loaded).Bytes().size() == 3 && (**loaded).Bytes().front() == 4 &&
            (**loaded).Bytes().back() == 6,
        "the real loader still verifies and exposes exactly the requested native slice");
  auto replacement = package.Bytes;
  replacement.resize(32);
  CHECK((*fresh)->Publish(package.Records, replacement),
        "a later writer can replace the measured package before the byte read");
  const auto stale = (*fresh)->Load(Key("second"), **secondBytes);
  const auto current = (*fresh)->PlanRead(Key("second"), replacement.size());
  CHECK(!stale && stale.error() == AssetCacheError::CapacityExceeded && current && *current &&
            (**current).PackageBytesMost == replacement.size() &&
            (*fresh)->Find(Key("second"))->has_value(),
        "stale admission refuses a larger read without erasing the newer valid product");
}
}

int main() {
  using namespace outshine::Test;
  auto root = (std::filesystem::temp_directory_path() / "outshine-commit-receipt-XXXXXX").string();
  CHECK(mkdtemp(root.data()) != nullptr, "the receipt fixture owns an isolated store");
  if (!std::filesystem::is_directory(root)) { return Report(); }
  CheckReceipt(root + "/assets.sqlite");
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "the fixture closes its stores before removing generated data");
  return Report();
}
