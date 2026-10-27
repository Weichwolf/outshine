#include "TerrainInputPreparation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <ranges>
#include <span>
#include <tuple>
#include <utility>
#include <string>
#include <vector>

#include "ChunkSurface.h"
#include "Geodesy.h"
#include "TerrainGrid.h"
#include "TerrainSourceCoverage.h"
#include "TileGeodesy.h"
#include "Vec3.h"

namespace outshine::Generators {

namespace {
constexpr int kSide = kPatchGrid + 1;
constexpr int kPageSide = kSide + 2;
constexpr size_t kInteriorNodes = static_cast<size_t>(kSide) * kSide;
constexpr size_t kPageNodes = static_cast<size_t>(kPageSide) * kPageSide;

[[nodiscard]] auto TileKey(Data::TileId tile) {
  return std::tuple(tile.Zoom, tile.X, tile.Y);
}

[[nodiscard]] double FractionOf(int k, uint32_t postings, int side) {
  return static_cast<double>(::outshine::Ground::ChunkNodePosting(k, postings, side)) /
         static_cast<double>(postings - 1u);
}

[[nodiscard]] int SourceZoomOf(const Sheet &sheet, int finestZoom) {
  if (sheet.SourceZoom >= 0) { return sheet.SourceZoom; }
  return sheet.Virtual ? finestZoom : sheet.Tile.Zoom;
}

[[nodiscard]] double NodeFraction(const Sheet &sheet, int k) {
  constexpr int side = kSide;
  if (sheet.Virtual || sheet.SourceZoom >= 0) {
    return static_cast<double>(k) / static_cast<double>(side - 1);
  }
  if (k < 0) { return -FractionOf(1, sheet.Postings, side); }
  if (k >= side) { return 2.0 - FractionOf(side - 2, sheet.Postings, side); }
  return FractionOf(k, sheet.Postings, side);
}

[[nodiscard]] size_t PageNode(int i, int j) {
  constexpr int pageSide = kPageSide;
  return static_cast<size_t>(j + 1) * static_cast<size_t>(pageSide) + static_cast<size_t>(i + 1);
}

void CopiesEdgeIntoRim(std::vector<float> &page, const std::vector<bool> &missing) {
  constexpr int side = kSide;
  for (int j = -1; j <= side; ++j) {
    for (int i = -1; i <= side; ++i) {
      if (!missing[PageNode(i, j)]) { continue; }
      page[PageNode(i, j)] = page[PageNode(std::clamp(i, 0, side - 1), std::clamp(j, 0, side - 1))];
    }
  }
}

[[nodiscard]] std::expected<std::vector<Data::TileId>, std::string>
SourceTilesFor(const Patchwork &candidate,
               TerrainInputPreparation::FieldPreparation preparation,
               int groundZoom) {
  constexpr size_t kMaximumAdditionalTiles = 256;
  if (preparation.AdditionalTiles.size() > kMaximumAdditionalTiles) {
    return std::unexpected("terrain field preparation exceeds its 256 additional tile budget");
  }
  if (preparation.FinestZoom < 0 ||
      preparation.FinestZoom > ::outshine::Ground::HeightField::MaximumTileZoom) {
    return std::unexpected("terrain field preparation has an invalid source zoom");
  }
  const uint32_t side = uint32_t{1} << static_cast<uint32_t>(preparation.FinestZoom);
  for (const Data::TileId tile : preparation.AdditionalTiles) {
    if (tile.Zoom != preparation.FinestZoom || tile.X >= side || tile.Y >= side) {
      return std::unexpected("additional terrain tile is outside the candidate source grid");
    }
  }
  std::vector<Data::TileId> sources;
  sources.reserve(candidate.Sheets.size());
  for (const Sheet &sheet : candidate.Sheets) {
    const int zoom = SourceZoomOf(sheet, preparation.FinestZoom);
    if (zoom < 0 || zoom > sheet.Tile.Zoom ||
        sheet.Tile.Zoom > ::outshine::Ground::HeightField::MaximumTileZoom) {
      return std::unexpected("terrain sheet has an invalid source zoom");
    }
    const auto drop = static_cast<uint32_t>(sheet.Tile.Zoom - zoom);
    sources.push_back({.Zoom = zoom, .X = sheet.Tile.X >> drop, .Y = sheet.Tile.Y >> drop});
  }
  auto tiles = PlanTerrainSourceTiles(sources,
                                      {.FinestZoom = preparation.FinestZoom,
                                       .GroundZoom = groundZoom,
                                       .Vectors = preparation.Vectors,
                                       .BuildingFootprints = preparation.BuildingFootprints,
                                       .AdditionalTiles = preparation.AdditionalTiles});
  return tiles;
}

}

const ::outshine::Ground::TerrainField *TerrainInputPreparation::FieldAt(Data::TileId tile) const {
  if (RequestsPrepared_ && ResolvedRequests_ == Requests_.size()) {
    const auto found = std::ranges::lower_bound(
        Fields_, TileKey(tile), {}, [](const auto &one) { return TileKey(one.first); });
    return found != Fields_.end() && found->first == tile ? found->second.get() : nullptr;
  }
  for (const auto &one : Fields_) {
    if (one.first == tile) { return one.second.get(); }
  }
  return nullptr;
}

bool TerrainInputPreparation::ShareSourcedField(
    Data::TileId tile, ::outshine::Ground::HeightField::Block &into) const {
  return TerrainFieldSet::Share(Fields_, tile, into);
}

TerrainFieldSet TerrainInputPreparation::SnapshotSourceFields() const {
  return TerrainFieldSet(Fields_, Metadata_);
}

std::expected<bool, std::string>
TerrainInputPreparation::PrepareFields(const Patchwork &candidate,
                                       const ::outshine::Ground::GroundStream &ground,
                                       FieldPreparation preparation) {
  if (!RequestsPrepared_) {
    auto tiles = SourceTilesFor(candidate, preparation, ground.BlockZoom());
    if (!tiles) { return std::unexpected(tiles.error()); }
    auto metadata = ground.PrepareSourceMetadata(*tiles);
    if (!metadata) {
      return std::unexpected("terrain source metadata exceeds its resident dependency budget");
    }
    Reset();
    Metadata_ = std::move(*metadata);
    Requests_.reserve(tiles->size());
    Fields_.reserve(tiles->size());
    for (const Data::TileId tile : *tiles) { Requests_.push_back({.Tile = tile}); }
    RequestsPrepared_ = true;
  }
  if (ResolvedRequests_ == Requests_.size()) { return true; }
  for (size_t checked = 0;
       checked < preparation.RequestsMost && ResolvedRequests_ < Requests_.size();
       ++checked) {
    FieldRequest &request = Requests_[NextRequest_];
    NextRequest_ = (NextRequest_ + 1u) % Requests_.size();
    if (request.Resolved) { continue; }
    std::shared_ptr<const ::outshine::Ground::TerrainField> field;
    const ::outshine::Ground::TilePool::Reply status =
        ground.PollStitchedField(request.Tile, field);
    switch (status) {
      case ::outshine::Ground::TilePool::Reply::Ready:
        if (!field) { return std::unexpected("a ready height field has no data"); }
        Fields_.emplace_back(request.Tile, std::move(field));
        break;
      case ::outshine::Ground::TilePool::Reply::Absent:
      case ::outshine::Ground::TilePool::Reply::Undeclared:
        Fields_.emplace_back(request.Tile, nullptr);
        break;
      case ::outshine::Ground::TilePool::Reply::Refused:
        return std::unexpected("a terrain source refused a height field");
      case ::outshine::Ground::TilePool::Reply::Pending:
      case ::outshine::Ground::TilePool::Reply::Deferred: continue;
    }
    request.Resolved = true;
    ++ResolvedRequests_;
  }
  if (ResolvedRequests_ != Requests_.size()) { return false; }
  std::ranges::sort(Fields_, {}, [](const auto &one) { return TileKey(one.first); });
  return true;
}

std::string TerrainInputPreparation::FieldDiagnostic() const {
  std::string result = std::to_string(ResolvedRequests_) + "/" + std::to_string(Requests_.size());
  const auto pending = std::ranges::find(Requests_, false, &FieldRequest::Resolved);
  if (pending != Requests_.end()) {
    const auto tile = pending->Tile;
    result += " next=" + std::to_string(tile.Zoom) + "/" + std::to_string(tile.X) + "/" +
              std::to_string(tile.Y);
  }
  return result;
}

std::optional<float> TerrainInputPreparation::AslAt(int zoom,
                                                    ::outshine::Ground::TileFrac at) const {
  long x = static_cast<long>(std::floor(at.X));
  const long y = static_cast<long>(std::floor(at.Y));
  const double col = at.X - static_cast<double>(x);
  const double row = at.Y - static_cast<double>(y);
  if (!::outshine::Ground::WrapTile(zoom, &x, &y)) { return std::nullopt; }
  const ::outshine::Ground::TerrainField *field =
      FieldAt({.Zoom = zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)});
  if (field == nullptr || !field->Meshable()) { return std::nullopt; }
  return field->PostingM({.Col = col, .Row = row});
}

