#include "PageIndexAsset.h"
#include "Check.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

std::vector<uint32_t> Ordered(const PlanHierarchy &tree, const Vec3 &eye) {
  std::vector<uint32_t> order;
  CHECK(tree.SelectNodesNearFirst(
            eye,
            [](const auto &) { return false; },
            [&](const auto &node) { order.push_back(tree.SourceOrder()[node.First]); }),
        "the retained hierarchy traverses its source metadata near child first");
  return order;
}

void Exercise(const std::string &path) {
  const auto parent = Sha256Hex("page-manifest", 13);
  const std::array bounds{Box{.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                          Box{.Min = {{10, 0, 0}}, .Max = {{11, 1, 1}}},
                          Box{.Min = {{100, 0, 0}}, .Max = {{101, 1, 1}}},
                          Box{.Min = {{110, 0, 0}}, .Max = {{111, 1, 1}}}};
  std::vector<uint32_t> before;
  {
    auto cache = AssetCache::Open(path);
    CHECK(cache, "the index asset store opens without any geometry producer");
    if (!cache) { return; }
    auto index = ResolvePageIndex(**cache, parent, bounds);
    CHECK(index && index->From == PageIndexAsset::Origin::Created &&
              index->Hierarchy.Nodes().size() == 2 * bounds.size() - 1 && index->ByteCount > 0,
          "one metadata miss builds and stores the complete spatial page hierarchy");
    if (!index) { return; }
    before = Ordered(index->Hierarchy, {{112, 0, 0}});
    CHECK(before == std::vector<uint32_t>({3, 2, 1, 0}),
          "the near side is visited first while every source page remains represented");
    const auto record = (*cache)->Find(index->Key);
    CHECK(record && *record && (**record).Parent == parent &&
              (**record).Kind == "spatial-page-index" && (**record).ByteCount == index->ByteCount,
          "the generic index remains bound to its immutable source manifest");
    const auto missing = (*cache)->Find(parent);
    CHECK(missing && !*missing,
          "no manifest or geometry body was needed to encode its known metadata");
  }
  auto reopened = AssetCache::Open(path);
  CHECK(reopened, "a fresh handle restores stored metadata without geometry or generators");
  if (!reopened) { return; }
  auto hit = ResolvePageIndex(**reopened, parent, bounds);
  CHECK(hit && hit->From == PageIndexAsset::Origin::Stored &&
            Ordered(hit->Hierarchy, {{112, 0, 0}}) == before &&
            Ordered(hit->Hierarchy, {{-1, 0, 0}}) == std::vector<uint32_t>({0, 1, 2, 3}),
        "the persisted tree supports both directions without rebuilding page products");
  if (!hit) { return; }
  size_t groups = 0;
  size_t sources = 0;
  CHECK(hit->Hierarchy.SelectNodesNearFirst(
            {{112, 0, 0}},
            [](const auto &node) { return node.Count <= 2; },
            [&](const auto &node) {
              ++groups;
              sources += node.Count;
            }) &&
            groups == 2 && sources == bounds.size(),
        "the same metadata selects complete grouped cohorts before any fine page IO");
  size_t visited = 0;
  CHECK(!hit->Hierarchy.SelectNodesNearFirst(
            {{std::numeric_limits<double>::quiet_NaN(), 0, 0}},
            [](const auto &) { return true; },
            [&](const auto &) { ++visited; }) &&
            visited == 0,
        "an invalid eye cannot partially publish a selection");
  auto altered = bounds;
  altered[0] = {.Min = {{1000, 0, 0}}, .Max = {{1001, 1, 1}}};
  const auto mismatched = ResolvePageIndex(**reopened, parent, altered);
  CHECK(!mismatched, "a different source envelope cannot silently reuse this manifest's tree");
  const auto invalid = ResolvePageIndex(**reopened, "bad-parent", bounds);
  CHECK(!invalid && invalid.error() == AssetCacheError::InvalidInput,
        "the metadata asset requires a valid immutable parent address");
  const auto empty = ResolvePageIndex(**reopened, parent, {});
  CHECK(empty && empty->From == PageIndexAsset::Origin::Empty && empty->Hierarchy.Nodes().empty(),
        "empty page sets need no persistent empty payload or producer work");
}
}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-tests.page-index-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  Exercise((directory / "assets.sqlite").string());
  std::filesystem::remove_all(directory);
  return outshine::Test::Report();
}
