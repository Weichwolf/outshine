#include "RoadPageRestoration.h"
#include "Log.h"
#include <algorithm>
#include <chrono>
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
namespace {
constexpr size_t kPageBytesMost = size_t{64} * 1024u * 1024u;
}

RoadPageRestoration::RoadPageRestoration(
    Tasks &pool,
    std::shared_ptr<Generators::PreparedRoadPages> pages,
    std::shared_ptr<const Generators::RoadPageManifest> manifest,
    Limits limits)
    : Queue_(pool,
             std::move(pages),
             {.PendingMost = limits.PendingMost, .PageBytesMost = kPageBytesMost}),
      Manifest_(std::move(manifest)),
      Limits_(limits) {}

std::expected<std::unique_ptr<RoadPageRestoration>, std::string>
RoadPageRestoration::Begin(Tasks &pool,
                           std::shared_ptr<Generators::PreparedRoadPages> pages,
                           std::shared_ptr<const Generators::RoadPageManifest> manifest,
                           std::optional<std::span<const uint32_t>> selected,
                           std::string manifestKey,
                           Limits limits) {
  if (!pages || !manifest || manifest->Materials.surfaces() != 1 ||
      manifest->Materials.parts() != 0 || manifest->Materials.images() != 0 ||
      manifest->Pages.size() > std::numeric_limits<uint32_t>::max() || limits.PendingMost == 0 ||
      limits.NativeBytesMost == 0) {
    return std::unexpected("road restoration needs complete native page and material metadata");
  }
  auto result = std::unique_ptr<RoadPageRestoration>(
      new RoadPageRestoration(pool, std::move(pages), std::move(manifest), limits));
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
  for (const auto index : result->Selected_) {
    const auto bytes = result->Manifest_->Pages[index].ByteCount;
    if (bytes == 0 || bytes > kPageBytesMost) {
      return std::unexpected("road restoration page exceeds its native payload ceiling");
    }
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
    Stage_ = Stage::Waiting;
  }
  constexpr size_t kStepsMost = 32;
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(1);
  for (size_t step = 0; step < kStepsMost; ++step) {
    const auto requested = Prefetch();
    if (!requested) { return std::unexpected(requested.error()); }
    const auto collected = CollectReady();
    if (!collected) { return std::unexpected(collected.error()); }
    if (Stage_ == Stage::Waiting && !Window_.front().Result) { return false; }
    const auto advanced = Stage_ == Stage::Waiting ? Collect(ground) : Transfer(ground);
    if (!advanced) { return std::unexpected(advanced.error()); }
    if (Missing_) { return false; }
    if (Next_ == Selected_.size()) { return true; }
    if (std::chrono::steady_clock::now() >= until) { return false; }
  }
  return false;
}

std::expected<void, std::string> RoadPageRestoration::Prefetch() {
  while (Requested_ < Selected_.size() && Window_.size() < Limits_.PendingMost) {
    const auto index = Selected_[Requested_];
    const auto &page = Manifest_->Pages[index];
    if (!Window_.empty() && (WindowBytes_ >= Limits_.NativeBytesMost ||
                             page.ByteCount > Limits_.NativeBytesMost - WindowBytes_)) {
      break;
    }
    if (!Queue_.RequestCached(page.Key, {.Key = ManifestKey_})) {
      return std::unexpected("road restoration lookup admission failed");
    }
    Window_.push_back({.Index = index, .Result = std::nullopt});
    WindowBytes_ += static_cast<size_t>(page.ByteCount);
    ++Requested_;
  }
  return {};
}

std::expected<void, std::string> RoadPageRestoration::CollectReady() {
  for (auto &completed : Queue_.Collect()) {
    const auto slot = std::ranges::find_if(Window_, [&](const Pending &pending) {
      return Manifest_->Pages[pending.Index].Key == completed.Key;
    });
    if (slot == Window_.end() || slot->Result) {
      return std::unexpected("road restoration completion differs from selected metadata");
    }
    slot->Result = std::move(completed.Result);
  }
  return {};
}

std::expected<bool, std::string> RoadPageRestoration::Collect(Geometry &ground) {
  if (Window_.empty()) { return std::unexpected("road restoration has no admitted page"); }
  auto &ready = Window_.front().Result;
  if (!ready) { return false; }
  auto one = std::move(*ready);
  const auto &reference = Manifest_->Pages[Selected_[Next_]];
  if (!one) { return std::unexpected(std::move(one.error())); }
  if (!*one) {
    Missing_ = true;
    for (const auto &pending : Window_) { Queue_.Cancel(Manifest_->Pages[pending.Index].Key); }
    return false;
  }
  auto &loaded = **one;
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
    FinishPage();
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
      FinishPage();
      break;
    default: return std::unexpected("road restoration transfer is not active");
  }
  if (!copied) { return std::unexpected("could not transfer a restored road page into the world"); }
  return false;
}

void RoadPageRestoration::FinishPage() {
  WindowBytes_ -= static_cast<size_t>(Manifest_->Pages[Window_.front().Index].ByteCount);
  Window_.pop_front();
  ++Next_;
  Loaded_.reset();
  Stage_ = Stage::Waiting;
}
}