TerrainInputPreparation::HaloBuildJob::HaloBuildJob(TerrainInputPreparation &inputs,
                                                    Patchwork &candidate,
                                                    int finestZoom) noexcept
    : Inputs_(&inputs), Candidate_(&candidate), FinestZoom_(finestZoom) {
  Inputs_->RimsMissing_ = 0;
}

bool TerrainInputPreparation::HaloBuildJob::BeginsSheet() {
  constexpr int side = kSide;
  while (SheetAt_ < Candidate_->Sheets.size()) {
    const Sheet &sheet = Candidate_->Sheets[SheetAt_];
    if (sheet.Side != side) {
      ++SheetAt_;
      continue;
    }
    Whole_ = (sheet.Virtual || sheet.SourceZoom >= 0) && sheet.Nodes.size() != kInteriorNodes;
    if (!Whole_ && sheet.Nodes.size() != kInteriorNodes) {
      ++SheetAt_;
      continue;
    }
    const int sourceZoom = SourceZoomOf(sheet, FinestZoom_);
    const auto drop = static_cast<uint32_t>(sheet.Tile.Zoom - sourceZoom);
    Zoom_ = sheet.Tile.Zoom - static_cast<int>(drop);
    Span_ = 1.0 / static_cast<double>(1u << drop);
    AtX_ = static_cast<double>(sheet.Tile.X >> drop) +
           static_cast<double>(sheet.Tile.X & ((1u << drop) - 1u)) * Span_;
    AtY_ = static_cast<double>(sheet.Tile.Y >> drop) +
           static_cast<double>(sheet.Tile.Y & ((1u << drop) - 1u)) * Span_;
    Page_.assign(kPageNodes, 0.0f);
    Missing_.assign(kPageNodes, false);
    AnyMissing_ = false;
    NodeAt_ = 0;
    Working_ = true;
    return true;
  }
  return false;
}

