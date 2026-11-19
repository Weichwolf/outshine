#include "RoadPageRestoration.h"
#include "PageIndexAsset.h"
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
#include <unordered_set>
#include <variant>
#include <vector>

namespace outshine {
namespace {
constexpr size_t kPageBytesMost = size_t{64} * 1024u * 1024u;

bool UniqueCoverage(const Generators::RoadPageManifest &manifest,
                    std::span<const uint32_t> selected) {
  std::unordered_set<uint64_t> parts;
  for (const auto index : selected) {
    for (const auto &part : manifest.Pages[index].Parts) {
      const uint64_t key = (static_cast<uint64_t>(part.Kind) << 32) | part.Index;
      if (!parts.insert(key).second) { return false; }
    }
  }
  return true;
}
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
                           std::optional<Vec3> eye,
                           Limits limits) {
  if (!pages || !manifest || manifest->Materials.surfaces() != 1 ||
      manifest->Materials.parts() != 0 || manifest->Materials.images() != 0 ||
      manifest->Pages.size() > std::numeric_limits<uint32_t>::max() || limits.PendingMost == 0 ||
      limits.NativeBytesMost == 0 || !Generators::ValidRoadPageAppearance(manifest->Appearance)) {
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
  if (!UniqueCoverage(*result->Manifest_, result->Selected_)) {
    return std::unexpected("road restoration selection overlaps source coverage");
  }
  for (const auto index : result->Selected_) {
    const auto &reference = result->Manifest_->Pages[index];
    if (!Generators::ValidRoadPageAppearance(reference.Appearance) ||
        reference.Appearance.index() != result->Manifest_->Appearance.index()) {
      return std::unexpected("road restoration page appearance differs from its manifest");
    }
    const auto bytes = reference.ByteCount;
    if (bytes == 0 || bytes > kPageBytesMost) {
      return std::unexpected("road restoration page exceeds its native payload ceiling");
    }
  }
  if (eye && result->Manifest_->PageIndex && !result->OrderPages(*eye)) {
    return std::unexpected("road page hierarchy does not match its selected manifest");
  }
  return result;
}

bool RoadPageRestoration::OrderPages(const Vec3 &eye) {
  const auto &hierarchy = Manifest_->PageIndex->Hierarchy;
  if (hierarchy.SourceOrder().size() != Manifest_->Pages.size()) { return false; }
  std::vector<uint8_t> wanted(Manifest_->Pages.size());
  for (const auto page : Selected_) { wanted[page] = 1; }
  std::vector<uint32_t> ordered;
  ordered.reserve(Selected_.size());
  const bool selected = hierarchy.SelectNodesNearFirst(
      eye,
      [](const PlanHierarchy::Node &) { return false; },
      [&](const PlanHierarchy::Node &node) {
        const uint32_t page = hierarchy.SourceOrder()[node.First];
        if (wanted[page] != 0) { ordered.push_back(page); }
      });
  if (!selected || ordered.size() != Selected_.size()) { return false; }
  Selected_.swap(ordered);
  return true;
}

std::expected<bool, std::string> RoadPageRestoration::Advance(const Consumer &consume) {
  if (!consume) { return std::unexpected("road restoration needs a page consumer"); }
  if (Missing_) { return false; }
  if (Next_ == Selected_.size()) { return true; }
  constexpr size_t kStepsMost = 32;
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(1);
  for (size_t step = 0; step < kStepsMost; ++step) {
    if (const auto requested = Prefetch(); !requested) {
      return std::unexpected(requested.error());
    }
    if (const auto collected = CollectReady(); !collected) {
      return std::unexpected(collected.error());
    }
    if (!Loaded_) {
      if (const auto loaded = Collect(); !loaded) { return std::unexpected(loaded.error()); }
      if (!Loaded_ || Missing_) { return false; }
    }
    const auto accepted = consume(*Loaded_, Manifest_->Pages[Selected_[Next_]], until);
    if (!accepted) { return std::unexpected(accepted.error()); }
    if (!*accepted) { return false; }
    FinishPage();
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

std::expected<void, std::string> RoadPageRestoration::Collect() {
  if (Window_.empty()) { return std::unexpected("road restoration has no admitted page"); }
  auto &ready = Window_.front().Result;
  if (!ready) { return {}; }
  auto one = std::move(*ready);
  const auto &reference = Manifest_->Pages[Selected_[Next_]];
  if (!one) { return std::unexpected(std::move(one.error())); }
  if (!*one) {
    Missing_ = true;
    for (const auto &pending : Window_) { Queue_.Cancel(Manifest_->Pages[pending.Index].Key); }
    return {};
  }
  auto *loaded = std::get_if<Generators::RoadPageLease>(&**one);
  if (loaded == nullptr) {
    return std::unexpected("road restoration received metadata instead of payload");
  }
  Loaded_ = std::move(loaded->Page);
  if (!Loaded_ || Loaded_->Address != Generators::StoredRoadPageAddress(*Manifest_, reference) ||
      loaded->NativeBytes != reference.ByteCount ||
      (!reference.ShapeKey.empty() && reference.Sources.size() != reference.Parts.size())) {
    return std::unexpected(
        "road restoration product differs from its plan, material, appearance or frame");
  }
  NativeBytes_ += loaded->NativeBytes;
  Meshing_ += Loaded_->Geometry.Swept;
  for (const auto &diagnostic : Loaded_->Geometry.Diagnostics) {
    const bool shared = !reference.ShapeKey.empty();
    const auto index = diagnostic.Part.Index;
    Log::Warn(LogTag::World,
              "road_page_part_rejected",
              {{"source", std::to_string(shared ? reference.Sources[index] : diagnostic.Source)},
               {"part", static_cast<int64_t>(shared ? reference.Parts[index].Index : index)},
               {"reason", diagnostic.Empty ? "empty_native_mesh" : "partial_native_mesh"},
               {"refused", static_cast<int64_t>(diagnostic.Stats.Refused)}});
  }
  return {};
}

void RoadPageRestoration::FinishPage() {
  WindowBytes_ -= static_cast<size_t>(Manifest_->Pages[Window_.front().Index].ByteCount);
  Window_.pop_front();
  ++Next_;
  Loaded_.reset();
}
}
