#include "PreparedUrbanAreas.h"
#include "UrbanAreaAsset.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine::Generators::Osm {
std::string PreparedUrbanAreas::RequestKey(const Definition &input) const {
  const auto &owner = input.Owner;
  if (owner.Zoom < 0 || owner.Zoom > Data::TileId::MaximumZoom ||
      owner.X >= (uint64_t{1} << static_cast<unsigned>(owner.Zoom)) ||
      owner.Y >= (uint64_t{1} << static_cast<unsigned>(owner.Zoom)) || !input.Recipe.Valid() ||
      (input.Schema != MvtSchema::Shortbread && input.Schema != MvtSchema::OpenMapTiles)) {
    return {};
  }
  constexpr size_t keyBytesMost = 4096;
  BinaryValueWriter out(keyBytesMost);
  if (!out.Text(Recipe_) || !out(owner.Zoom,
                                 owner.X,
                                 owner.Y,
                                 input.Schema,
                                 input.Recipe.ProjectedPitchM,
                                 input.Recipe.FillFraction,
                                 input.Recipe.PatchSide)) {
    return {};
  }
  return Sha256Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}

bool PreparedUrbanAreas::Matches(const UrbanArea &area, const Definition &input) const {
  const auto parent = Parent(input.Owner);
  const size_t count = parent == input.Owner ? 1u : 2u;
  if (area.Recipe.ProjectedPitchM != input.Recipe.ProjectedPitchM ||
      area.Recipe.FillFraction != input.Recipe.FillFraction ||
      area.Recipe.PatchSide != input.Recipe.PatchSide || area.Tiers.size() > Budget_.CellsMost ||
      area.Inputs.size() != count || area.Inputs[0].Identity.Kind != Data::DataKind::VectorMap ||
      area.Inputs[0].Identity.Tile != input.Owner) {
    return false;
  }
  return count == 1 || (area.Inputs[1].Identity.Kind == Data::DataKind::VectorMap &&
                        area.Inputs[1].Identity.Tile == parent);
}

std::expected<PreparedUrbanAreas::Loaded, AssetCacheError>
PreparedUrbanAreas::Load(const std::string &request, const Definition &input, bool publication) {
  auto record = Cache_->FindRequest(request);
  if (!record) { return std::unexpected(record.error()); }
  if (!*record) {
    if (!publication) { ++Misses_; }
    return Loaded{};
  }
  auto loaded = Cache_->Load((**record).Key, kUrbanAreaBytesMost);
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (!*loaded) {
    if (!publication) { ++Misses_; }
    return Loaded{};
  }
  if (publication) {
    PublicationReadBytes_ += (**loaded).Bytes().size();
  } else {
    ReadBytes_ += (**loaded).Bytes().size();
  }
  auto area = DecodeUrbanArea((**loaded).Bytes());
  if ((**loaded).Record().Kind != "urban-area" || !area || !Matches(*area, input)) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  if (!publication) { ++Hits_; }
  return Loaded{.Area = std::make_shared<UrbanArea>(std::move(*area)),
                .Key = (**loaded).Record().Key};
}

std::expected<PreparedUrbanAreas::Loaded, std::string>
PreparedUrbanAreas::Generate(const std::string &request, Work &work) {
  const std::stop_callback stopped(Stop_.get_token(), [&work] { work.Stop.request_stop(); });
  if (work.Stop.stop_requested()) { return std::unexpected("urban-area preparation cancelled"); }
  const UrbanAreaLimits limits{.CellsMost = Budget_.CellsMost,
                               .SourceVerticesMost = Budget_.SourceVerticesMost,
                               .FeaturesMost = Budget_.FeaturesMost,
                               .Deadline =
                                   std::chrono::steady_clock::now() + Budget_.GenerationTime,
                               .Stop = work.Stop.get_token()};
  std::array<const OsmField *, 2> sources{};
  size_t sourceBytes = 0;
  for (size_t index = 0; index < work.SourceCount; ++index) {
    if (!work.Sources[index]) { return std::unexpected("urban-area source is not ready"); }
    const auto held = work.Sources[index]->HeapBytes();
    if (held > Budget_.SourceBytesMost - sourceBytes) {
      return std::unexpected("urban-area input exceeds source memory budget");
    }
    sourceBytes += held;
    sources[index] = work.Sources[index].get();
  }
  ++Generations_;
  auto area =
      GenerateUrbanArea(std::span(sources.data(), work.SourceCount), work.Input.Recipe, limits);
  if (!area) { return std::unexpected(std::move(area.error())); }
  GeneratedCells_ += area->Tiers.size();
  if (work.Stop.stop_requested()) { return std::unexpected("urban-area preparation cancelled"); }
  auto package = PackageUrbanArea(*area, request, kUrbanAreaBytesMost);
  if (!package) { return std::unexpected(std::move(package.error())); }
  *area = UrbanArea{};
  work.Sources = {};
  work.Layouts = {};
  auto prepared =
      Cache_->PreparePublication(std::move(package->Records), std::move(package->Bytes));
  if (!prepared) { return std::unexpected("could not prepare native urban-area publication"); }
  if (work.Stop.stop_requested()) { return std::unexpected("urban-area preparation cancelled"); }
  SetPublication(work, std::move(*prepared));
  return Loaded{};
}
}
