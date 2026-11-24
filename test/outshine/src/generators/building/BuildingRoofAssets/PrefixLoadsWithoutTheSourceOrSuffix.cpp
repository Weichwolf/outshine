#include "BuildingAssetFixture.h"
#include "BuildingRoofAssets.h"
#include "Check.h"
#include "OpenAssetStore.h"
#include "PreparedBuildingAssets.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <limits>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

void CheckPrefix(const std::filesystem::path &root) {
  const auto output = OpenAssetStore((root / "roof").string());
  Data::RawSourceCache rawSources({.Directory = (root / "sources").string()});
  const Data::SourceSet sources(rawSources);
  auto inputs = PreparedBuildingAssets::Open(OpenAssetStore((root / "forms").string()), sources);
  CHECK(output && inputs, "the source and display products have independent stores");
  if (!output || !inputs) { return; }
  auto raw = BuildingAssetFixture::Inputs();
  auto shape = raw.Structures.front();
  for (int at = 1; at < 144; ++at) {
    shape.HeightM = raw.Structures.front().HeightM + at * 0.1;
    raw.Structures.push_back(shape);
  }
  Ground::HeightField::Block ground;
  ground.At = {.Zoom = 0, .X = 0, .Y = 0};
  ground.Raster = {.Side = 2, .Postings = 2};
  ground.Nodes = {125, 125, 125, 125};
  ground.Sources = {{.Kind = Data::DataKind::Elevation, .SourceId = "test-dem", .Revision = "1"}};
  const auto heights = Ground::HeightField::Of(0, {ground});
  if (!heights) {
    CHECK(false, "test heights are valid");
    return;
  }
  const std::atomic_bool stop{false};
  const auto key = (*inputs)->Key({.Zoom = 0, .X = 0, .Y = 0}, 0, {}, 1000);
  const auto base = (*inputs)->Generate(key, raw, *heights, stop);
  CHECK(base && *base, "prepared roofs exist");
  if (!base || !*base) { return; }
  auto asset = BuildingRoofAssets::Resolve(*output, inputs->get(), key);
  CHECK(asset && asset->SourceCount() == raw.Structures.size() && asset->Blocks().size() == 2,
        "the roof archive retains source identity and size-sorted blocks");
  if (!asset || asset->Blocks().size() != 2) { return; }
  CHECK(asset->Prefix(std::numeric_limits<double>::quiet_NaN()) == 0 &&
            asset->Prefix(asset->Blocks().front().MaximumDiameterM) == 1,
        "the exact limit admits only the first block");
  const auto entries = output->Select(
      {.Bounds = asset->Bounds(), .Kind = "building-roof-block", .Level = 1, .Planes = {}});
  if (!entries) {
    CHECK(false, "roof blocks are addressable");
    return;
  }
  const auto tail = std::ranges::min_element(*entries, {}, &AssetRecord::ByteCount);
  CHECK(tail != entries->end() && output->Remove(tail->Key), "the larger-shape suffix can expire");
  const auto cached = BuildingRoofAssets::Resolve(*output, nullptr, key);
  CHECK(cached, "the manifest remains available independently of the source or suffix");
  if (!cached) { return; }
  const auto prefix = cached->LoadBlock(*output, 0);
  const auto suffix = cached->LoadBlock(*output, 1);
  CHECK(prefix && *prefix && (**prefix).size() == cached->Blocks().front().Rows,
        "reading only the selected prefix requires no source input");
  CHECK(suffix && !*suffix, "a missing block is unavailable without claiming geometry");
  if (!prefix || !*prefix) { return; }
  for (const auto &row : **prefix) {
    const auto &original = (*base)->Surfaces[row.Source].Bounds();
    CHECK(row.WiderBounds().Holds(original.Min) && row.WiderBounds().Holds(original.Max),
          "rounding the proxy record keeps the original spatial bounds inside");
  }
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-tests.roof-prefix-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  CheckPrefix(root);
  std::error_code error;
  std::filesystem::remove_all(root, error);
  return Report();
}
