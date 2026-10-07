#include "PreparedStreetGraph.h"
#include "AssetSourceRecipe.h"
#include "BinaryValueArchive.h"
#include "SourceSet.h"
#include "content/AssetGeneration.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
constexpr size_t kPackageBytesMost = size_t{512} * 1024u * 1024u;
constexpr uint64_t kFormat = 0x000131534eULL;

template <class Archive, class Elevated> bool ElevationFields(Archive &archive, Elevated &of) {
  return archive(of.Points,
                 of.Refused,
                 of.SteepestGrade,
                 of.OverTenPercent,
                 of.OverThirtyPercent,
                 of.SteepestSealedGrade,
                 of.SealedOverTenPercent);
}

std::optional<std::vector<uint8_t>> Encode(const StreetGraphBuilder::Built &built,
                                           size_t bytesMost) {
  if (!built.Graph || built.Elevated.Refused != 0) { return std::nullopt; }
  BinaryValueWriter out(bytesMost);
  if (!out(kFormat) || !ElevationFields(out, built.Elevated)) { return std::nullopt; }
  const auto headerBytes = out.Out.Bytes().size();
  auto native = built.Graph->EncodeAsset(bytesMost - headerBytes);
  if (!native || !out.Out.Put(*native)) { return std::nullopt; }
  return std::move(out.Out).TakeBytes();
}

std::optional<StreetGraphBuilder::Built> Decode(std::span<const uint8_t> bytes) {
  BinaryValueReader in(bytes);
  uint64_t format = 0;
  StreetGraphBuilder::Built built;
  if (!in(format) || format != kFormat || !ElevationFields(in, built.Elevated) ||
      built.Elevated.Refused != 0 || !std::isfinite(built.Elevated.SteepestGrade) ||
      built.Elevated.SteepestGrade < 0 || !std::isfinite(built.Elevated.SteepestSealedGrade) ||
      built.Elevated.SteepestSealedGrade < 0) {
    return std::nullopt;
  }
  const auto native = in.In.Take(in.In.Remaining());
  if (!native) { return std::nullopt; }
  auto graph = Path::Network::DecodeAsset(*native, kPackageBytesMost);
  if (!graph || built.Elevated.Points != graph->PointCount() ||
      built.Elevated.OverTenPercent > graph->PointCount() ||
      built.Elevated.OverThirtyPercent > built.Elevated.OverTenPercent ||
      built.Elevated.SealedOverTenPercent > built.Elevated.OverTenPercent) {
    return std::nullopt;
  }
  built.Ways = graph->WayCount();
  built.Nodes = graph->NodeCount();
  built.Edges = graph->EdgeCount();
  built.Junctions = graph->JunctionCount();
  std::vector<Path::Network::Crossing> crossings;
  auto swept = graph->Crossings(crossings);
  if (!swept) { return std::nullopt; }
  built.CrossingSweep = *swept;
  built.Graph = std::make_shared<const Path::Network>(std::move(*graph));
  return built;
}
}

PreparedStreetGraph::PreparedStreetGraph(std::unique_ptr<AssetCache> cache, std::string recipe)
    : Cache_(std::move(cache)), Recipe_(std::move(recipe)) {}

std::expected<std::shared_ptr<PreparedStreetGraph>, std::string>
PreparedStreetGraph::Open(const std::string &directory, const Data::SourceSet &sources) {
  auto cache = AssetCache::Open((std::filesystem::path(directory) / "assets.sqlite").string());
  if (!cache) { return std::unexpected("could not open the prepared street graph cache"); }
  return std::shared_ptr<PreparedStreetGraph>(new PreparedStreetGraph(
      std::move(*cache),
      Generators::AssetSourceRecipe(
          "prepared-street-graph-1",
          sources,
          std::array{Data::DataKind::Elevation, Data::DataKind::VectorMap})));
}

std::expected<PreparedStreetGraph::Loaded, std::string>
PreparedStreetGraph::Resolve(const std::string &key, const Factory &factory) {
  const std::scoped_lock lock(Lock_);
  auto existing = Cache_->Load(key, kPackageBytesMost);
  if (!existing) { return std::unexpected("could not read the prepared street graph"); }
  if (*existing) {
    auto decoded = Decode((**existing).Bytes());
    if (decoded) {
      return Loaded{
          .Graph = std::move(*decoded), .Hit = true, .ReadBytes = (**existing).Bytes().size()};
    }
    if (!Cache_->Remove(key)) {
      return std::unexpected("could not remove an invalid prepared street graph");
    }
  }
  std::optional<StreetGraphBuilder::Built> generated;
  auto loaded =
      ResolveAsset(*Cache_,
                   key,
                   kPackageBytesMost,
                   [&](size_t bytesMost) -> std::expected<GeneratedAssetPackage, std::string> {
                     if (!factory) { return std::unexpected("street graph miss has no generator"); }
                     auto built = factory();
                     if (!built) { return std::unexpected(std::move(built.error())); }
                     generated = std::move(*built);
                     auto encoded = Encode(*generated, bytesMost);
                     if (!encoded) {
                       return std::unexpected("street graph is incomplete or exceeds its budget");
                     }
                     auto bounds = generated->Graph->BoundsEcef();
                     if (bounds.Empty()) { bounds.Cover(Vec3{}); }
                     return GeneratedAssetPackage{.Records = {{.Key = key,
                                                               .Kind = "street-network",
                                                               .Bounds = bounds,
                                                               .Package = {},
                                                               .ByteCount = encoded->size(),
                                                               .Parent = {}}},
                                                  .Bytes = std::move(*encoded)};
                   });
  if (generated && generated->Elevated.Refused != 0) {
    return Loaded{.Graph = std::move(*generated)};
  }
  if (!loaded) { return std::unexpected(std::move(loaded.error())); }
  auto decoded = Decode(loaded->Bytes());
  if (!decoded) { return std::unexpected("published street graph failed native validation"); }
  if (generated) {
    generated->Graph = std::move(decoded->Graph);
    return Loaded{.Graph = std::move(*generated), .ReadBytes = loaded->Bytes().size()};
  }
  return Loaded{.Graph = std::move(*decoded), .Hit = true, .ReadBytes = loaded->Bytes().size()};
}
}
