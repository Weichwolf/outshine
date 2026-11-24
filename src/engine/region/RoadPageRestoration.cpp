#include "RoadPageRestoration.h"
#include "PageIndexAsset.h"
#include "Log.h"
#include "SourceProductSelection.h"
#include <algorithm>
#include <array>
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
    Limits limits,
    std::shared_ptr<AssetPipeline> publications)
    : RepairEnabled_(publications != nullptr),
      Queue_(pool,
             std::move(pages),
             {.PendingMost = limits.PendingMost, .PageBytesMost = kPageBytesMost},
             std::move(publications)),
      Manifest_(std::move(manifest)),
      Limits_(limits) {}

std::expected<std::unique_ptr<RoadPageRestoration>, std::string>
RoadPageRestoration::Begin(Tasks &pool,
                           std::shared_ptr<Generators::PreparedRoadPages> pages,
                           std::shared_ptr<const Generators::RoadPageManifest> manifest,
                           std::optional<std::span<const uint32_t>> selected,
                           std::string manifestKey,
                           std::optional<Vec3> eye,
                           Limits limits,
                           std::shared_ptr<AssetPipeline> publications,
                           std::optional<DisplayView> display) {
  if (!pages || !manifest || manifest->Materials.surfaces() != 1 ||
      manifest->Materials.parts() != 0 || manifest->Materials.images() != 0 ||
      manifest->Pages.size() > std::numeric_limits<uint32_t>::max() || limits.PendingMost == 0 ||
      limits.NativeBytesMost == 0 || !Generators::ValidRoadPageAppearance(manifest->Appearance)) {
    return std::unexpected("road restoration needs complete native page and material metadata");
  }
  auto result = std::unique_ptr<RoadPageRestoration>(new RoadPageRestoration(
      pool, std::move(pages), std::move(manifest), limits, std::move(publications)));
  result->ManifestKey_ = std::move(manifestKey);
  result->Display_ = display;
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
    const auto accepted = consume(*Loaded_, ActiveReference(Window_.front()), until);
    if (!accepted) { return std::unexpected(accepted.error()); }
    if (!*accepted) { return false; }
    FinishPage();
    if (Next_ == Selected_.size()) { return true; }
    if (std::chrono::steady_clock::now() >= until) { return false; }
  }
  return false;
}

const Generators::RoadPageReference &
RoadPageRestoration::ActiveReference(const Pending &pending) const {
  return pending.Display ? *pending.Display : Manifest_->Pages[pending.Index];
}

std::optional<RoadPageQueue::DisplayPage>
RoadPageRestoration::PreferredDisplay(const Generators::RoadPageReference &reference) const {
  if (!Display_ || reference.PositionErrorM || reference.Level != 0) { return std::nullopt; }
  const auto &view = *Display_;
  const double error =
      view.PathEnd
          ? SourceProductSelection::GeometryErrorLimitM(
                reference.Bounds,
                ViewPath{view.Eye, *view.PathEnd, view.TranslationM},
                view.ViewportPx,
                view.Projection)
          : SourceProductSelection::GeometryErrorLimitM(
                reference.Bounds, view.Eye, view.ViewportPx, view.Projection, view.TranslationM);
  constexpr std::array errors{1.0, 0.5, 0.25, 0.125};
  const auto *const selected =
      std::ranges::find_if(errors, [error](double level) { return level <= error; });
  if (selected == errors.end()) { return std::nullopt; }
  auto address = Generators::StoredRoadPageAddress(*Manifest_, reference);
  address.Level = 1;
  address.PositionErrorM = *selected;
  address.DisplayPolicy = Generators::RoadDisplayPolicy::StoredSurfaceEnvelope;
  return RoadPageQueue::DisplayPage{.Address = std::move(address),
                                    .BytesMost = static_cast<size_t>(reference.ByteCount - 1)};
}

