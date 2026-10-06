#include <utility>
#include <expected>
#include "StructureBake.h"
#include "PreparedStructureTile.h"
#include "PreparedStructurePlan.h"
#include "StructurePreparation.h"
#include "BuildingMaterials.h"
#include <bit>
#include "Digest.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "math/Units.h"
#include "StructureMassing.h"
#include "StructurePlanSelection.h"
#include "Geodesy.h"
#include <scene/ProjectedErrorBudget.h>

namespace outshine::Generators {

namespace {

#include "FacadeOpeningValues.h"

constexpr double kBlocksPerTile = 8.0;
constexpr double kNearestSeenM = 1.45;
constexpr double kArchitectureM = 0.33;
constexpr double kRoofAllowanceM = 3.2;

[[nodiscard]] bool WasStopped(const std::atomic_bool *stopping) {
  return stopping != nullptr && stopping->load(std::memory_order_relaxed);
}

struct Ring {
  uint32_t First = 0, Count = 0;
};

struct Spread {
  double LowLat = 0.0, HighLat = 0.0, LowLon = 0.0, HighLon = 0.0;
};

struct Standing {
  double BaseM = 0.0, SeatM = 0.0, HeightM = 0.0;
  double RoofAreaM2 = 0.0;
  bool Pitched = false;
  std::optional<Vec3f> WallColour;
  LevelOfDetail Level = LevelOfDetail::Fine;
};

using MassPlans = std::vector<StructureMassPlan>;

StructureMassPlan StructureMassOf(Spread over, Standing at, uint32_t cell) {
  StructureMassPlan plan{.LowLat = over.LowLat,
                         .HighLat = over.HighLat,
                         .LowLon = over.LowLon,
                         .HighLon = over.HighLon,
                         .BaseSum = at.BaseM,
                         .SeatSum = at.SeatM,
                         .HeightSum = at.HeightM,
                         .MinimumBaseM = at.BaseM,
                         .MaximumTopM = at.BaseM + at.HeightM + kRoofAllowanceM,
                         .Count = 1,
                         .Cell = cell,
                         .PitchedAreaM2 = at.Pitched ? at.RoofAreaM2 : 0.0,
                         .RoofAreaM2 = at.RoofAreaM2,
                         .HasWallColour = at.WallColour.has_value(),
                         .Level = at.Level};
  const Vec3f colour = at.WallColour.value_or(kBuildingWallColour);
  for (size_t channel = 0; channel < 3; ++channel) {
    plan.WallColourSum[channel] = static_cast<double>(colour[channel]) * at.RoofAreaM2;
  }
  return plan;
}

std::expected<void, StructureMeshError> RaiseStructureMass(const StructureMassPlan &of,
                                                           const RawTile &raw,
                                                           const StructureMesher &mesher,
                                                           MeshScratch &scratch,
                                                           Raised &into) {
  if (of.Count == 0) { return {}; }
  std::array<double, 8> ring{};
  std::array<double, 4> cornerHeights{};
  const auto plan = DescribeStructureMass(of, raw, ring, cornerHeights);
  return mesher.Mesh(plan, scratch, into);
}

[[nodiscard]] uint64_t DigestOver(const Raised &built) {
  uint64_t mixed = kDigestBasis;
  const auto fold = [&mixed](uint64_t one) { mixed = (mixed ^ one) * kDigestPrime; };
  const auto foldVertex = [&fold](const StoredVertex &one) {
    const auto *const held = reinterpret_cast<const float *>(&one);
    for (size_t at = 0; at < kStoredVertexFloats; ++at) { fold(std::bit_cast<uint32_t>(held[at])); }
  };
  for (const StoredVertex &one : built.WallCorners) { foldVertex(one); }
  for (const StoredVertex &one : built.RoofCorners) { foldVertex(one); }
  for (const uint32_t one : built.WallRun) { fold(one); }
  for (const uint32_t one : built.RoofRun) { fold(one); }
  for (const float one : built.WallColours) { fold(std::bit_cast<uint32_t>(one)); }
  return mixed;
}

[[nodiscard]] std::expected<ClusteredMesh, ClusterError> CookedOver(
    const RawTile &raw, std::span<const StoredVertex> corners, std::span<const uint32_t> run) {
  if (raw.ClusterTriangles == 0) { return ClusteredMesh{}; }
  const std::span<const float> positions(reinterpret_cast<const float *>(corners.data()),
                                         corners.size() * kStoredVertexFloats);
  return CookClusters(
      {.PositionsM = positions, .Indices = run, .StrideFloats = kStoredVertexFloats},
      raw.ClusterTriangles);
}

std::expected<void, ClusterError> FinalizeBake(const RawTile &raw, BakedTile &out) {
  auto walls = CookedOver(raw, out.Built.WallCorners, out.Built.WallRun);
  if (!walls) { return std::unexpected(walls.error()); }
  auto roofs = CookedOver(raw, out.Built.RoofCorners, out.Built.RoofRun);
  if (!roofs) { return std::unexpected(roofs.error()); }
  out.Walls = std::move(*walls);
  out.Roofs = std::move(*roofs);
  out.Digest = DigestOver(out.Built);
  return {};
}

std::expected<void, StructureMeshError> AccountMesh(std::expected<void, StructureMeshError> result,
                                                    BakedTile &out) {
  if (result) { return {}; }
  if (result.error() == StructureMeshError::UnsupportedFootprint) {
    ++out.UnsupportedMeshes;
    return {};
  }
  return std::unexpected(result.error());
}

std::expected<void, StructureBakeError> FinishStructures(MassPlans &masses,
                                                         const RawTile &raw,
                                                         const StructureMesher &mesher,
                                                         MeshScratch &scratch,
                                                         BakedTile &out,
                                                         const std::atomic_bool *stopping) {
  auto grouped = GroupStructureMasses(masses, raw, stopping);
  if (!grouped) { return std::unexpected(grouped.error()); }
  for (const StructureMassPlan &block : *grouped) {
    if (WasStopped(stopping)) { return std::unexpected(StructureBakeErrorKind::Cancelled); }
    const auto built = AccountMesh(RaiseStructureMass(block, raw, mesher, scratch, out.Built), out);
    if (!built) { return std::unexpected(built.error()); }
  }
  out.Blocks = static_cast<int>(grouped->size());
  return FinalizeBake(raw, out);
}

void IncludeBounds(outshine::Ground::GeoBounds &bounds,
                   const outshine::Ground::GeoBounds &additional) {
  bounds.MinLatDeg = std::min(bounds.MinLatDeg, additional.MinLatDeg);
  bounds.MaxLatDeg = std::max(bounds.MaxLatDeg, additional.MaxLatDeg);
  bounds.MinLonDeg = std::min(bounds.MinLonDeg, additional.MinLonDeg);
  bounds.MaxLonDeg = std::max(bounds.MaxLonDeg, additional.MaxLonDeg);
}

void IncludeFootprint(BakedTile &out, const StructureCell &cell) {
  const size_t cellAt = cell.Index - 1u;
  const uint64_t cellBit = uint64_t{1} << cellAt;
  if ((out.OccupiedCells & cellBit) == 0) {
    out.CellBounds[cellAt] = cell.Footprint;
  } else {
    IncludeBounds(out.CellBounds[cellAt], cell.Footprint);
  }
  out.OccupiedCells |= cellBit;
  if (out.FootprintBounds) {
    IncludeBounds(*out.FootprintBounds, cell.Footprint);
  } else {
    out.FootprintBounds = cell.Footprint;
  }
}

struct BuildingDetail {
  LevelOfDetail Envelope = LevelOfDetail::Fine;
  bool RecessedOpenings = true;
};

BuildingDetail DetailOf(const RawTile &raw, const Spread &bounds, double statedM) {
  BuildingDetail detail{.Envelope = raw.RequestedDetail.value_or(LevelOfDetail::Fine)};
  if (!raw.RequestedDetail || raw.RequestedCell) {
    const double nearLat = std::clamp(raw.Eye.LatitudeDeg, bounds.LowLat, bounds.HighLat);
    const double midLon = 0.5 * (bounds.LowLon + bounds.HighLon);
    const double localEyeLon = midLon + std::remainder(raw.Eye.LongitudeDeg - midLon, kDegPerTurn);
    const double nearLon = std::clamp(localEyeLon, bounds.LowLon, bounds.HighLon);
    const double northM = (nearLat - raw.Eye.LatitudeDeg) * kMPerDegLat;
    const double eastM =
        (nearLon - localEyeLon) * kMPerDegLon * std::cos(raw.Eye.LatitudeDeg * kDeg2Rad);
    const double awayAtLeastM = std::max(std::sqrt(northM * northM + eastM * eastM), kNearestSeenM);
    const double conservativeAwayM =
        std::max(awayAtLeastM - kStructureEyeDetailGuardM, kNearestSeenM);
    const auto &projection = raw.Projection;
    detail.RecessedOpenings = !projection.Allows(kOpeningDepthM, conservativeAwayM);
    if (!raw.RequestedDetail &&
        projection.Allows(std::max(kArchitectureM, statedM), conservativeAwayM)) {
      detail.Envelope = LevelOfDetail::Shell;
    }
    if (!raw.RequestedDetail && detail.Envelope == LevelOfDetail::Shell &&
        projection.Allows(0.5 * raw.TileSpanM / kBlocksPerTile, conservativeAwayM)) {
      detail.Envelope = LevelOfDetail::Massed;
    }
  }
  return detail;
}

void IncludeShellError(BakedTile &out, size_t cellAt, std::optional<double> error) {
  double &cellError = out.CellShellErrorM[cellAt];
  if (!error || !std::isfinite(*error) || !(*error > 0.0)) {
    cellError = -1.0;
  } else if (cellError >= 0.0) {
    cellError = std::max(cellError, *error);
  }
}

std::expected<void, StructureBakeError> EmitStructure(const PreparedStructure &prepared,
                                                      std::span<const double> pts,
                                                      std::span<const GeographicRing> holes,
                                                      std::span<const double> corners,
                                                      const RawTile &raw,
                                                      const StructureMesher &mesher,
                                                      MeshScratch &scratch,
                                                      BakedTile &out,
                                                      MassPlans &masses,
                                                      StructurePlanSelection &selection,
                                                      const BuildingSurface *surface = nullptr) {
  const auto &one = prepared.Layout;
  if (raw.RequestedCell && one.Cell.Index != *raw.RequestedCell) { return {}; }
  const auto &fp = prepared.Standing;
  const double base = prepared.BaseAslM;
  const double seat = prepared.SeatAslM;
  const Spread bounds{.LowLat = prepared.Bounds.MinLatDeg,
                      .HighLat = prepared.Bounds.MaxLatDeg,
                      .LowLon = prepared.Bounds.MinLonDeg,
                      .HighLon = prepared.Bounds.MaxLonDeg};
  IncludeFootprint(out, one.Cell);
  out.SeatSpreadM.push_back(seat - base);
  out.AcrossM.push_back(prepared.AcrossM);
  out.Fronted += static_cast<int>(fp.Street.Known);
  const double statedM =
      raw.TileSpanM > 0.0 && raw.Extent > 0 ? raw.TileSpanM / static_cast<double>(raw.Extent) : 0.0;
  out.OsmHeights +=
      static_cast<int>(fp.Source == ::outshine::Ground::BuildingHeightSource::Declared);
  out.DefaultHeights +=
      static_cast<int>(fp.Source == ::outshine::Ground::BuildingHeightSource::Generated);
  const size_t cellAt = one.Cell.Index - 1u;
  out.CellMaxHeightM[cellAt] = std::max(out.CellMaxHeightM[cellAt], fp.HeightM);

  const auto detail = DetailOf(raw, bounds, statedM);
  const auto level = detail.Envelope;
  out.Prints.push_back(fp);
  out.FootprintDetails.push_back(level);

  auto plan = PreparedStructurePlan(prepared, pts, holes, corners, raw.AnchorEcef);
  plan.Prepared = surface;
  plan.Coarseness = level;
  plan.RecessedOpenings = detail.RecessedOpenings;

  const auto mass = StructureMassOf(bounds,
                                    {.BaseM = base,
                                     .SeatM = seat,
                                     .HeightM = fp.HeightM,
                                     .RoofAreaM2 = prepared.AreaM2,
                                     .Pitched = one.Pitched != 0,
                                     .WallColour = one.WallColour,
                                     .Level = level},
                                    one.Cell.Index);
  if (!raw.RequestedDetail) {
    IncludeShellError(
        out, cellAt, selection.Add(plan, mass, out.Prints.size() - 1u, mesher, scratch));
    return {};
  }
  if (level >= LevelOfDetail::Massed && one.MinimumHeightM == 0.0 && one.HoleCount == 0) {
    IncludeShellError(out, cellAt, mesher.ShellSurfaceErrorM(plan, scratch));
    masses.push_back(mass);
    ++out.Lumped;
    return {};
  }

  const size_t wallFirst = out.Built.WallCorners.size();
  const auto built = AccountMesh(mesher.Mesh(plan, scratch, out.Built), out);
  if (!built) { return std::unexpected(built.error()); }
  IncludeShellError(
      out,
      cellAt,
      level <= LevelOfDetail::Shell
          ? mesher.ShellSurfaceErrorM(std::span(out.Built.WallCorners).subspan(wallFirst))
          : mesher.ShellSurfaceErrorM(plan, scratch));
  return {};
}

std::expected<void, StructureBakeError> BakeOne(const RawTile &raw,
                                                const Ground::HeightField &heights,
                                                const StructureMesher &mesher,
                                                MeshScratch &scratch,
                                                BakedTile &out,
                                                const RawTile::Structure &one,
                                                std::span<const double> pts,
                                                const std::vector<WayLine> &ways,
                                                MassPlans &masses,
                                                StructurePlanSelection &selection,
                                                std::vector<double> &corners,
                                                const std::atomic_bool *stopping) {
  if (one.Cell.Index == 0 || one.Cell.Index > kStructureCellsPerTile) {
    return std::unexpected(StructureBakeErrorKind::InvalidCell);
  }
  if (raw.RequestedCell && one.Cell.Index != *raw.RequestedCell) { return {}; }
  auto prepared = EnrichStructure(raw, heights, one, pts, ways, corners, out, stopping);
  if (!prepared) { return std::unexpected(prepared.error()); }
  if (!*prepared) { return {}; }
  return EmitStructure(
      **prepared, pts, raw.Holes, corners, raw, mesher, scratch, out, masses, selection);
}

std::expected<void, StructureBakeError> ValidateStructureView(const RawTile &raw) {
  if (raw.RequestedDetail && *raw.RequestedDetail > LevelOfDetail::Massed) {
    return std::unexpected(StructureBakeErrorKind::InvalidDetail);
  }
  if (raw.RequestedCell &&
      (*raw.RequestedCell == 0 || *raw.RequestedCell > kStructureCellsPerTile)) {
    return std::unexpected(StructureBakeErrorKind::InvalidCell);
  }
  if (raw.RequestedCell && !raw.RequestedDetail) {
    return std::unexpected(StructureBakeErrorKind::InvalidDetail);
  }
  return {};
}

}

struct StructureBakeProgress::State {
  enum class Phase { Planning, Emission, Complete };
  Phase Current = Phase::Planning;
  StructurePlanSelection Selection;
  std::vector<WayLine> Ways;
  MassPlans Masses;
  std::vector<double> Corners;
  BakedTile Tile;
  size_t Next = 0;
  size_t Count = 0;
  bool Started = false;
  bool Finalized = false;

