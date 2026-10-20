#include "RoadPageRestoration.h"
#include "Log.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace outshine {
RoadPageRestoration::RoadPageRestoration(
    Tasks &pool,
    std::shared_ptr<Generators::PreparedRoadPages> pages,
    std::shared_ptr<const Generators::RoadPageManifest> manifest)
    : Queue_(
          pool, std::move(pages), {.PendingMost = 1, .PageBytesMost = size_t{64} * 1024u * 1024u}),
      Manifest_(std::move(manifest)) {}

std::expected<std::unique_ptr<RoadPageRestoration>, std::string>
RoadPageRestoration::Begin(Tasks &pool,
                           std::shared_ptr<Generators::PreparedRoadPages> pages,
                           std::shared_ptr<const Generators::RoadPageManifest> manifest,
                           std::optional<std::span<const uint32_t>> selected,
                           std::string manifestKey) {
  if (!pages || !manifest || manifest->Materials.surfaces() != 1 ||
      manifest->Materials.parts() != 0 || manifest->Materials.images() != 0 ||
      manifest->Pages.size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected("road restoration needs complete native page and material metadata");
  }
  auto result = std::unique_ptr<RoadPageRestoration>(
      new RoadPageRestoration(pool, std::move(pages), std::move(manifest)));
  result->ManifestKey_ = std::move(manifestKey);
  if (selected) {
    result->Selected_.assign(selected->begin(), selected->end());
  } else {
    result->Selected_.resize(result->Manifest_->Pages.size());
    std::iota(result->Selected_.begin(), result->Selected_.end(), uint32_t{0});
  }
  if (!std::ranges::is_sorted(result->Selected_) ||
      std::ranges::adjacent_find(result->Selected_) != result->Selected_.end() ||
      (!result->Selected_.empty() && result->Selected_.back() >= result->Manifest_->Pages.size())) {
    return std::unexpected(
        "road restoration selection is duplicated, unordered or outside its manifest");
  }
  return result;
}

std::expected<bool, std::string> RoadPageRestoration::Advance(Geometry &ground) {
  if (Missing_) { return false; }
  if (Next_ == Selected_.size()) { return true; }
  if (Stage_ == Stage::Material) {
    const auto surface =
        ground.addSurface("streets", Manifest_->Materials.surfaceAt(MaterialInstance(0)));
    if (!surface) { return std::unexpected("could not restore the shared road material"); }
    Surface_ = *surface;
    Stage_ = Stage::Request;
    return false;
  }
  if (Stage_ == Stage::Request) {
    if (!Queue_.RequestCached(Manifest_->Pages[Selected_[Next_]].Key, {.Key = ManifestKey_})) {
      return std::unexpected("road restoration lookup admission failed");
    }
    Stage_ = Stage::Waiting;
    return false;
  }
  return Stage_ == Stage::Waiting ? Collect(ground) : Transfer(ground);
}

std::expected<bool, std::string> RoadPageRestoration::Collect(Geometry &ground) {
  auto completed = Queue_.Collect();
  if (completed.empty()) { return false; }
  auto &one = completed.front();
  const auto &reference = Manifest_->Pages[Selected_[Next_]];
  if (completed.size() != 1 || one.Key != reference.Key) {
    return std::unexpected("road restoration completion differs from selected metadata");
  }
  if (!one.Result) { return std::unexpected(std::move(one.Result.error())); }
  if (!*one.Result) {
    Missing_ = true;
    return false;
  }
  auto &loaded = **one.Result;
  Loaded_ = std::move(loaded.Page);
  if (!Loaded_ || Loaded_->Address.PlanKey != Manifest_->PlanKey ||
      Loaded_->Address.MaterialKey != Manifest_->MaterialKey ||
      Loaded_->Address.MaterialIndex != 0 ||
      Loaded_->Address.Anchor.LongitudeDeg != Manifest_->Anchor.LongitudeDeg ||
      Loaded_->Address.Anchor.LatitudeDeg != Manifest_->Anchor.LatitudeDeg ||
      Loaded_->Address.Level != reference.Level || Loaded_->Address.Parts != reference.Parts ||
      loaded.NativeBytes != reference.ByteCount) {
    return std::unexpected("road restoration product differs from its plan, material or frame");
  }
  NativeBytes_ += loaded.NativeBytes;
  for (const auto &diagnostic : Loaded_->Geometry.Diagnostics) {
    Log::Warn(LogTag::World,
              "road_page_part_rejected",
              {{"source", std::to_string(diagnostic.Source)},
               {"part", static_cast<int64_t>(diagnostic.Part.Index)},
               {"reason", diagnostic.Empty ? "empty_native_mesh" : "partial_native_mesh"},
               {"refused", static_cast<int64_t>(diagnostic.Stats.Refused)}});
  }
  if (Loaded_->Geometry.Mesh.Index.empty()) {
    Loaded_.reset();
    ++Next_;
    Stage_ = Stage::Request;
    return false;
  }
  if (!Surface_) { return std::unexpected("road restoration material is not ready"); }
  const auto part = ground.addPart("streets", *Surface_);
  if (!part) { return std::unexpected("could not restore the native road page"); }
  Part_ = *part;
  Stage_ = Stage::Positions;
  return false;
}

std::expected<bool, std::string> RoadPageRestoration::Transfer(Geometry &ground) {
  const auto &mesh = Loaded_->Geometry.Mesh;
  bool copied = false;
  switch (Stage_) {
    case Stage::Positions:
      copied = ground.setPositions(Part_, mesh.PositionM).has_value();
      Stage_ = Stage::Normals;
      break;
    case Stage::Normals:
      copied = ground.setNormals(Part_, mesh.NormalM).has_value();
      Stage_ = Stage::Colours;
      break;
    case Stage::Colours:
      copied = ground.setColours(Part_, mesh.ColourRgba).has_value();
      Stage_ = Stage::Triangles;
      break;
    case Stage::Triangles:
      copied = ground.setTriangles(Part_, mesh.Index).has_value();
      ++Next_;
      Loaded_.reset();
      Stage_ = Stage::Request;
      break;
    default: return std::unexpected("road restoration transfer is not active");
  }
  if (!copied) { return std::unexpected("could not transfer a restored road page into the world"); }
  return false;
}
}
