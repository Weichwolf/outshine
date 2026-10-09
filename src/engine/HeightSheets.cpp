#include "HeightSheets.h"

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
#include "GroundLattice.h"
#include "TerrainSourceCoverage.h"
#include "TileGeodesy.h"
#include "math/Vec3.h"

namespace outshine {

static_assert(kPatchGrid + 1 == Render::GroundLattice::kSide,
              "a page holds the nodes the patchwork's chunk was built from, so the lattice draws "
              "the surface the roads are draped on");

namespace {

[[nodiscard]] auto TileKey(Data::TileId tile) {
  return std::tuple(tile.Zoom, tile.X, tile.Y);
}

[[nodiscard]] double FractionOf(int k, uint32_t postings, int side) {
  return static_cast<double>(Ground::ChunkNodePosting(k, postings, side)) /
         static_cast<double>(postings - 1u);
}

[[nodiscard]] int SourceZoomOf(const Sheet &sheet, int finestZoom) {
  if (sheet.SourceZoom >= 0) { return sheet.SourceZoom; }
  return sheet.Virtual ? finestZoom : sheet.Tile.Zoom;
}

[[nodiscard]] double NodeFraction(const Sheet &sheet, int k) {
  constexpr int side = Render::GroundLattice::kSide;
  if (sheet.Virtual || sheet.SourceZoom >= 0) {
    return static_cast<double>(k) / static_cast<double>(side - 1);
  }
  if (k < 0) { return -FractionOf(1, sheet.Postings, side); }
  if (k >= side) { return 2.0 - FractionOf(side - 2, sheet.Postings, side); }
  return FractionOf(k, sheet.Postings, side);
}

[[nodiscard]] size_t PageNode(int i, int j) {
  constexpr int pageSide = Render::GroundLattice::kPageSide;
  return static_cast<size_t>(j + 1) * static_cast<size_t>(pageSide) + static_cast<size_t>(i + 1);
}

void CopiesEdgeIntoRim(std::vector<float> &page, const std::vector<bool> &missing) {
  constexpr int side = Render::GroundLattice::kSide;
  for (int j = -1; j <= side; ++j) {
    for (int i = -1; i <= side; ++i) {
      if (!missing[PageNode(i, j)]) { continue; }
      page[PageNode(i, j)] = page[PageNode(std::clamp(i, 0, side - 1), std::clamp(j, 0, side - 1))];
    }
  }
}

[[nodiscard]] std::expected<std::vector<Data::TileId>, std::string> SourceTilesFor(
    const Patchwork &candidate, HeightSheets::FieldPreparation preparation, int groundZoom) {
  constexpr size_t kMaximumAdditionalTiles = 256;
  if (preparation.AdditionalTiles.size() > kMaximumAdditionalTiles) {
    return std::unexpected("terrain field preparation exceeds its 256 additional tile budget");
  }
  if (preparation.FinestZoom < 0 || preparation.FinestZoom > Ground::HeightField::MaximumTileZoom) {
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
        sheet.Tile.Zoom > Ground::HeightField::MaximumTileZoom) {
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

const Ground::TerrainField *HeightSheets::FieldAt(Data::TileId tile) const {
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

bool HeightSheets::ShareSourcedField(Data::TileId tile, Ground::HeightField::Block &into) const {
  return SourcedTerrainFields::Share(Fields_, tile, into);
}

std::expected<SourcedTerrainFields, SourcedTerrainFields::CaptureError>
HeightSheets::CaptureSourcedFields(std::span<const Ground::TileSpot> requests,
                                   size_t bytesMost) const {
  return SourcedTerrainFields::Capture(Fields_, requests, bytesMost, Metadata_);
}

SourcedTerrainFields HeightSheets::SnapshotSourcedFields() const {
  return SourcedTerrainFields(Fields_, Metadata_);
}

std::expected<bool, std::string> HeightSheets::PrepareFields(const Patchwork &candidate,
                                                             const Ground::GroundStream &ground,
                                                             FieldPreparation preparation) {
  if (!RequestsPrepared_) {
    auto tiles = SourceTilesFor(candidate, preparation, ground.BlockZoom());
    if (!tiles) { return std::unexpected(tiles.error()); }
    auto metadata = ground.PrepareSourceMetadata(*tiles);
    if (!metadata) {
      return std::unexpected("terrain source metadata exceeds its resident dependency budget");
    }
    ForgetsFields();
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
    std::shared_ptr<const Ground::TerrainField> field;
    const Ground::TilePool::Reply status = ground.PollStitchedField(request.Tile, field);
    switch (status) {
      case Ground::TilePool::Reply::Ready:
        if (!field) { return std::unexpected("a ready height field has no data"); }
        Fields_.emplace_back(request.Tile, std::move(field));
        break;
      case Ground::TilePool::Reply::Absent:
      case Ground::TilePool::Reply::Undeclared: Fields_.emplace_back(request.Tile, nullptr); break;
      case Ground::TilePool::Reply::Refused:
        return std::unexpected("a terrain source refused a height field");
      case Ground::TilePool::Reply::Pending:
      case Ground::TilePool::Reply::Deferred: continue;
    }
    request.Resolved = true;
    ++ResolvedRequests_;
  }
  if (ResolvedRequests_ != Requests_.size()) { return false; }
  std::ranges::sort(Fields_, {}, [](const auto &one) { return TileKey(one.first); });
  return true;
}

std::string HeightSheets::FieldDiagnostic() const {
  std::string result = std::to_string(ResolvedRequests_) + "/" + std::to_string(Requests_.size());
  const auto pending = std::ranges::find(Requests_, false, &FieldRequest::Resolved);
  if (pending != Requests_.end()) {
    const auto tile = pending->Tile;
    result += " next=" + std::to_string(tile.Zoom) + "/" + std::to_string(tile.X) + "/" +
              std::to_string(tile.Y);
  }
  return result;
}

std::optional<float> HeightSheets::AslAt(int zoom, Ground::TileFrac at) const {
  long x = static_cast<long>(std::floor(at.X));
  const long y = static_cast<long>(std::floor(at.Y));
  const double col = at.X - static_cast<double>(x);
  const double row = at.Y - static_cast<double>(y);
  if (!Ground::WrapTile(zoom, &x, &y)) { return std::nullopt; }
  const Ground::TerrainField *field =
      FieldAt({.Zoom = zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)});
  if (field == nullptr || !field->Meshable()) { return std::nullopt; }
  return field->PostingM({.Col = col, .Row = row});
}

HeightSheets::HaloBuildJob::HaloBuildJob(HeightSheets &sheets,
                                         Patchwork &candidate,
                                         int finestZoom) noexcept
    : Sheets_(&sheets), Candidate_(&candidate), FinestZoom_(finestZoom) {
  Sheets_->RimsMissing_ = 0;
}

bool HeightSheets::HaloBuildJob::BeginsSheet() {
  constexpr int side = Render::GroundLattice::kSide;
  while (SheetAt_ < Candidate_->Sheets.size()) {
    const Sheet &sheet = Candidate_->Sheets[SheetAt_];
    if (sheet.Side != side) {
      ++SheetAt_;
      continue;
    }
    Whole_ = (sheet.Virtual || sheet.SourceZoom >= 0) &&
             sheet.Nodes.size() != Render::GroundLattice::kNodes;
    if (!Whole_ && sheet.Nodes.size() != Render::GroundLattice::kNodes) {
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
    Page_.assign(Render::GroundLattice::kPageNodes, 0.0f);
    Missing_.assign(Render::GroundLattice::kPageNodes, false);
    AnyMissing_ = false;
    NodeAt_ = 0;
    Working_ = true;
    return true;
  }
  return false;
}

bool HeightSheets::HaloBuildJob::AdvancesNode() {
  constexpr int side = Render::GroundLattice::kSide;
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
  const std::optional<float> asl = Sheets_->AslAt(
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

void HeightSheets::HaloBuildJob::CompletesSheet() {
  if (AnyMissing_) {
    CopiesEdgeIntoRim(Page_, Missing_);
    ++Sheets_->RimsMissing_;
  }
  Candidate_->Sheets[SheetAt_].Nodes = std::move(Page_);
  ++Haloed_;
  ++SheetAt_;
  Working_ = false;
}

bool HeightSheets::HaloBuildJob::Advance(size_t nodesMost) {
  while (nodesMost > 0) {
    if (!Working_ && !BeginsSheet()) { return true; }
    --nodesMost;
    if (!AdvancesNode()) { continue; }
    if (NodeAt_ == Render::GroundLattice::kPageNodes) { CompletesSheet(); }
  }
  return !Working_ && SheetAt_ == Candidate_->Sheets.size();
}

size_t HeightSheets::Halos(Patchwork &laid, int finestZoom) {
  HaloBuildJob job(*this, laid, finestZoom);
  while (!job.Advance(std::numeric_limits<size_t>::max())) {}
  return job.Haloed();
}

bool HeightSheets::Stitch(Patchwork &laid, std::string &error) {
  if (!Framed_) { return true; }
  return StitchEdges(laid, error);
}

bool HeightSheets::Hands(Patchwork &laid, std::string &error) {
  return Stitch(laid, error) && (!Framed_ || Residency_.Publish(laid, Frame_, error));
}

bool HeightSheets::BeginResidency(const Patchwork &laid, std::string &error) {
  return !Framed_ || Residency_.BeginPublish(laid, error);
}

std::expected<bool, std::string> HeightSheets::AdvanceResidency(const Patchwork &laid,
                                                                size_t sheetsMost) {
  if (!Framed_) { return true; }
  return Residency_.AdvancePublish(laid, Frame_, sheetsMost);
}

std::optional<double> HeightSheets::FieldUpM(int zoom, EastNorth at) const {
  if (!Framed_) { return std::nullopt; }
  const Vec3 &origin = Frame_.OriginEcef();
  const Vec3 &east = Frame_.EastEcef();
  const Vec3 &north = Frame_.NorthEcef();
  Vec3 ecef;
  for (int axis = 0; axis < 3; ++axis) {
    ecef[axis] = origin[axis] + at.EastM * east[axis] + at.NorthM * north[axis];
  }
  const Ground::Geo geo = Ground::EcefToGeoWgs84({.X = ecef[0], .Y = ecef[1], .Z = ecef[2]});
  const std::optional<double> aslM =
      AslMAt(zoom, {.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg});
  if (!aslM) { return std::nullopt; }
  return Frame_
      .ToLocalPosition(
          {.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg, .HeightM = *aslM})
      .UpM;
}

std::optional<double> HeightSheets::AslMAt(int zoom, LongitudeLatitude at) const {
  if (!Framed_) { return std::nullopt; }
  return SourcedTerrainFields::AslMAt(Fields_, zoom, at);
}

void HeightSheets::Clear() {
  Residency_.Clear();
  ForgetsFields();
}

uint64_t HeightSheets::Digest() const {
  return Residency_.Digest();
}

size_t HeightSheets::HeapBytes() const noexcept {
  size_t bytes = Residency_.HeapBytes() +
                 Fields_.capacity() * sizeof(decltype(Fields_)::value_type) +
                 Requests_.capacity() * sizeof(FieldRequest);
  if (Metadata_) { bytes += Metadata_->HeapBytes(); }
  for (const auto &entry : Fields_) {
    if (entry.second) { bytes += entry.second->HeapBytes(); }
  }
  return bytes;
}

}
