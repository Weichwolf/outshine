#include "UrbanAreaStreaming.h"
#include "UrbanAreaTerrainPrisms.h"
#include "TerrainResidency.h"
#include "SceneRenderer.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <vector>
#include <utility>

namespace outshine {
namespace {
constexpr int kSourceZoom = 11;
constexpr size_t kSourcesMost = 64, kGroupsMost = 512;
constexpr size_t kPatchesPerGroupSide = 4, kPatchesMost = 16;
constexpr size_t kSamplesMost = 16384, kAssetBytesMost = 262144, kJobsMost = 2;

double ResolvedRadius(const UrbanAreaStreaming::View &view) {
  const Generators::Osm::UrbanAreaRecipe recipe;
  return std::min(view.RadiusM,
                  recipe.ProjectedPitchM * std::cos(view.Focus.LatitudeDeg * kDeg2Rad) *
                      view.FocalPx);
}

}

UrbanAreaStreaming::UrbanAreaStreaming(Tasks &pool,
                                       std::shared_ptr<Generators::Osm::PreparedUrbanAreas> sources,
                                       Data::SourceReader &reader,
                                       Generators::Osm::MvtSchema schema,
                                       std::string database)
    : Pool_(pool),
      Sources_(std::move(sources)),
      Reader_(reader),
      Schema_(schema),
      Database_(std::move(database)) {}

UrbanAreaStreaming::~UrbanAreaStreaming() {
  for (auto &group : Groups_) { Retire(group); }
}

void UrbanAreaStreaming::Into(Render::SceneRenderer &renderer) noexcept {
  Renderer_ = &renderer;
}

void UrbanAreaStreaming::Retire(Group &group) {
  if (Renderer_ != nullptr && group.Handle) { Renderer_->ReleasePrismGrid(group.Handle); }
  group.Handle = {};
  group.Slot->Clear();
  if (group.Slot->InFlight()) { Retiring_.push_back(std::move(group.Slot)); }
}

std::expected<void, std::string> UrbanAreaStreaming::PlanSources(const View &view) {
  const double radius = ResolvedRadius(view);
  const double angle = radius / (kWgs84A * (1 - kWgs84F) * (1 - kWgs84F));
  const double latitude = view.Focus.LatitudeDeg * kDeg2Rad;
  const double longitudeSpan =
      std::abs(latitude) + angle >= kPi * .5
          ? kPi
          : std::asin(std::clamp(std::sin(angle) / std::cos(latitude), 0., 1.));
  const auto northWest = Ground::ToTileFracClamped(
      {.LongitudeDeg = 0, .LatitudeDeg = view.Focus.LatitudeDeg + angle * kRad2Deg}, kSourceZoom);
  const auto southEast = Ground::ToTileFracClamped(
      {.LongitudeDeg = 0, .LatitudeDeg = view.Focus.LatitudeDeg - angle * kRad2Deg}, kSourceZoom);
  const double tiles = std::ldexp(1., kSourceZoom);
  const auto west = static_cast<long>(
      std::floor((view.Focus.LongitudeDeg + 180. - longitudeSpan * kRad2Deg) / 360. * tiles));
  const auto east = static_cast<long>(
      std::floor((view.Focus.LongitudeDeg + 180. + longitudeSpan * kRad2Deg) / 360. * tiles));
  const auto north = static_cast<long>(std::floor(northWest.Y));
  const auto south =
      std::min(static_cast<long>(tiles) - 1, static_cast<long>(std::floor(southEast.Y)));
  const auto count = static_cast<size_t>(east - west + 1) * static_cast<size_t>(south - north + 1);
  if (count > kSourcesMost) {
    return std::unexpected("resolved urban source demand exceeds budget");
  }
  std::vector<Source> wanted;
  wanted.reserve(count);
  for (long row = north; row <= south; ++row) {
    for (long column = west; column <= east; ++column) {
      long wrapped = column;
      if (!Ground::WrapTile(kSourceZoom, &wrapped, &row)) { continue; }
      const Data::TileId tile{.Zoom = kSourceZoom,
                              .X = static_cast<uint32_t>(wrapped),
                              .Y = static_cast<uint32_t>(row)};
      const auto held = std::ranges::find(Areas_, tile, &Source::Owner);
      if (held != Areas_.end()) {
        wanted.push_back(std::move(*held));
      } else {
        wanted.push_back({.Owner = tile,
                          .Request = {tile, Schema_, {.PatchSide = 32}},
                          .SourceDelivery = {},
                          .OccupiedGroups = {}});
      }
    }
  }
  Areas_ = std::move(wanted);
  NextSource_ = 0;
  return {};
}

std::expected<bool, std::string> UrbanAreaStreaming::PollSource() {
  using State = Generators::Osm::PreparedUrbanAreas::State;
  for (size_t visited = 0; visited < Areas_.size(); ++visited) {
    Source &area = Areas_[NextSource_++ % Areas_.size()];
    if (area.SourceDelivery.Status != State::Pending) { continue; }
    auto delivery = Sources_->Acquire(Reader_, area.Request);
    if (!delivery) { return std::unexpected(delivery.error()); }
    area.SourceDelivery = std::move(*delivery);
    if (area.SourceDelivery.Status == State::Absent ||
        area.SourceDelivery.Status == State::Refused) {
      return std::unexpected("urban source unavailable at " + std::to_string(area.Owner.Zoom) +
                             "/" + std::to_string(area.Owner.X) + "/" +
                             std::to_string(area.Owner.Y));
    }
    if (area.SourceDelivery.Status != State::Ready) { return false; }
    if (!area.SourceDelivery.Product) {
      return std::unexpected("ready urban source has no product");
    }
    if (area.SourceDelivery.Product->Occupied == 0) { return true; }
    const auto groups = Generators::Osm::UrbanAreaGroups::Make(
        *area.SourceDelivery.Product, {.PatchesPerSide = kPatchesPerGroupSide, .GroupsMost = 1024});
    if (!groups) { return std::unexpected(std::string(groups.error())); }
    auto occupied = groups->Occupied(*area.SourceDelivery.Product);
    if (!occupied) { return std::unexpected(std::string(occupied.error())); }
    area.OccupiedGroups = std::move(*occupied);
    return true;
  }
  return false;
}

std::expected<std::vector<UrbanAreaStreaming::Candidate>, std::string>
UrbanAreaStreaming::SelectGroups(const View &view, const TerrainResidency &terrain) const {
  const auto heights = terrain.SampleAslBoundsM();
  if (!heights) { return std::unexpected("urban demand requires published terrain bounds"); }
  std::vector<Candidate> candidates;
  for (size_t source = 0; source < Areas_.size(); ++source) {
    const auto &area = Areas_[source].SourceDelivery.Product;
    if (!area || area->Occupied == 0) { continue; }
    auto groups = Generators::Osm::UrbanAreaGroups::Make(
        *area, {.PatchesPerSide = kPatchesPerGroupSide, .GroupsMost = 1024});
    if (!groups) { return std::unexpected(std::string(groups.error())); }
    for (const size_t group : Areas_[source].OccupiedGroups) {
      const auto range = groups->At(group);
      if (!range) { return std::unexpected("urban group has no intrinsic range"); }
      const auto bounds = area->BoundPatch(*range, *heights);
      if (!bounds) { return std::unexpected(std::string(bounds.error())); }
      const double distance = Length(bounds->CentreEcef - view.EyeEcef) - bounds->RadiusM;
      if (distance <= ResolvedRadius(view)) {
        candidates.push_back({.Source = source, .Group = group, .DistanceM = distance});
      }
    }
  }
  if (candidates.size() > kGroupsMost) {
    return std::unexpected("resolved urban group demand " + std::to_string(candidates.size()) +
                           " exceeds residency budget " + std::to_string(kGroupsMost));
  }
  std::ranges::sort(candidates, {}, &Candidate::DistanceM);
  return candidates;
}

std::expected<void, std::string> UrbanAreaStreaming::PlanGroups(const View &view,
                                                                const TerrainResidency &terrain) {
  auto candidates = SelectGroups(view, terrain);
  if (!candidates) { return std::unexpected(candidates.error()); }

  std::vector<Group> wanted;
  wanted.reserve(candidates->size());
  for (const auto &candidate : *candidates) {
    const auto &source = Areas_[candidate.Source];
    const auto held = std::ranges::find_if(Groups_, [&](const Group &group) {
      return group.Owner == source.Owner && group.Index == candidate.Group &&
             group.SourceKey == source.SourceDelivery.AssetKey;
    });
    if (held != Groups_.end()) {
      wanted.push_back(std::move(*held));
      continue;
    }
    const auto groups = Generators::Osm::UrbanAreaGroups::Make(
        *source.SourceDelivery.Product,
        {.PatchesPerSide = kPatchesPerGroupSide, .GroupsMost = 1024});
    auto patches = groups->Patches({.Index = candidate.Group, .PatchesMost = kPatchesMost});
    if (!patches) { return std::unexpected(std::string(patches.error())); }
    wanted.push_back({.Owner = source.Owner,
                      .Index = candidate.Group,
                      .Area = source.SourceDelivery.Product,
                      .Patches = std::move(*patches),
                      .SourceKey = source.SourceDelivery.AssetKey,
                      .Request = {},
                      .PublishedKey = {},
                      .Slot = std::make_unique<UrbanPrismSlot>(Pool_, Database_, kAssetBytesMost),
                      .Handle = {},
                      .Exclusions = {},
                      .ChildrenRevision = 0});
  }
  for (auto &group : Groups_) {
    if (group.Slot) { Retire(group); }
  }
  Groups_ = std::move(wanted);
  Planned_ = true;
  return {};
}

std::expected<void, std::string>
UrbanAreaStreaming::Advance(const View &view,
                            const TerrainResidency &terrain,
                            uint64_t terrainRevision,
                            std::span<const Ground::GeoBounds> children,
                            uint64_t childrenRevision) {
  if (!std::isfinite(view.FocalPx) || view.FocalPx <= 0 || !std::isfinite(view.RadiusM) ||
      view.RadiusM <= 0 || !std::isfinite(view.Focus.LatitudeDeg) ||
      std::abs(view.Focus.LatitudeDeg) > kMercatorLatMaxDeg ||
      !std::isfinite(view.Focus.LongitudeDeg) ||
      std::abs(view.Focus.LongitudeDeg) > kDegPerHalfTurn) {
    return std::unexpected("invalid resolved urban view");
  }
  const bool moved = !View_ || View_->EyeEcef != view.EyeEcef || View_->FocalPx != view.FocalPx ||
                     View_->RadiusM != view.RadiusM;
  const bool changed = !TerrainRevision_ || *TerrainRevision_ != terrainRevision;
  if (moved) {
    if (auto planned = PlanSources(view); !planned) { return planned; }
  }
  auto acquired = PollSource();
  if (!acquired) { return std::unexpected(acquired.error()); }
  if (!terrain.HasPublishedSurface()) { return {}; }
  if (!Planned_ || moved || changed || *acquired) {
    if (auto planned = PlanGroups(view, terrain); !planned) { return planned; }
    View_ = view;
    TerrainRevision_ = terrainRevision;
    for (auto &group : Groups_) {
      if (!changed && !group.Request.empty()) { continue; }
      const auto identity = UrbanPrismTerrainIdentity(
          *group.Area,
          group.Patches,
          terrain,
          {.PatchesMost = kPatchesMost, .RegionsMost = 16, .PagesMost = 4096});
      if (!identity) {
        return std::unexpected("urban terrain identity failure " +
                               std::to_string(static_cast<int>(identity.error())) + " at " +
                               std::to_string(group.Owner.Zoom) + "/" +
                               std::to_string(group.Owner.X) + "/" + std::to_string(group.Owner.Y));
      }
      auto request =
          Generators::Osm::UrbanPrismRequestKey(group.SourceKey, *identity, group.Patches, 0);
      if (!request) { return std::unexpected(request.error()); }
      group.Request = std::move(*request);
    }
  }
  return AdvanceGroups(view, terrain, children, childrenRevision);
}

std::expected<void, std::string>
UrbanAreaStreaming::AdvanceGroups(const View &view,
                                  const TerrainResidency &terrain,
                                  std::span<const Ground::GeoBounds> children,
                                  uint64_t childrenRevision) {
  std::erase_if(Retiring_, [](const auto &slot) { return !slot->InFlight(); });
  WantedChildrenRevision_ = childrenRevision;
  size_t admitted = Costs().Admitted;
  bool captured = false;
  bool published = false;
  for (auto &group : Groups_) {
    const bool wasAdmitted = group.Slot->HoldsAdmission();
    const auto admission = wasAdmitted || admitted < kJobsMost
                               ? UrbanPrismSlot::Admission::AllowWork
                               : UrbanPrismSlot::Admission::CollectOnly;
    auto advanced = group.Slot->Advance(
        group.Request,
        [&](const std::stop_token &stop)
            -> std::optional<Generators::Osm::UrbanPrismPreparation::Captured> {
          if (captured) { return std::nullopt; }
          captured = true;
          ++Captures_;
          return CaptureUrbanPrismFactory(group.Area,
                                          group.Patches,
                                          terrain,
                                          group.Request,
                                          kSamplesMost,
                                          {.BytesMost = kAssetBytesMost},
                                          stop);
        },
        admission);
    if (!advanced) { return std::unexpected(advanced.error()); }
    const bool isAdmitted = group.Slot->HoldsAdmission();
    if (isAdmitted && !wasAdmitted) { ++admitted; }
    if (!isAdmitted && wasAdmitted) { --admitted; }
    if (published || Renderer_ == nullptr || !group.Slot->Ready()) { continue; }
    auto placed = Publish(group, view, children, childrenRevision);
    if (!placed) { return std::unexpected(placed.error()); }
    published = *placed;
  }
  return {};
}

bool UrbanAreaStreaming::Ready() const noexcept {
  using State = Generators::Osm::PreparedUrbanAreas::State;
  return Planned_ &&
         std::ranges::all_of(
             Areas_,
             [](const Source &area) { return area.SourceDelivery.Status == State::Ready; }) &&
         std::ranges::all_of(Groups_, [this](const Group &group) {
           return group.Slot->ReadyKey() == group.Request && group.PublishedKey == group.Request &&
                  group.ChildrenRevision == WantedChildrenRevision_;
         });
}

bool UrbanAreaStreaming::AwaitProgress(double seconds) const {
  for (const auto &slot : Retiring_) {
    if (slot->InFlight()) { return slot->AwaitProgress(seconds); }
  }
  for (const auto &group : Groups_) {
    if (group.Slot->InFlight()) { return group.Slot->AwaitProgress(seconds); }
  }
  return Sources_->AwaitProgress(seconds);
}

UrbanAreaStreaming::Counters UrbanAreaStreaming::Costs() const noexcept {
  Counters costs{.Sources = Areas_.size(),
                 .Groups = Groups_.size(),
                 .Resident = 0,
                 .InFlight = Retiring_.size(),
                 .Admitted = 0,
                 .Missing = 0,
                 .Captures = Captures_,
                 .Publications = Publications_};
  for (const auto &slot : Retiring_) {
    if (slot->HoldsAdmission()) { ++costs.Admitted; }
  }
  for (const auto &area : Areas_) {
    if (!area.SourceDelivery.Product) { ++costs.Missing; }
  }
  for (const auto &group : Groups_) {
    if (group.Handle) { ++costs.Resident; }
    if (group.Slot->InFlight()) { ++costs.InFlight; }
    if (group.Slot->HoldsAdmission()) { ++costs.Admitted; }
  }
  return costs;
}
}
