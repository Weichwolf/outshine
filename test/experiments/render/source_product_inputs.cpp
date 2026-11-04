#include "AssetCache.h"
#include "PreparedBuildingAssets.h"
#include "PreparedStructureTile.h"
#include "PageIndexAsset.h"
#include "ByteArchive.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include <chrono>
#include <cstddef>
#include <cmath>
#include <limits>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <ratio>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
bool Write(const std::string &path, std::span<const uint8_t> bytes) {
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  file.flush();
  return static_cast<bool>(file);
}

bool WriteGeometry(const std::string &path,
                   const outshine::Generators::PreparedStructureTile &base,
                   const std::string &revision) {
  if (base.Surfaces.size() > std::numeric_limits<uint32_t>::max()) { return false; }
  outshine::ByteWriter bytes(std::numeric_limits<size_t>::max());
  if (!bytes.Number(uint64_t{0x3152544D4F454753ULL}) ||
      !bytes.Number(static_cast<uint32_t>(base.Surfaces.size())) ||
      !bytes.Number(static_cast<uint32_t>(revision.size())) ||
      !bytes.Put({reinterpret_cast<const uint8_t *>(revision.data()), revision.size()})) {
    return false;
  }
  for (const auto &surface : base.Surfaces) {
    const auto &selection = surface.PreparedSelection();
    const double error = selection ? selection->ShellErrorM.value_or(-1) : -1;
    if (!std::isfinite(error) || surface.FaceCount() > std::numeric_limits<uint32_t>::max() ||
        !bytes.Number(error) || !bytes.Number(static_cast<uint32_t>(surface.FaceCount()))) {
      return false;
    }
  }
  return Write(path, bytes.Bytes());
}
}

int main(int argc, char *const *argv) {
  using namespace outshine;
  using namespace outshine::Generators;
  if (argc != 4) {
    std::cerr << "source_product_inputs <asset-db> <base-key> <output-prefix>\n";
    return 2;
  }
  const std::string prefix = argv[3];
  auto opened = AssetCache::Open(argv[1]);
  if (!opened) { return 3; }
  const std::shared_ptr<AssetCache> cache(std::move(*opened));
  Data::RawSourceCache raw({.Directory = prefix + "-unused-sources"});
  const Data::SourceSet sources(raw);
  const auto assets = PreparedBuildingAssets::Open(cache, sources);
  if (!assets) { return 4; }
  const auto began = std::chrono::steady_clock::now();
  const auto base = (*assets)->Load(argv[2]);
  if (!base || !*base) { return 5; }
  const auto bytes = PageIndexAsset::EncodedBytes((*base)->Structures.size());
  if (!bytes) { return 6; }
  const auto tree = (*assets)->ResolveSourceIndex(argv[2], *bytes);
  if (!tree || !*tree) { return 7; }
  const auto stored = cache->Load((*tree)->Key, *bytes);
  if (!stored || !*stored) { return 8; }
  std::vector<uint8_t> cells;
  std::vector<uint8_t> supported;
  cells.reserve((*base)->Structures.size());
  supported.reserve((*base)->Surfaces.size());
  for (const auto &source : (*base)->Structures) {
    cells.push_back(static_cast<uint8_t>(source.Layout.Cell.Index));
  }
  for (const auto &surface : (*base)->Surfaces) {
    supported.push_back(surface.FaceCount() != 0 && !surface.Bounds().Empty() ? 1 : 0);
  }
  if (cells.size() != supported.size() || !Write(prefix + ".bin", (**stored).Bytes()) ||
      !Write(prefix + ".cells", cells) || !Write(prefix + ".supported", supported) ||
      !WriteGeometry(prefix + ".geometry", **base, argv[2])) {
    return 9;
  }
  const auto costs = (*assets)->Costs();
  const double elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  std::cout << "{\"sources\":" << cells.size() << ",\"index_bytes\":" << *bytes
            << ",\"header_read_bytes\":" << costs.ReadBytes
            << ",\"form_read_bytes\":" << costs.SurfaceReadBytes << ",\"export_ms\":" << elapsed
            << "}\n";
  return costs.SurfaceReadBytes == 0 ? 0 : 10;
}