std::expected<void, std::string> RoadPageRestoration::Prefetch() {
  while (Requested_ < Selected_.size() && Window_.size() < Limits_.PendingMost) {
    const auto index = Selected_[Requested_];
    const auto &page = Manifest_->Pages[index];
    if (!Window_.empty() && (WindowBytes_ >= Limits_.NativeBytesMost ||
                             page.ByteCount > Limits_.NativeBytesMost - WindowBytes_)) {
      break;
    }
    if (!Queue_.RequestCached(page.Key,
                              {.Key = CanRepair(page) ? std::string{} : ManifestKey_},
                              PreferredDisplay(page))) {
      return std::unexpected("road restoration lookup admission failed");
    }
    Window_.push_back(
        {.Index = index, .Result = std::nullopt, .Producer = {}, .Display = std::nullopt});
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
  auto &pending = Window_.front();
  const auto &reference = Manifest_->Pages[pending.Index];
  if (pending.Producer) { return Repair(pending, reference); }
  auto ready = std::exchange(pending.Result, std::nullopt);
  if (!ready) { return {}; }
  auto &one = *ready;
  switch (pending.Step) {
    case Stage::Recipe: {
      if (one && *one) {
        auto *producer = std::get_if<RoadPageQueue::RecipeProducer>(&**one);
        if (producer == nullptr || !*producer) {
          return std::unexpected("road repair did not receive a recipe producer");
        }
        pending.Producer = std::move(*producer);
        return Repair(pending, reference);
      }
      [[fallthrough]];
    }
    case Stage::Publish:
      if (!one) {
        Log::Warn(LogTag::World,
                  "road_page_recipe_repair_failed",
                  {{"page", reference.Key}, {"reason", one.error()}});
      }
      return VerifyRepair(pending, reference);
    case Stage::Read:
      if (one && !*one && CanRepair(reference)) {
        if (!Queue_.RequestRecipe(Generators::StoredRoadPageAddress(*Manifest_, reference))) {
          return std::unexpected("road repair recipe admission failed");
        }
        pending.Step = Stage::Recipe;
        return {};
      }
      break;
    case Stage::Verify: break;
  }
  return CollectRead(pending, one);
}

std::expected<void, std::string> RoadPageRestoration::CollectRead(Pending &pending,
                                                                  RoadPageQueue::Result &result) {
  if (!result) { return std::unexpected(std::move(result.error())); }
  if (!*result) {
    Missing_ = true;
    for (const auto &page : Window_) { Queue_.Cancel(Manifest_->Pages[page.Index].Key); }
    return {};
  }
  const auto &reference = Manifest_->Pages[pending.Index];
  const auto *loaded = std::get_if<Generators::RoadPageLease>(&**result);
  if (loaded != nullptr && loaded->Record.Key != reference.Key) {
    if (!loaded->Page || !Display_ || pending.Step != Stage::Read) {
      return std::unexpected("road restoration received an unrequested display page");
    }
    const auto &address = loaded->Page->Address;
    pending.Display = reference;
    auto &display = *pending.Display;
    display.Key = loaded->Record.Key;
    display.Level = address.Level;
    display.PositionErrorM = address.PositionErrorM;
    display.DisplayPolicy = address.DisplayPolicy;
    display.Bounds = loaded->Record.Bounds;
    display.ByteCount = loaded->Record.ByteCount;
  }
  const auto collected = CollectPayload(ActiveReference(pending), **result);
  if (collected && pending.Step == Stage::Verify) { ++RecoveredPages_; }
  return collected;
}

bool RoadPageRestoration::CanRepair(const Generators::RoadPageReference &reference) const noexcept {
  return RepairEnabled_ && !reference.PositionErrorM;
}

std::expected<void, std::string>
RoadPageRestoration::Repair(Pending &pending, const Generators::RoadPageReference &reference) {
  const auto admitted =
      Queue_.RequestRecord({.Address = Generators::StoredRoadPageAddress(*Manifest_, reference),
                            .Parent = {},
                            .RequestKey = {},
                            .NativeBytesMost = kPageBytesMost},
                           pending.Producer);
  if (!admitted) {
    Log::Warn(LogTag::World,
              "road_page_recipe_repair_failed",
              {{"page", reference.Key}, {"reason", admitted.error()}});
    pending.Producer.reset();
    return VerifyRepair(pending, reference);
  }
  if (*admitted) {
    pending.Producer.reset();
    pending.Step = Stage::Publish;
  }
  return {};
}

std::expected<void, std::string>
RoadPageRestoration::VerifyRepair(Pending &pending,
                                  const Generators::RoadPageReference &reference) {
  if (!Queue_.RequestCached(reference.Key, {.Key = ManifestKey_})) {
    return std::unexpected("road repair verification admission failed");
  }
  pending.Step = Stage::Verify;
  return {};
}

std::expected<void, std::string>
RoadPageRestoration::CollectPayload(const Generators::RoadPageReference &reference,
                                    RoadPageQueue::Product &product) {
  auto *loaded = std::get_if<Generators::RoadPageLease>(&product);
  if (loaded == nullptr) {
    return std::unexpected("road restoration received metadata instead of payload");
  }
  Loaded_ = std::move(loaded->Page);
  if (!Loaded_ || Loaded_->Address != Generators::StoredRoadPageAddress(*Manifest_, reference) ||
      loaded->NativeBytes != reference.ByteCount || loaded->Record.Bounds != reference.Bounds ||
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
  if (Window_.front().Display) { ++DisplayPages_; }
  WindowBytes_ -= static_cast<size_t>(Manifest_->Pages[Window_.front().Index].ByteCount);
  Window_.pop_front();
  ++Next_;
  Loaded_.reset();
}
}
