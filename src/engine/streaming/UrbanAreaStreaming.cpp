#include "UrbanAreaStreaming.h"
#include "UrbanAreaTerrainPrisms.h"
#include "TerrainResidency.h"
#include "SceneRenderer.h"
#include "RenderFrame.h"
#include <algorithm>
#include <chrono>
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
constexpr size_t kLookaheadUpdates = 4;
constexpr double kMostAwaitS = 0.01;

double ResolvedRadius(const UrbanAreaStreaming::View &view) {
  const Generators::Osm::UrbanAreaRecipe recipe;
  return std::min(view.RadiusM,
                  recipe.ProjectedPitchM * std::cos(view.Focus.LatitudeDeg * kDeg2Rad) *
                      view.FocalPx);
}

bool IntersectsSweep(Vec3 offset, double reach, Vec3 ahead, double aheadSquared) {
  const double nearest = std::clamp(Dot(offset, ahead) / aheadSquared, 0.0, 1.0);
  const Vec3 remaining = offset - ahead * nearest;
  return Dot(remaining, remaining) <= reach * reach;
}

std::expected<std::vector<Data::TileId>, std::string>
SourceTiles(const UrbanAreaStreaming::View &view) {
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
  std::vector<Data::TileId> wanted;
  wanted.reserve(count);
  for (long row = north; row <= south; ++row) {
    for (long column = west; column <= east; ++column) {
      long wrapped = column;
      if (!Ground::WrapTile(kSourceZoom, &wrapped, &row)) { continue; }
      wanted.push_back({.Zoom = kSourceZoom,
                        .X = static_cast<uint32_t>(wrapped),
                        .Y = static_cast<uint32_t>(row)});
    }
  }
  return wanted;
}

Vec3 PathEye(const TangentFrame &frame, const Vec3 &position) {
  return frame.ToEcefPosition(
      {.EastM = position[0], .NorthM = RenderFrame::NorthOfZ(position[2]), .UpM = position[1]});
}

std::expected<std::vector<Data::TileId>, std::string>
SourceTilesAlongPath(const UrbanAreaStreaming::View &view, std::span<const Vec3> positions) {
  auto tiles = SourceTiles(view);
  if (!tiles) { return tiles; }
  for (const auto &position : positions) {
    const auto eye = PathEye(view.Frame, position);
    const auto point = Ground::EcefToGeoWgs84({.X = eye[0], .Y = eye[1], .Z = eye[2]});
    if (!std::isfinite(point.LatitudeDeg) || !std::isfinite(point.LongitudeDeg)) { break; }
    auto future = view;
    future.Focus = {.LongitudeDeg = point.LongitudeDeg, .LatitudeDeg = point.LatitudeDeg};
    const auto requested = SourceTiles(future);
    if (!requested) { break; }
    const size_t previous = tiles->size();
    for (const auto tile : *requested) {
      if (!std::ranges::contains(*tiles, tile)) { tiles->push_back(tile); }
    }
    if (tiles->size() > kSourcesMost) {
      tiles->resize(previous);
      break;
    }
  }
  return tiles;
}

}

UrbanAreaStreaming::UrbanAreaStreaming(std::shared_ptr<Generators::Osm::PreparedUrbanAreas> sources,
                                       Generators::Osm::MvtSchema schema)
    : Sources_(std::move(sources)), Schema_(schema) {}

UrbanAreaStreaming::~UrbanAreaStreaming() {
  for (auto &group : Groups_) { Retire(group); }
}

void UrbanAreaStreaming::Into(Render::SceneRenderer &renderer) noexcept {
  Renderer_ = &renderer;
}

void UrbanAreaStreaming::Retire(Group &group) {
  if (Renderer_ != nullptr && group.Handle) { Renderer_->ReleasePrism(group.Handle); }
  group.Handle = {};
  group.Slot->Clear();
  if (group.Slot->InFlight()) { Retiring_.push_back(std::move(group.Slot)); }
}

