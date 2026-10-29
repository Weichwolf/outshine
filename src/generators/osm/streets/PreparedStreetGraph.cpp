#include "PreparedStreetGraph.h"
#include "AssetSourceRecipe.h"
#include "BinaryValueArchive.h"
#include "SourceSet.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <stop_token>
#include <variant>
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
  auto recipe = Generators::AssetSourceRecipe(
      "prepared-street-graph-3",
      sources,
      std::array{Data::DataKind::Elevation, Data::DataKind::VectorMap});
  if (recipe.empty()) { return std::unexpected("street source metadata cannot be encoded"); }
  return std::shared_ptr<PreparedStreetGraph>(
      new PreparedStreetGraph(std::move(*cache), std::move(recipe)));
}

std::expected<void, PreparedStreetGraph::Failure>
PreparedStreetGraph::Prepare(Resolution &state, const Factory &factory) {
  if (!factory) { return std::unexpected(std::string("street graph miss has no generator")); }
  auto generated = factory();
  if (!generated) { return std::unexpected(std::move(generated.error())); }
  auto &built = state.Generated_.emplace(std::move(*generated));
  if (built.Elevated.Refused != 0) { return {}; }
  auto encoded = Encode(built, kPackageBytesMost);
  if (!encoded) {
    return std::unexpected(std::string("street graph is incomplete or exceeds its budget"));
  }
  auto bounds = built.Graph->BoundsEcef();
  if (bounds.Empty()) { bounds.Cover(Vec3{}); }
  std::vector<AssetRecord> records{{.Key = state.Key_,
                                    .Kind = "street-network",
                                    .Bounds = bounds,
                                    .Package = {},
                                    .ByteCount = encoded->size(),
                                    .Parent = {}}};
  auto package = Cache_->PreparePublication(std::move(records), std::move(*encoded));
  if (!package) { return std::unexpected(package.error()); }
  state.Package_ = std::move(*package);
  ++state.Preparations_;
  state.Current_ = Resolution::Phase::Publish;
  return {};
}

PreparedStreetGraph::Result PreparedStreetGraph::Restore(Resolution &state) {
  auto loaded = Cache_->Load(state.Key_, kPackageBytesMost);
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (!*loaded) { return std::unexpected(std::string("published asset package is unavailable")); }
  auto decoded = Decode((**loaded).Bytes());
  if (!decoded || !state.Generated_) {
    return std::unexpected(std::string("published street graph failed native validation"));
  }
  state.Generated_->Graph = std::move(decoded->Graph);
  return Loaded{.Graph = std::move(*state.Generated_), .ReadBytes = (**loaded).Bytes().size()};
}

PreparedStreetGraph::Result PreparedStreetGraph::Resume(Resolution &state,
                                                        const Factory &factory,
                                                        const std::stop_token &stop) {
  using Phase = Resolution::Phase;
  if (state.Current_ == Phase::Lookup) {
    auto existing = Cache_->Load(state.Key_, kPackageBytesMost);
    if (!existing) { return std::unexpected(existing.error()); }
    if (*existing) {
      auto decoded = Decode((**existing).Bytes());
      if (decoded) {
        return Loaded{
            .Graph = std::move(*decoded), .Hit = true, .ReadBytes = (**existing).Bytes().size()};
      }
    }
    state.Current_ = Phase::Generate;
  }
  if (state.Current_ == Phase::Generate) {
    const auto prepared = Prepare(state, factory);
    if (!prepared) { return std::unexpected(prepared.error()); }
    if (state.Generated_ && state.Generated_->Elevated.Refused != 0) {
      return Loaded{.Graph = std::move(*state.Generated_)};
    }
  }
  if (stop.stop_requested()) {
    return std::unexpected(std::string("street graph candidate canceled"));
  }
  if (state.Current_ == Phase::Publish) {
    if (!state.Package_) {
      return std::unexpected(std::string("prepared street graph publication is missing"));
    }
    ++state.Attempts_;
    const auto published = Cache_->Publish(*state.Package_);
    if (!published) { return std::unexpected(published.error()); }
    state.Current_ = Phase::Restore;
  }
  if (state.Current_ == Phase::Restore) { return Restore(state); }
  return std::unexpected(std::string("street graph resolution is already complete"));
}

PreparedStreetGraph::Result PreparedStreetGraph::Resolve(Resolution &state,
                                                         const Factory &factory,
                                                         const std::stop_token &stop) {
  auto resolved = stop.stop_requested()
                      ? Result(std::unexpected(std::string("street graph candidate canceled")))
                      : Resume(state, factory, stop);
  if (stop.stop_requested()) {
    resolved = std::unexpected(std::string("street graph candidate canceled"));
  }
  const auto *error = resolved ? nullptr : std::get_if<AssetCacheError>(&resolved.error());
  if (error == nullptr || *error != AssetCacheError::Busy) {
    state.Package_.reset();
    state.Generated_.reset();
    state.Current_ = Resolution::Phase::Complete;
  }
  return resolved;
}
}
