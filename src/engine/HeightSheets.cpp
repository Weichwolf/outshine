#include "HeightSheets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <ranges>
#include <utility>
#include <string>
#include <tuple>
#include <vector>

#include "ChunkSurface.h"
#include "FlatMap.h"
#include "Geodesy.h"
#include "TerrainGrid.h"
#include "GroundLattice.h"
#include "OsmField.h"
#include "OsmLayer.h"
#include "TileGeodesy.h"
#include "math/Vec3.h"

namespace outshine {

static_assert(kPatchGrid + 1 == Render::GroundLattice::kSide,
              "a page holds the nodes the patchwork's chunk was built from, so the lattice draws "
              "the surface the roads are draped on");

namespace {

constexpr uint8_t kPolygonFeature = 3;

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

struct SourceCoverage {
  int FinestZoom;
  int GroundZoom;
};

struct SourceTile {
  int Zoom;
  long X;
  long Y;
};

void AppendBuildingHeightTiles(std::vector<Data::TileId> &tiles,
                               const Ground::OsmField &vectors,
                               int zoom) {
  const int buildings = vectors.Layer(Ground::OsmLayer::Buildings);
  for (const Ground::OsmField::Feature &feature : vectors.Features()) {
    if (feature.Type != kPolygonFeature || std::cmp_not_equal(feature.Layer, buildings)) {
      continue;
    }
    const Ground::TileSpot low = Ground::HeightField::SpotOf(
        {.LongitudeDeg = feature.MinLon, .LatitudeDeg = feature.MaxLat}, zoom);
    const Ground::TileSpot high = Ground::HeightField::SpotOf(
        {.LongitudeDeg = feature.MaxLon, .LatitudeDeg = feature.MinLat}, zoom);
    for (long y = low.Y; y <= high.Y; ++y) {
      for (long x = low.X; x <= high.X; ++x) {
        tiles.push_back(
            {.Zoom = zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)});
      }
    }
  }
}

[[nodiscard]] std::vector<Data::TileId> SourceTilesOf(const Patchwork &candidate,
                                                      SourceCoverage coverage,
                                                      const Ground::OsmField *vectors) {
  std::vector<Data::TileId> tiles;
  constexpr size_t kNeighbours = 9;
  tiles.reserve(candidate.Sheets.size() * kNeighbours * 2u);
  const auto appendNeighbours = [&tiles](SourceTile source) {
    for (long dy = -1; dy <= 1; ++dy) {
      for (long dx = -1; dx <= 1; ++dx) {
        long nx = source.X + dx;
        const long ny = source.Y + dy;
        if (ny < 0 || !Ground::WrapTile(source.Zoom, &nx, &ny)) { continue; }
        tiles.push_back(
            {.Zoom = source.Zoom, .X = static_cast<uint32_t>(nx), .Y = static_cast<uint32_t>(ny)});
      }
    }
  };
  for (const Sheet &sheet : candidate.Sheets) {
    const int sourceZoom = SourceZoomOf(sheet, coverage.FinestZoom);
    const auto drop = static_cast<uint32_t>(sheet.Tile.Zoom - sourceZoom);
    const long x = static_cast<long>(sheet.Tile.X >> drop);
    const long y = static_cast<long>(sheet.Tile.Y >> drop);
    appendNeighbours({.Zoom = sourceZoom, .X = x, .Y = y});
    if (coverage.GroundZoom >= 0 && coverage.GroundZoom < sourceZoom) {
      const auto parentDrop = static_cast<uint32_t>(sourceZoom - coverage.GroundZoom);
      appendNeighbours({.Zoom = coverage.GroundZoom,
                        .X = static_cast<long>(static_cast<uint32_t>(x) >> parentDrop),
                        .Y = static_cast<long>(static_cast<uint32_t>(y) >> parentDrop)});
    }
  }
  if (vectors != nullptr) {
    for (const Ground::OsmField::Tile &tile : vectors->Tiles()) {
      const int zoom = std::min(tile.Z, coverage.FinestZoom);
      const auto drop = static_cast<uint32_t>(tile.Z - zoom);
      appendNeighbours({.Zoom = zoom,
                        .X = static_cast<long>(static_cast<uint32_t>(tile.X) >> drop),
                        .Y = static_cast<long>(static_cast<uint32_t>(tile.Y) >> drop)});
    }
    AppendBuildingHeightTiles(tiles, *vectors, coverage.FinestZoom);
  }
  const auto key = [](Data::TileId tile) { return std::tuple(tile.Zoom, tile.X, tile.Y); };
  std::ranges::sort(tiles, {}, key);
  tiles.erase(std::ranges::unique(tiles).begin(), tiles.end());
  return tiles;
}

}

const Ground::TerrainField *HeightSheets::FieldAt(Data::TileId tile) const {
  for (const auto &one : Fields_) {
    if (one.first == tile) { return one.second.get(); }
  }
  return nullptr;
}