bool TerrainInputPreparation::HaloBuildJob::AdvancesNode() {
  constexpr int side = kSide;
  constexpr auto pageSide = static_cast<size_t>(side) + 2u;
  Sheet &sheet = Candidate_->Sheets[SheetAt_];
  const int j = static_cast<int>(NodeAt_ / pageSide) - 1;
  const int i = static_cast<int>(NodeAt_ % pageSide) - 1;
  const bool inside = i >= 0 && i < side && j >= 0 && j < side;
  if (inside && !Whole_) {
    Page_[NodeAt_] =
        sheet.Nodes[static_cast<size_t>(j) * static_cast<size_t>(side) + static_cast<size_t>(i)];
    ++NodeAt_;
    return true;
  }
  const std::optional<float> asl = Inputs_->AslAt(
      Zoom_,
      {.X = AtX_ + Span_ * NodeFraction(sheet, i), .Y = AtY_ + Span_ * NodeFraction(sheet, j)});
  if (asl) {
    Page_[NodeAt_] = *asl;
  } else if (inside) {
    Working_ = false;
    ++SheetAt_;
    return false;
  } else {
    Missing_[NodeAt_] = true;
    AnyMissing_ = true;
  }
  ++NodeAt_;
  return true;
}

void TerrainInputPreparation::HaloBuildJob::CompletesSheet() {
  if (AnyMissing_) {
    CopiesEdgeIntoRim(Page_, Missing_);
    ++Inputs_->RimsMissing_;
  }
  Candidate_->Sheets[SheetAt_].Nodes = std::move(Page_);
  ++Haloed_;
  ++SheetAt_;
  Working_ = false;
}

bool TerrainInputPreparation::HaloBuildJob::Advance(size_t nodesMost) {
  while (nodesMost > 0) {
    if (!Working_ && !BeginsSheet()) { return true; }
    --nodesMost;
    if (!AdvancesNode()) { continue; }
    if (NodeAt_ == kPageNodes) { CompletesSheet(); }
  }
  return !Working_ && SheetAt_ == Candidate_->Sheets.size();
}

size_t TerrainInputPreparation::Halos(Patchwork &laid, int finestZoom) {
  HaloBuildJob job(*this, laid, finestZoom);
  while (!job.Advance(std::numeric_limits<size_t>::max())) {}
  return job.Haloed();
}

std::optional<double> TerrainInputPreparation::FieldUpM(int zoom, EastNorth at) const {
  if (!Framed_) { return std::nullopt; }
  const Vec3 &origin = Frame_.OriginEcef();
  const Vec3 &east = Frame_.EastEcef();
  const Vec3 &north = Frame_.NorthEcef();
  Vec3 ecef;
  for (int axis = 0; axis < 3; ++axis) {
    ecef[axis] = origin[axis] + at.EastM * east[axis] + at.NorthM * north[axis];
  }
  const ::outshine::Ground::Geo geo =
      ::outshine::Ground::EcefToGeoWgs84({.X = ecef[0], .Y = ecef[1], .Z = ecef[2]});
  const std::optional<double> aslM =
      AslMAt(zoom, {.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg});
  if (!aslM) { return std::nullopt; }
  return Frame_
      .ToLocalPosition(
          {.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg, .HeightM = *aslM})
      .UpM;
}

std::optional<double> TerrainInputPreparation::AslMAt(int zoom, LongitudeLatitude at) const {
  if (!Framed_) { return std::nullopt; }
  return TerrainFieldSet::AslMAt(zoom, at, [this](Data::TileId tile) { return FieldAt(tile); });
}

size_t TerrainInputPreparation::HeapBytes() const noexcept {
  size_t bytes = Fields_.capacity() * sizeof(decltype(Fields_)::value_type) +
                 Requests_.capacity() * sizeof(FieldRequest);
  if (Metadata_) { bytes += Metadata_->HeapBytes(); }
  for (const auto &entry : Fields_) {
    if (entry.second) { bytes += entry.second->HeapBytes(); }
  }
  return bytes;
}

}
