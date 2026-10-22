#include "PreparedUrbanAreas.h"
#include "UrbanAreaAsset.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include "content/AssetGeneration.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <span>
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

std::expected<PreparedUrbanAreas::Product, std::string>
PreparedUrbanAreas::Load(const std::string &request, const Definition &input) {
  const std::scoped_lock lock(CacheMutex_);
  auto record = Cache_->FindRequest(request);
  if (!record) { return std::unexpected("could not find native urban-area asset"); }
  if (!*record) {
    ++Misses_;
    return Product{};
  }
  auto loaded = Cache_->Load((**record).Key, kUrbanAreaBytesMost);
  if (!loaded) { return std::unexpected("could not load native urban-area asset"); }
  if (!*loaded) {
    ++Misses_;
    return Product{};
  }
  ReadBytes_ += (**loaded).Bytes().size();
  auto area = DecodeUrbanArea((**loaded).Bytes());
  if ((**loaded).Record().Kind != "urban-area" || !area || !Matches(*area, input)) {
    return std::unexpected("native urban-area asset does not match its request or budget");
  }
  ++Hits_;
  return Product(std::make_shared<UrbanArea>(std::move(*area)));
}

std::expected<PreparedUrbanAreas::Product, std::string>
PreparedUrbanAreas::Generate(const std::string &request, const Work &work) {
  if (Stop_.stop_requested()) { return std::unexpected("urban-area preparation cancelled"); }
  const UrbanAreaLimits limits{.CellsMost = Budget_.CellsMost,
                               .SourceVerticesMost = Budget_.SourceVerticesMost,
                               .FeaturesMost = Budget_.FeaturesMost,
                               .Deadline =
                                   std::chrono::steady_clock::now() + Budget_.GenerationTime,
                               .Stop = Stop_.get_token()};
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
  if (Stop_.stop_requested()) { return std::unexpected("urban-area preparation cancelled"); }
  const std::scoped_lock lock(CacheMutex_);
  bool generated = false;
  auto published =
      ResolveAssetRequest(*Cache_,
                          request,
                          kUrbanAreaBytesMost,
                          [&](size_t budget) -> std::expected<GeneratedAssetPackage, std::string> {
                            if (Stop_.stop_requested()) {
                              return std::unexpected("urban-area preparation cancelled");
                            }
                            generated = true;
                            return PackageUrbanArea(*area, request, budget);
                          });
  if (!published) { return std::unexpected(std::move(published.error())); }
  if (generated) {
    ++Writes_;
  } else {
    ReadBytes_ += published->Bytes().size();
  }
  auto complete = DecodeUrbanArea(published->Bytes());
  if (published->Record().Kind != "urban-area" || !complete || !Matches(*complete, work.Input)) {
    return std::unexpected("published native urban-area asset failed validation");
  }
  return Product(std::make_shared<UrbanArea>(std::move(*complete)));
}
}