bool HeightSheets::CopySourcedField(Data::TileId tile, Ground::HeightField::Block &into) const {
  if (tile.Zoom < 0 || tile.Zoom > Ground::HeightField::MaximumTileZoom) { return false; }
  for (int zoom = tile.Zoom; zoom >= 0; --zoom) {
    const auto drop = static_cast<uint32_t>(tile.Zoom - zoom);
    const Data::TileId source{.Zoom = zoom, .X = tile.X >> drop, .Y = tile.Y >> drop};
    const Ground::TerrainField *const field = FieldAt(source);
    if (field == nullptr || !field->Meshable() || field->Sources().empty()) { continue; }
    if (zoom == tile.Zoom) { return Ground::HeightField::CopiesField(*field, tile, into); }
    return Ground::HeightField::ResamplesSourcedAncestor(*field, source, tile, into);
  }
  return false;
}

std::expected<bool, std::string> HeightSheets::PrepareFields(const Patchwork &candidate,
                                                             const Ground::GroundStream &ground,
                                                             FieldPreparation preparation) {
  if (!RequestsPrepared_) {
    ForgetsFields();
    const std::vector<Data::TileId> tiles =
        SourceTilesOf(candidate,
                      {.FinestZoom = preparation.FinestZoom, .GroundZoom = ground.BlockZoom()},
                      preparation.Vectors);
    Requests_.reserve(tiles.size());
    Fields_.reserve(tiles.size());
    for (const Data::TileId tile : tiles) { Requests_.push_back({.Tile = tile}); }
    RequestsPrepared_ = true;
  }
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
  return ResolvedRequests_ == Requests_.size();
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

namespace {

struct SeamEdge {
  long StepX = 0;
  long StepY = 0;
  int FixedFine = 0;
  int FixedCoarse = 0;
  bool AlongJ = false;
};

constexpr std::array<SeamEdge, 4> kSeamEdges = {{
    {.StepX = -1,
     .StepY = 0,
     .FixedFine = 0,
     .FixedCoarse = Render::GroundLattice::kSide - 1,
     .AlongJ = true},
    {.StepX = 1,
     .StepY = 0,
     .FixedFine = Render::GroundLattice::kSide - 1,
     .FixedCoarse = 0,
     .AlongJ = true},
    {.StepX = 0,
     .StepY = -1,
     .FixedFine = 0,
     .FixedCoarse = Render::GroundLattice::kSide - 1,
     .AlongJ = false},
    {.StepX = 0,
     .StepY = 1,
     .FixedFine = Render::GroundLattice::kSide - 1,
     .FixedCoarse = 0,
     .AlongJ = false},
}};

constexpr uint64_t kTileHashMix = 0x9E3779B185EBCA87ULL;

struct SheetHash {
  [[nodiscard]] constexpr uint64_t operator()(const Data::TileId &tile) const noexcept {
    uint64_t hash = static_cast<uint32_t>(tile.Zoom);
    hash = (hash ^ tile.X) * kTileHashMix;
    return (hash ^ tile.Y) * kTileHashMix;
  }
};

using SheetIndex = FlatMap<size_t, Data::TileId, SheetHash>;

struct ZoomRange {
  int Coarsest = std::numeric_limits<int>::max();
  int Finest = std::numeric_limits<int>::min();
};

void StitchAlong(Sheet &fine,
                 const Sheet &coarse,
                 const SeamEdge &edge,
                 HeightSheets::SeamKind *kind) {
  constexpr int side = Render::GroundLattice::kSide;
  const int drop = fine.Tile.Zoom - coarse.Tile.Zoom;
  const uint32_t scale = 1u << static_cast<uint32_t>(drop);
  const uint32_t along = edge.AlongJ ? fine.Tile.Y : fine.Tile.X;
  const double offset = static_cast<double>(along % scale) * static_cast<double>(side - 1);
  const auto coarseAt = [&](int k) {
    return static_cast<double>(
        coarse.Nodes[edge.AlongJ ? PageNode(edge.FixedCoarse, k) : PageNode(k, edge.FixedCoarse)]);
  };
  for (int k = 0; k < side; ++k) {
    const double c = (offset + static_cast<double>(k)) / static_cast<double>(scale);
    const int c0 = std::min(static_cast<int>(c), side - 2);
    const double chord = std::lerp(coarseAt(c0), coarseAt(c0 + 1), c - c0);
    float &height =
        fine.Nodes[edge.AlongJ ? PageNode(edge.FixedFine, k) : PageNode(k, edge.FixedFine)];
    if (kind != nullptr && k % static_cast<int>(scale) != 0) {
      const double differenceM = std::fabs(height - chord);
      if (differenceM > kind->OddBeforeM) {
        kind->OddBeforeM = differenceM;
        const double fraction = static_cast<double>(k) / static_cast<double>(side - 1);
        const double fixed = static_cast<double>(edge.FixedFine) / static_cast<double>(side - 1);
        const Ground::Geo at = Ground::TileFracToGeo(
            {.X = static_cast<double>(fine.Tile.X) + (edge.AlongJ ? fixed : fraction),
             .Y = static_cast<double>(fine.Tile.Y) + (edge.AlongJ ? fraction : fixed)},
            fine.Tile.Zoom);
        kind->WorstLongitudeDeg = at.LongitudeDeg;
        kind->WorstLatitudeDeg = at.LatitudeDeg;
        kind->WorstFineZoom = fine.Tile.Zoom;
        kind->WorstCoarseZoom = coarse.Tile.Zoom;
      }
    }
    height = static_cast<float>(chord);
    if (kind != nullptr) {
      double &after = k % static_cast<int>(scale) == 0 ? kind->EvenM : kind->OddAfterM;
      after = std::max(after, std::fabs(height - chord));
    }
  }
}

}

namespace {

[[nodiscard]] const Sheet *CoarseNeighbor(const Sheet &fine,
                                          const SeamEdge &edge,
                                          const Patchwork &laid,
                                          const SheetIndex &index,
                                          int coarsest) {
  long nx = static_cast<long>(fine.Tile.X) + edge.StepX;
  const long ny = static_cast<long>(fine.Tile.Y) + edge.StepY;
  if (!Ground::WrapTile(fine.Tile.Zoom, &nx, &ny)) { return nullptr; }
  for (int zoom = fine.Tile.Zoom - 1; zoom >= coarsest; --zoom) {
    const auto drop = static_cast<uint32_t>(fine.Tile.Zoom - zoom);
    const Data::TileId wanted{.Zoom = zoom,
                              .X = static_cast<uint32_t>(nx) >> drop,
                              .Y = static_cast<uint32_t>(ny) >> drop};
    if (const size_t *found = index.Find(wanted)) { return &laid.Sheets[*found]; }
  }
  return nullptr;
}

[[nodiscard]] bool
IndexSheets(const Patchwork &laid, SheetIndex &index, ZoomRange &zooms, std::string &error) {
  for (size_t i = 0; i < laid.Sheets.size(); ++i) {
    if (laid.Sheets[i].Nodes.size() != Render::GroundLattice::kPageNodes) { continue; }
    const auto added = index.Emplace(laid.Sheets[i].Tile, i);
    if (!added) {
      error = added.error() == FlatMapError::AllocationFailed
                  ? "ground stitch index allocation failed"
                  : "ground stitch index capacity exceeded";
      return false;
    }
    if (!added->second) {
      error = "ground patchwork repeats tile";
      return false;
    }
    zooms.Coarsest = std::min(zooms.Coarsest, laid.Sheets[i].Tile.Zoom);
    zooms.Finest = std::max(zooms.Finest, laid.Sheets[i].Tile.Zoom);
  }
  return true;
}

void StitchAtZoom(Patchwork &laid,
                  const SheetIndex &index,
                  ZoomRange zooms,
                  int zoom,
                  HeightSheets::Seam &seams) {
  for (size_t sheetIndex = 0; sheetIndex < laid.Sheets.size(); ++sheetIndex) {
    Sheet &fine = laid.Sheets[sheetIndex];
    if (fine.Tile.Zoom != zoom || fine.Nodes.size() != Render::GroundLattice::kPageNodes) {
      continue;
    }
    for (const SeamEdge &edge : kSeamEdges) {
      const Sheet *coarse = CoarseNeighbor(fine, edge, laid, index, zooms.Coarsest);
      if (coarse == nullptr) { continue; }
      HeightSheets::SeamKind *kind = fine.Virtual && coarse->Virtual ? &seams.Virtual : &seams.Real;
      ++kind->Edges;
      StitchAlong(fine, *coarse, edge, kind);
    }
  }
}

}

bool HeightSheets::StitchEdges(Patchwork &laid, std::string &error) {
  Seams_ = {};
  SheetIndex index;
  ZoomRange zooms;
  if (!IndexSheets(laid, index, zooms, error)) { return false; }
  for (int zoom = zooms.Coarsest; zoom <= zooms.Finest; ++zoom) {
    StitchAtZoom(laid, index, zooms, zoom, Seams_);
  }
  return true;
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
  for (int heldZoom = zoom; heldZoom >= 0; --heldZoom) {
    const Ground::TileFrac frac = Ground::ToTileFracClamped(
        {.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg}, heldZoom);
    const Data::TileId tile{.Zoom = heldZoom,
                            .X = static_cast<uint32_t>(std::floor(frac.X)),
                            .Y = static_cast<uint32_t>(std::floor(frac.Y))};
    const Ground::TerrainField *field = FieldAt(tile);
    if (field == nullptr || !field->Meshable()) { continue; }
    return field->PostingM(
        {.Col = frac.X - std::floor(frac.X), .Row = frac.Y - std::floor(frac.Y)});
  }
  return std::nullopt;
}

void HeightSheets::Clear() {
  Residency_.Clear();
  ForgetsFields();
}

uint64_t HeightSheets::Digest() const {
  return Residency_.Digest();
}

size_t HeightSheets::HeapBytes() const noexcept {
  return Residency_.HeapBytes() + Fields_.capacity() * sizeof(decltype(Fields_)::value_type) +
         Requests_.capacity() * sizeof(FieldRequest);
}

}