  [[nodiscard]] std::expected<void, StructureBakeError> Validate(const RawTile &raw) const {
    const auto valid = ValidateStructureView(raw);
    if (!valid) { return valid; }
    if (Started && Tile.RequestedDetail != raw.RequestedDetail) {
      return std::unexpected(StructureBakeErrorKind::ChangedDetail);
    }
    if (Started && Tile.RequestedCell != raw.RequestedCell) {
      return std::unexpected(StructureBakeErrorKind::ChangedCell);
    }
    return {};
  }

  void Start(const RawTile &raw, bool fallback, size_t count) {
    Count = count;
    BakedTile &out = Tile;
    out.Walls = {};
    out.Roofs = {};
    out.Built.Clear();
    out.Prints.clear();
    out.FootprintDetails.clear();
    out.SeatSpreadM.clear();
    out.AcrossM.clear();
    out.OsmHeights = 0;
    out.DefaultHeights = 0;
    out.Fronted = 0;
    out.Lumped = 0;
    out.Blocks = 0;
    out.NoGround = 0;
    out.UnsupportedMeshes = 0;
    out.SkippedRings = 0;
    out.FallbackHeights = fallback;
    const StructureSelectionView selectedView{
        .Eye = raw.Eye, .EyeEcef = raw.EyeEcef, .Projection = raw.Projection};
    out.SelectedView =
        !raw.RequestedDetail && selectedView.Contains(raw.Eye, raw.Projection, raw.EyeEcef)
            ? std::optional(selectedView)
            : std::nullopt;
    out.RequestedDetail = raw.RequestedDetail;
    out.RequestedCell = raw.RequestedCell;
    out.FootprintBounds.reset();
    out.OccupiedCells = 0;
    out.CellBounds = {};
    out.CellMaxHeightM.fill(0.0f);
    out.CellShellErrorM.fill(0.0);
    Ways = StructureWays(raw);
    Masses.clear();
    Corners.clear();
    Started = true;
  }
};

StructureBakeProgress::StructureBakeProgress() : State_(std::make_unique<State>()) {}

StructureBakeProgress::~StructureBakeProgress() = default;

size_t StructureBakeProgress::BakedStructures() const noexcept {
  return State_->Next;
}

std::expected<bool, StructureBakeError>
StructureBakeProgress::AdvanceStructures(const RawTile &raw,
                                         const outshine::Ground::HeightField &heights,
                                         const StructureMesher &mesher,
                                         MeshScratch &scratch,
                                         size_t structuresMost,
                                         const std::atomic_bool *stopping) {
  const auto valid = State_->Validate(raw);
  if (!valid) { return std::unexpected(valid.error()); }
  if (structuresMost == 0) { return false; }
  State &state = *State_;
  assert(!state.Finalized);
  BakedTile &out = state.Tile;
  if (!state.Started) { state.Start(raw, heights.Fallback(), raw.Structures.size()); }
  if (state.Current == State::Phase::Complete) { return true; }
  if (state.Current == State::Phase::Emission) {
    const auto emitted = state.Selection.Emit(raw, mesher, scratch, out, structuresMost, stopping);
    if (!emitted) { return std::unexpected(emitted.error()); }
    if (*emitted) { state.Current = State::Phase::Complete; }
    return *emitted;
  }
  const std::span<const double> pts = raw.LatLon;
  const size_t until = std::min(state.Next + structuresMost, raw.Structures.size());
  for (; state.Next < until; ++state.Next) {
    if (WasStopped(stopping)) { return std::unexpected(StructureBakeErrorKind::Cancelled); }
    const auto baked = BakeOne(raw,
                               heights,
                               mesher,
                               scratch,
                               out,
                               raw.Structures[state.Next],
                               pts,
                               state.Ways,
                               state.Masses,
                               state.Selection,
                               state.Corners,
                               stopping);
    if (!baked) { return std::unexpected(baked.error()); }
  }
  if (state.Next != raw.Structures.size()) { return false; }
  if (raw.RequestedDetail) {
    state.Current = State::Phase::Complete;
    return true;
  }
  const auto selected = state.Selection.Select(raw, mesher, scratch, out, stopping);
  if (!selected) { return std::unexpected(selected.error()); }
  state.Current = State::Phase::Emission;
  return false;
}

std::expected<BakedTile, StructureBakeError>
StructureBakeProgress::Finalize(const RawTile &raw,
                                const StructureMesher &mesher,
                                MeshScratch &scratch,
                                const std::atomic_bool *stopping) {
  assert(State_->Started && State_->Next == State_->Count);
  assert(!State_->Finalized);
  assert(State_->Current == State::Phase::Complete);
  if (WasStopped(stopping)) { return std::unexpected(StructureBakeErrorKind::Cancelled); }
  const std::expected<void, StructureBakeError> finished =
      raw.RequestedDetail
          ? FinishStructures(State_->Masses, raw, mesher, scratch, State_->Tile, stopping)
          : FinalizeBake(raw, State_->Tile);
  if (!finished) { return std::unexpected(finished.error()); }
  State_->Finalized = true;
  return std::move(State_->Tile);
}

std::expected<void, StructureBakeError> BakeStructures(const RawTile &raw,
                                                       const outshine::Ground::HeightField &heights,
                                                       const StructureMesher &mesher,
                                                       MeshScratch &scratch,
                                                       BakedTile &out,
                                                       const std::atomic_bool *stopping) {
  StructureBakeProgress progress;
  const size_t batchSize = std::max(raw.Structures.size(), size_t{1});
  for (;;) {
    const auto completed =
        progress.AdvanceStructures(raw, heights, mesher, scratch, batchSize, stopping);
    if (!completed) { return std::unexpected(completed.error()); }
    if (*completed) { break; }
  }
  auto finalized = progress.Finalize(raw, mesher, scratch, stopping);
  if (!finalized) { return std::unexpected(finalized.error()); }
  out = std::move(*finalized);
  return {};
}

std::expected<bool, StructureBakeError>
StructureBakeProgress::AdvancePrepared(const PreparedStructureTile &base,
                                       const RawTile &view,
                                       const StructureMesher &mesher,
                                       MeshScratch &scratch,
                                       size_t structuresMost,
                                       const std::atomic_bool *stopping) {
  if (WasStopped(stopping)) { return std::unexpected(StructureBakeErrorKind::Cancelled); }
  if (base.Surfaces.size() != base.Structures.size()) {
    return std::unexpected(StructureMeshError::InvalidPlan);
  }
  const auto valid = State_->Validate(view);
  if (!valid) { return std::unexpected(valid.error()); }
  if (structuresMost == 0) { return false; }
  State &state = *State_;
  assert(!state.Finalized);
  BakedTile &out = state.Tile;
  if (!state.Started) {
    state.Start(view, base.FallbackHeights, base.Structures.size());
    out.SkippedRings = base.SkippedRings;
    out.NoGround = base.NoGround;
    out.Coordinates = std::make_shared<Ground::BuildingGeometry>();
    out.Coordinates->Origin = base.Origin;
    out.Coordinates->Points = base.PointsLatLon;
    out.Coordinates->Rings = base.Holes;
  }
  if (state.Current == State::Phase::Complete) { return true; }
  if (state.Current == State::Phase::Emission) {
    const auto emitted = state.Selection.Emit(view, mesher, scratch, out, structuresMost, stopping);
    if (!emitted) { return std::unexpected(emitted.error()); }
    if (*emitted) { state.Current = State::Phase::Complete; }
    return *emitted;
  }
  const size_t until = std::min(state.Next + structuresMost, base.Structures.size());
  for (; state.Next < until; ++state.Next) {
    const auto &one = base.Structures[state.Next];
    if (WasStopped(stopping)) { return std::unexpected(StructureBakeErrorKind::Cancelled); }
    if (view.RequestedCell && one.Layout.Cell.Index != *view.RequestedCell) { continue; }
    const auto emitted =
        EmitStructure(one,
                      base.PointsLatLon,
                      base.Holes,
                      std::span(base.CornerAslM).subspan(one.CornerFirst, one.Layout.PointCount),
                      view,
                      mesher,
                      scratch,
                      out,
                      state.Masses,
                      state.Selection,
                      &base.Surfaces[state.Next]);
    if (!emitted) { return std::unexpected(emitted.error()); }
    out.Prints.back().FirstPoint = one.Layout.LocalFirst;
    out.Prints.back().FirstHole = one.Layout.FirstHole;
    if (base.Origin.Provenance) { out.Coordinates->Sources.push_back(one.Layout.SourceId); }
  }
  if (state.Next != base.Structures.size()) { return false; }
  if (view.RequestedDetail) {
    state.Current = State::Phase::Complete;
    return true;
  }
  const auto selected = state.Selection.Select(view, mesher, scratch, out, stopping);
  if (!selected) { return std::unexpected(selected.error()); }
  state.Current = State::Phase::Emission;
  return false;
}

std::expected<BakedTile, StructureBakeError>
BakePreparedStructures(const PreparedStructureTile &base,
                       const RawTile &view,
                       const StructureMesher &mesher,
                       MeshScratch &scratch,
                       const std::atomic_bool *stopping) {
  RawTile raw;
  raw.Eye = view.Eye;
  raw.EyeEcef = view.EyeEcef;
  raw.RequestedDetail = view.RequestedDetail;
  raw.RequestedCell = view.RequestedCell;
  raw.Projection = view.Projection;
  raw.ClusterTriangles = view.ClusterTriangles;
  raw.AnchorEcef = base.AnchorEcef;
  raw.TileSpanM = base.TileSpanM;
  raw.Extent = base.Extent;
  StructureBakeProgress progress;
  for (;;) {
    const auto advanced = progress.AdvancePrepared(base, raw, mesher, scratch, 64, stopping);
    if (!advanced) { return std::unexpected(advanced.error()); }
    if (*advanced) { break; }
  }
  return progress.Finalize(raw, mesher, scratch, stopping);
}

}
