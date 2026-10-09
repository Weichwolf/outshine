#include "Check.h"
#include "PreparedTerrainAssets.h"
#include "PreparedGroundPatchCodec.h"
#include "SourceSet.h"
#include <array>
#include <chrono>
#include <filesystem>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-ground-patch-defect-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Data::ContentStore store({.Directory = (root / "sources").string()});
  Data::SourceSet sources(store);
  auto opened = PreparedTerrainAssets::Open((root / "assets").string(), sources);
  CHECK(opened.has_value(), "native sampling cache opens");
  if (!opened) { return Test::Report(); }
  auto assets = std::move(*opened);
  constexpr Data::TileId at{.Zoom = 14, .X = 8937, .Y = 5683};
  std::array<GroundPatch::Posting, 4> postings;
  for (auto &posting : postings) { posting.Height = GroundSample::At(100.12345); }
  const auto patch = GroundPatch::Complete(Tile(at.Zoom, at.X, at.Y), 2, postings);
  const auto key = assets->PatchKey(at, {}, 2, 13);
  const auto neighbor = assets->PatchKey(at, {}, 2, 12);
  CHECK(patch && assets->StorePatch(key, at, *patch) && assets->StorePatch(neighbor, at, *patch),
        "independent complete sampling requests coexist");
  auto storage = AssetCache::Open((root / "assets/assets.sqlite").string());
  CHECK(storage.has_value(), "fixture can inject a semantically invalid but intact package");
  if (!storage || !patch) { return Test::Report(); }
  auto record = (*storage)->Find(key);
  auto encoded = EncodePreparedGroundPatch(at, *patch);
  CHECK(record && *record && encoded, "complete metadata and original payload exist");
  if (!record || !*record || !encoded) { return Test::Report(); }
  encoded->pop_back();
  auto broken = **record;
  broken.Package.clear();
  broken.ByteCount = encoded->size();
  CHECK((*storage)->Publish({&broken, 1}, *encoded).has_value(),
        "fixture publishes truncated samples with a valid native package checksum");
  auto refused = assets->LoadPatch(key, at, 2);
  CHECK(refused && !*refused && assets->Costs().PatchMisses == 1,
        "semantic payload failure becomes an ordinary miss rather than partial readiness");
  auto absent = (*storage)->Find(key);
  auto preserved = assets->LoadPatch(neighbor, at, 2);
  CHECK(absent && !*absent && preserved && *preserved &&
            (*preserved)->HeightAslM({}) == patch->HeightAslM({}),
        "repair removes only the invalid record and retains neighboring valid products");
  CHECK(assets->StorePatch(key, at, *patch).has_value(),
        "the miss can atomically replace the damaged product");
  auto repaired = assets->LoadPatch(key, at, 2);
  CHECK(repaired && *repaired && (*repaired)->HeightAslM({}) == patch->HeightAslM({}),
        "repaired samples use the same normal load path");
  storage->reset();
  assets.reset();
  std::filesystem::remove_all(root);
  return Test::Report();
}