std::expected<void, std::string> UrbanAreaStreaming::PlanSources(const View &view) {
  const auto tiles = SourceTiles(view);
  if (!tiles) { return std::unexpected(tiles.error()); }
  if (MotionSources_ && std::ranges::all_of(*tiles, [this](Data::TileId tile) {
        return std::ranges::contains(Areas_, tile, &Source::Owner);
      })) {
    for (auto &area : Areas_) { area.Required = std::ranges::contains(*tiles, area.Owner); }
    return {};
  }
  MotionSources_ = false;
  std::vector<Source> wanted;
  wanted.reserve(tiles->size());
  for (const auto tile : *tiles) {
    const auto held = std::ranges::find(Areas_, tile, &Source::Owner);
    if (held != Areas_.end()) {
      held->Required = true;
      wanted.push_back(std::move(*held));
    } else {
      wanted.push_back(NewSource(tile, true));
    }
  }
  Areas_ = std::move(wanted);
  NextSource_ = 0;
  return {};
}

UrbanAreaStreaming::Source UrbanAreaStreaming::NewSource(Data::TileId tile, bool required) const {
  return {.Owner = tile,
          .Request = {tile, Schema_, {.PatchSide = 32}},
          .SourceDelivery = {},
          .OccupiedGroups = {},
          .Required = required};
}

std::expected<bool, std::string> UrbanAreaStreaming::PollSource() {
  using State = Generators::Osm::PreparedUrbanAreas::State;
  for (size_t visited = 0; visited < Areas_.size(); ++visited) {
    Source &area = Areas_[NextSource_++ % Areas_.size()];
    if (area.SourceDelivery.Status != State::Pending) { continue; }
    auto delivery = Sources_->Acquire(area.Request);
    if (!delivery) { return std::unexpected(delivery.error()); }
    area.SourceDelivery = std::move(*delivery);
    if (area.SourceDelivery.Status == State::Absent ||
        area.SourceDelivery.Status == State::Refused) {
      return std::unexpected(
          "urban source unavailable at " + std::to_string(area.Owner.Zoom) + "/" +
          std::to_string(area.Owner.X) + "/" + std::to_string(area.Owner.Y) +
          (area.SourceDelivery.Failure ? ": " + area.SourceDelivery.Failure->Describe()
                                       : std::string{}));
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
UrbanAreaStreaming::SelectGroups(const View &view,
                                 const TerrainResidency &terrain,
                                 Vec3 ahead) const {
  const auto heights = terrain.SampleAslBoundsM();
  if (!heights) { return std::unexpected("urban demand requires published terrain bounds"); }
  const double radius = ResolvedRadius(view);
  const double aheadSquared = Dot(ahead, ahead);
  const bool moving = aheadSquared > 0 && aheadSquared <= radius * radius;
  std::vector<Candidate> candidates;
  std::optional<Candidate> upcoming;
  for (size_t source = 0; source < Areas_.size(); ++source) {
    const auto &input = Areas_[source];
    const auto &area = input.SourceDelivery.Product;
    if (!area || area->Occupied == 0 || (!input.Required && !moving)) { continue; }
    auto groups = Generators::Osm::UrbanAreaGroups::Make(
        *area, {.PatchesPerSide = kPatchesPerGroupSide, .GroupsMost = 1024});
    if (!groups) { return std::unexpected(std::string(groups.error())); }
    for (const size_t group : input.OccupiedGroups) {
      const auto range = groups->At(group);
      if (!range) { return std::unexpected("urban group has no intrinsic range"); }
      const auto bounds = area->BoundPatch(*range, *heights);
      if (!bounds) { return std::unexpected(std::string(bounds.error())); }
      const Vec3 offset = bounds->CentreEcef - view.EyeEcef;
      const double distance = Length(offset) - bounds->RadiusM;
      if (input.Required && distance <= radius) {
        candidates.push_back({.Source = source, .Group = group, .DistanceM = distance});
      } else if (moving && (!upcoming || distance < upcoming->DistanceM) &&
                 IntersectsSweep(offset, radius + bounds->RadiusM, ahead, aheadSquared)) {
        upcoming =
            Candidate{.Source = source, .Group = group, .DistanceM = distance, .Required = false};
      }
    }
  }
  return OrderGroupCandidates(std::move(candidates), upcoming);
}

std::expected<std::vector<UrbanAreaStreaming::Candidate>, std::string>
UrbanAreaStreaming::OrderGroupCandidates(std::vector<Candidate> candidates,
                                         std::optional<Candidate> upcoming) {
  if (candidates.size() > kGroupsMost) {
    return std::unexpected("resolved urban group demand " + std::to_string(candidates.size()) +
                           " exceeds residency budget " + std::to_string(kGroupsMost));
  }
  std::ranges::sort(candidates, {}, &Candidate::DistanceM);
  if (upcoming && candidates.size() < kGroupsMost) { candidates.push_back(*upcoming); }
  return candidates;
}

std::expected<void, std::string>
UrbanAreaStreaming::PlanGroups(const View &view, const TerrainResidency &terrain, Vec3 ahead) {
  auto candidates = SelectGroups(view, terrain, ahead);
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
      held->Required = candidate.Required;
      if (!held->Required && held->Handle) {
        Renderer_->ReleasePrism(held->Handle);
        held->Handle = {};
        held->PublishedKey.clear();
      }
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
                      .Slot = std::make_unique<UrbanPrismSlot>(Sources_, kAssetBytesMost),
                      .Handle = {},
                      .Exclusions = {},
                      .ChildrenRevision = 0,
                      .Required = candidate.Required});
  }
  for (auto &group : Groups_) {
    if (group.Slot) { Retire(group); }
  }
  Groups_ = std::move(wanted);
  Planned_ = true;
  return {};
}

