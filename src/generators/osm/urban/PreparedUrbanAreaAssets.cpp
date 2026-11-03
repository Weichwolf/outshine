#include "PreparedUrbanWorker.h"
#include "UrbanAreaAsset.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include <array>
#include <atomic>
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
std::string PreparedUrbanAreas::Worker::RequestKey(const Definition &input) const {
  const auto &owner = input.Owner;
  if (owner.Zoom < 0 || owner.Zoom > Data::TileId::MaximumZoom ||
      owner.X >= (uint64_t{1} << static_cast<unsigned>(owner.Zoom)) ||
      owner.Y >= (uint64_t{1} << static_cast<unsigned>(owner.Zoom)) || !input.Recipe.Valid() ||
      (input.Schema != MvtSchema::Shortbread && input.Schema != MvtSchema::OpenMapTiles)) {
    return {};
  }
  constexpr size_t keyBytesMost = 4096;
  BinaryValueWriter out(keyBytesMost);
  if (!out.Text(Recipe) || !out(owner.Zoom,
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

bool PreparedUrbanAreas::Worker::Matches(const UrbanArea &area, const Definition &input) const {
  const auto parent = Parent(input.Owner);
  const size_t count = parent == input.Owner ? 1u : 2u;
  if (area.Recipe.ProjectedPitchM != input.Recipe.ProjectedPitchM ||
      area.Recipe.FillFraction != input.Recipe.FillFraction ||
      area.Recipe.PatchSide != input.Recipe.PatchSide || area.Tiers.size() > Limits.CellsMost ||
      area.Inputs.size() != count || area.Inputs[0].Identity.Kind != Data::DataKind::VectorMap ||
      area.Inputs[0].Identity.Tile != input.Owner) {
    return false;
  }
  return count == 1 || (area.Inputs[1].Identity.Kind == Data::DataKind::VectorMap &&
                        area.Inputs[1].Identity.Tile == parent);
}

std::expected<PreparedUrbanAreas::Loaded, std::string>
PreparedUrbanAreas::Worker::Load(const std::string &request, const Definition &input) {
  auto loaded = Read(request, input);
  if (!loaded) { return ReadFailure(loaded.error()); }
  if (loaded->Area) {
    ++Hits;
  } else {
    ++Misses;
  }
  return std::move(*loaded);
}

std::expected<PreparedUrbanAreas::Loaded, PreparedUrbanAreas::Worker::Failure>
PreparedUrbanAreas::Worker::Read(const std::string &request, const Definition &input) {
  const std::scoped_lock lock(CacheMutex);
  auto loaded = Cache->LoadRequest(request, kUrbanAreaBytesMost);
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (!*loaded) { return Loaded{}; }
  ReadBytes += (**loaded).Bytes().size();
  auto area = DecodeUrbanArea((**loaded).Bytes());
  if ((**loaded).Record().Kind != "urban-area" || !area || !Matches(*area, input)) {
    return std::unexpected(
        std::string("native urban-area asset does not match its request or budget"));
  }
  return Loaded{.Area = std::make_shared<UrbanArea>(std::move(*area)),
                .Key = (**loaded).Record().Key};
}

std::expected<PreparedUrbanAreas::Loaded, std::string>
PreparedUrbanAreas::Worker::Generate(const std::string &request, Work &work) {
  if (Stop.stop_requested()) { return std::unexpected("urban-area preparation cancelled"); }
  auto existing = Read(request, work.Input);
  if (!existing) { return ReadFailure(existing.error()); }
  if (existing->Area) {
    ++Hits;
    return std::move(*existing);
  }
  const UrbanAreaLimits limits{.CellsMost = Limits.CellsMost,
                               .SourceVerticesMost = Limits.SourceVerticesMost,
                               .FeaturesMost = Limits.FeaturesMost,
                               .Deadline = std::chrono::steady_clock::now() + Limits.GenerationTime,
                               .Stop = Stop.get_token()};
  std::array<const OsmField *, 2> sources{};
  size_t sourceBytes = 0;
  for (size_t index = 0; index < work.SourceCount; ++index) {
    if (!work.Sources[index]) { return std::unexpected("urban-area source is not ready"); }
    const auto held = work.Sources[index]->HeapBytes();
    if (held > Limits.SourceBytesMost - sourceBytes) {
      return std::unexpected("urban-area input exceeds source memory budget");
    }
    sourceBytes += held;
    sources[index] = work.Sources[index].get();
  }
  ++Generations;
  auto area =
      GenerateUrbanArea(std::span(sources.data(), work.SourceCount), work.Input.Recipe, limits);
  if (!area) { return std::unexpected(std::move(area.error())); }
  GeneratedCells += area->Tiers.size();
  if (Stop.stop_requested()) { return std::unexpected("urban-area preparation cancelled"); }
  auto package = PackageUrbanArea(*area, request, kUrbanAreaBytesMost);
  if (!package) { return std::unexpected(std::move(package.error())); }
  auto prepared = Cache->PreparePublication(std::move(package->Records), std::move(package->Bytes));
  if (!prepared) {
    return std::unexpected(std::string("native urban-area publication cannot be prepared"));
  }
  work.Publication = std::move(*prepared);
  work.PublicationBytes.store(work.Publication->CapacityBytes(), std::memory_order_release);
  work.Sources = {};
  work.Layouts = {};
  work.SourceCount = 0;
  work.Stage = Work::Phase::Publish;
  return Publish(request, work);
}
}
