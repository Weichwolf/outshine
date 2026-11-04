#include "AssetCache.h"
#include "PreparedBuildingAssets.h"
#include "PreparedStructureTile.h"
#include "BuildingMesh.h"
#include "BuildingScratch.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <iostream>
#include <memory>
#include <ratio>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
template <class T> bool Number(std::string_view text, T &value) {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

struct Counts {
  size_t Sources = 0, Triangles = 0, OutputBytes = 0, CoordinateBytes = 0, Unsupported = 0;
};

std::expected<Counts, outshine::Generators::StructureBakeError>
Bake(const outshine::Generators::PreparedStructureTile &base,
     std::span<const uint32_t> sources,
     std::string_view mode,
     uint64_t triangleAttempts) {
  using namespace outshine;
  using namespace outshine::Generators;
  RawTile view;
  view.RequestedDetail = LevelOfDetail::Fine;
  BuildingScratch scratch;
  scratch.Triangles.emplace(BuildingScratch::TriangleBudget{.Remaining = triangleAttempts});
  const BuildingMesh mesher;
  if (mode == "cell") {
    view.RequestedCell = base.Structures[sources.front()].Layout.Cell.Index;
    const auto baked = BakePreparedStructures(base, view, mesher, scratch);
    if (!baked) { return std::unexpected(baked.error()); }
    const size_t coordinates = baked->Coordinates ? baked->Coordinates->HeapBytes() : 0;
    return Counts{.Sources = baked->Prints.size(),
                  .Triangles = (baked->Built.WallRun.size() + baked->Built.RoofRun.size()) / 3,
                  .OutputBytes = baked->HeapBytes() + coordinates,
                  .CoordinateBytes = coordinates,
                  .Unsupported = baked->UnsupportedMeshes};
  }
  const auto group = BakePreparedStructureSources(base, view, sources, mesher, scratch);
  if (!group) { return std::unexpected(group.error()); }
  return Counts{.Sources = group->Sources.size(),
                .Triangles = (group->Mesh.WallRun.size() + group->Mesh.RoofRun.size()) / 3,
                .OutputBytes = group->HeapBytes()};
}
}

int main(int argc, char *const *argv) {
  using namespace outshine;
  using namespace outshine::Generators;
  if (argc < 6) {
    std::cerr << "source_group_bake <asset-db> <base-key> <subset|cell> <triangle-attempts> "
                 "<source-ordinal>...\n";
    return 2;
  }
  const std::string_view mode = argv[3];
  uint64_t attempts = 0;
  if ((mode != "subset" && mode != "cell") || !Number(argv[4], attempts) || attempts == 0) {
    return 2;
  }
  std::vector<uint32_t> selected;
  for (int at = 5; at < argc; ++at) {
    uint32_t ordinal = 0;
    if (!Number(argv[at], ordinal)) { return 2; }
    selected.push_back(ordinal);
  }
  std::ranges::sort(selected);
  if (std::ranges::adjacent_find(selected) != selected.end()) { return 2; }
  auto opened = AssetCache::Open(argv[1]);
  if (!opened) { return 3; }
  const std::shared_ptr<AssetCache> cache(std::move(*opened));
  Data::RawSourceCache raw({.Directory = (std::filesystem::temp_directory_path() /
                                          "outshine-master-source-group-unused-sources")
                                             .string()});
  const Data::SourceSet sources(raw);
  const auto assets = PreparedBuildingAssets::Open(cache, sources);
  if (!assets) { return 4; }
  const auto base = (*assets)->Load(argv[2]);
  if (!base || !*base || selected.back() >= (*base)->Structures.size()) { return 5; }
  if (mode == "cell" && std::ranges::any_of(selected, [&](uint32_t at) {
        return (*base)->Structures[at].Layout.Cell.Index !=
               (*base)->Structures[selected.front()].Layout.Cell.Index;
      })) {
    return 2;
  }
  const auto before = (*assets)->Costs();
  const auto began = std::chrono::steady_clock::now();
  const auto baked = Bake(**base, selected, mode, attempts);
  const double elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  if (!baked) {
    std::cerr << Describe(baked.error()) << '\n';
    return 6;
  }
  const auto after = (*assets)->Costs();
  std::cout << R"({"mode":")" << mode << R"(","parent_sources":)" << (*base)->Structures.size()
            << ",\"requested_sources\":" << selected.size()
            << ",\"baked_sources\":" << baked->Sources << ",\"triangles\":" << baked->Triangles
            << ",\"bake_ms\":" << elapsed << ",\"output_heap_bytes\":" << baked->OutputBytes
            << ",\"coordinate_heap_bytes\":" << baked->CoordinateBytes
            << ",\"form_cell_reads\":" << after.SurfaceHits - before.SurfaceHits
            << ",\"form_bytes\":" << after.SurfaceReadBytes - before.SurfaceReadBytes
            << ",\"unsupported_meshes\":" << baked->Unsupported << "}\n";
  return 0;
}