std::expected<void, std::string> UrbanAreaStreaming::BindRequests(const TerrainResidency &terrain,
                                                                  bool terrainChanged) {
  for (auto &group : Groups_) {
    if (!terrainChanged && !group.Request.empty()) { continue; }
    const auto identity = UrbanPrismTerrainIdentity(
        *group.Area,
        group.Patches,
        terrain,
        {.PatchesMost = kPatchesMost, .RegionsMost = 16, .PagesMost = 4096});
    if (!identity) {
      if (!group.Required) {
        group.Slot->Clear();
        group.Request.clear();
        continue;
      }
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
  return {};
}

std::expected<void, std::string>
UrbanAreaStreaming::Advance(const View &view,
                            const TerrainResidency &terrain,
                            uint64_t terrainRevision,
                            std::span<const Ground::GeoBounds> children,
                            uint64_t childrenRevision,
                            std::optional<Vec3> knownDisplacement) {
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
  if (!Planned_ || moved || changed || *acquired || knownDisplacement) {
    const Vec3 ahead = knownDisplacement.value_or(
        View_ ? (view.EyeEcef - View_->EyeEcef) * static_cast<double>(kLookaheadUpdates) : Vec3{});
    if (auto planned = PlanGroups(view, terrain, ahead); !planned) { return planned; }
    if (auto bound = BindRequests(terrain, changed); !bound) { return bound; }
    View_ = view;
    TerrainRevision_ = terrainRevision;
  }
  return AdvanceGroups(view, terrain, children, childrenRevision);
}

std::expected<void, std::string>
UrbanAreaStreaming::PrepareMotionSources(std::span<const Vec3> positions,
                                         const View &view,
                                         std::chrono::steady_clock::time_point deadline) {
  if (MotionSources_) {
    MotionSources_ = false;
    std::erase_if(Areas_, [](const Source &area) { return !area.Required; });
    NextSource_ = 0;
  }
  const auto tiles = SourceTilesAlongPath(view, positions);
  if (!tiles) { return std::unexpected(tiles.error()); }
  const size_t previous = Areas_.size();
  for (const auto tile : *tiles) {
    if (!std::ranges::contains(Areas_, tile, &Source::Owner)) {
      Areas_.push_back(NewSource(tile, false));
    }
  }
  if (Areas_.size() == previous) { return {}; }
  using State = Generators::Osm::PreparedUrbanAreas::State;
  while (std::ranges::any_of(
      Areas_, [](const Source &area) { return area.SourceDelivery.Status != State::Ready; })) {
    const auto progress = PollSource();
    if (!progress) { return std::unexpected(progress.error()); }
    const double remaining =
        std::chrono::duration<double>(deadline - std::chrono::steady_clock::now()).count();
    if (remaining <= 0) { return std::unexpected("urban route source preparation timed out"); }
    if (!*progress) { (void)Sources_->AwaitProgress(std::min(remaining, kMostAwaitS)); }
  }
  MotionSources_ = true;
  return {};
}

std::expected<void, std::string>
UrbanAreaStreaming::PrepareMotion(std::span<const Vec3> positions,
                                  const TerrainResidency &terrain,
                                  uint64_t terrainRevision,
                                  std::span<const Ground::GeoBounds> children,
                                  uint64_t childrenRevision,
                                  std::chrono::steady_clock::time_point deadline) {
  if (!View_ || positions.size() < 2 || std::chrono::steady_clock::now() >= deadline) { return {}; }
  const View view = *View_;
  if (auto ready = PrepareMotionSources(positions, view, deadline); !ready) { return ready; }
  const Vec3 endpoint = positions[std::min(kLookaheadUpdates, positions.size() - 1)];
  const Vec3 ahead = PathEye(view.Frame, endpoint) - view.EyeEcef;
  if (auto begun = Advance(view, terrain, terrainRevision, children, childrenRevision, ahead);
      !begun) {
    return begun;
  }
  if (Groups_.empty() || Groups_.back().Required) { return {}; }
  const Group &group = Groups_.back();
  while (!group.Request.empty() && group.Slot->ReadyKey() != group.Request) {
    const double remaining =
        std::chrono::duration<double>(deadline - std::chrono::steady_clock::now()).count();
    if (remaining <= 0) { break; }
    (void)AwaitProgress(std::min(remaining, kMostAwaitS));
    if (auto advanced = AdvanceGroups(view, terrain, children, childrenRevision); !advanced) {
      return advanced;
    }
  }
  return {};
}

std::expected<bool, std::string> UrbanAreaStreaming::LoadGroup(Group &group,
                                                               const TerrainResidency &terrain,
                                                               UrbanPrismSlot::Admission admission,
                                                               bool &captured) {
  return group.Slot->Advance(
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
    if (group.Request.empty()) { continue; }
    const bool wasAdmitted = group.Slot->HoldsAdmission();
    const size_t limit = group.Required ? kJobsMost : kJobsMost - 1;
    const auto admission = wasAdmitted || admitted < limit ? UrbanPrismSlot::Admission::AllowWork
                                                           : UrbanPrismSlot::Admission::CollectOnly;
    auto advanced = LoadGroup(group, terrain, admission, captured);
    admitted = admitted - static_cast<size_t>(wasAdmitted) +
               static_cast<size_t>(group.Slot->HoldsAdmission());
    if (!advanced) {
      if (group.Required) { return std::unexpected(advanced.error()); }
      group.Slot->Clear();
      group.Request.clear();
      continue;
    }
    if (!group.Required || Renderer_ == nullptr || !group.Slot->Ready()) { continue; }
    const auto &key = group.Slot->ReadyKey();
    if (key != group.Request && key != group.PublishedKey) { continue; }
    auto placed = Publish(group, view, children, childrenRevision, !published);
    if (!placed) { return std::unexpected(placed.error()); }
    published = published || *placed;
  }
  return {};
}

bool UrbanAreaStreaming::Ready() const noexcept {
  return Blocker().empty();
}

std::string_view UrbanAreaStreaming::Blocker() const noexcept {
  using State = Generators::Osm::PreparedUrbanAreas::State;
  if (!Planned_ || std::ranges::any_of(Areas_, [](const Source &area) {
        return area.Required && area.SourceDelivery.Status != State::Ready;
      })) {
    return "pending resolved urban sources";
  }
  for (const auto &group : Groups_) {
    if (!group.Required) { continue; }
    if (group.Slot->ReadyKey() != group.Request) { return "pending resolved urban geometry"; }
    if (group.PublishedKey != group.Request) { return "pending resolved urban upload"; }
    if (group.ChildrenRevision != WantedChildrenRevision_) {
      return "pending resolved urban child exclusions";
    }
  }
  return {};
}

bool UrbanAreaStreaming::AwaitProgress(double seconds) const {
  for (const auto &slot : Retiring_) {
    if (slot->HoldsAdmission()) { return slot->AwaitProgress(seconds); }
  }
  for (const auto &group : Groups_) {
    if (group.Slot->HoldsAdmission()) { return group.Slot->AwaitProgress(seconds); }
  }
  return Sources_->AwaitProgress(seconds);
}

UrbanAreaStreaming::Counters UrbanAreaStreaming::Costs() const noexcept {
  Counters costs{.Sources = Areas_.size(),
                 .Groups = 0,
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
    if (group.Required) { ++costs.Groups; }
    if (group.Handle) { ++costs.Resident; }
    if (group.Slot->InFlight()) { ++costs.InFlight; }
    if (group.Slot->HoldsAdmission()) { ++costs.Admitted; }
  }
  return costs;
}
}
