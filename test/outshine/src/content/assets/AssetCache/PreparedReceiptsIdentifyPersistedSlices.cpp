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

struct PackageData {
  std::vector<AssetRecord> Records;
  std::vector<uint8_t> Bytes;
};

PackageData Package() {
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
  auto prepared = (*opened)->PreparePublication(package.Records, package.Bytes);
  const auto before = (*opened)->Find(Key("first"));
  CHECK(prepared && before && !*before,
        "preparing normalized owning bytes does not publish any records");
  if (!prepared) { return; }
  CHECK((*opened)->Publish(*prepared).has_value(), "the prepared package commits atomically");
  const auto found = (*opened)->Find(Key("second"));
  CHECK(found && *found, "the persisted normalized slice has an independent metadata receipt");
  if (!found || !*found) { return; }
  const auto &receipt = *found;
  CHECK(receipt && receipt->Key == Key("second") && receipt->Kind == "second" &&
            receipt->Package == Sha256Hex(package.Bytes.data(), package.Bytes.size()) &&
            receipt->OffsetBytes == 3 && receipt->ByteCount == 3 && !receipt->RequestKey,
        "the receipt identifies the committed normalized record and its actual package slice");
  CHECK(package.Records[0].Package.empty() && package.Records[1].Package.empty(),
        "normalization does not mutate or return the caller's uncommitted record");
  const auto alias = (*opened)->FindRequest(Key("request"));
  CHECK(alias && *alias && (**alias).Key == Key("first") && (**alias).RequestKey == Key("request"),
        "the same atomic commit preserves durable request bindings");
  opened->reset();
  auto fresh = AssetCache::Open(path);
  CHECK(fresh.has_value(), "a fresh cache connection sees the committed package");
  if (!fresh) { return; }
  const auto loaded = (*fresh)->Load(Key("second"), package.Bytes.size());
  CHECK(loaded && *loaded && (**loaded).Bytes().size() == 3 && (**loaded).Bytes().front() == 4 &&
            (**loaded).Bytes().back() == 6,
        "the real loader still verifies and exposes exactly the requested native slice");
}
}

int main() {
  using namespace outshine::Test;
  auto root =
      (std::filesystem::temp_directory_path() / "outshine-prepared-receipt-XXXXXX").string();
  CHECK(mkdtemp(root.data()) != nullptr, "the receipt fixture owns an isolated store");
  if (!std::filesystem::is_directory(root)) { return Report(); }
  CheckReceipt(root + "/assets.sqlite");
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "the fixture closes its stores before removing generated data");
  return Report();
}
