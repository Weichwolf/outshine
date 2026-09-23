#include "TerrainPress.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ratio>
#include <utility>
#include <vector>

#include "TerrainGrid.h"
#include "TileGeodesy.h"
#include "math/Vec3.h"

namespace outshine::Generators {

struct TerrainPressJob::State {
  enum class Phase : uint8_t { Gather, Decide, Write, Reproject, Measure, Done };

  std::vector<EarthworkStamp> Stamps;
  Patchwork *Candidate;
  TangentFrame Frame;
  TerrainPageLayout Layout;
  double MostEarthworkM;
  std::vector<EastNorth> Positions;
  std::vector<double> HeightsM;
  std::vector<double> PreviousM;
  std::vector<size_t> SourceSheets;
  std::unique_ptr<EarthworkPressJob> PressJob;
  EarthworkPressResult PressResult;
  PressedTerrain Result;
  size_t NextSheet = 0;
  size_t NextPoint = 0;
  Phase Current = Phase::Gather;

  static double Measures(double &total, std::chrono::steady_clock::time_point began) {
    const double elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    total += elapsed;
    return elapsed;
  }

  State(std::vector<EarthworkStamp> yields,
        Patchwork &candidate,
        TangentFrame frame,
        TerrainPageLayout layout,
        double mostEarthworkM)
      : Stamps(std::move(yields)),
        Candidate(&candidate),
        Frame(frame),
        Layout(layout),
        MostEarthworkM(mostEarthworkM) {
    if (Stamps.empty() || !Layout.Valid()) { Current = Phase::Done; }
    if (Current == Phase::Done) { return; }
    const auto validSheets = static_cast<size_t>(std::count_if(
        Candidate->Sheets.begin(), Candidate->Sheets.end(), [this](const Sheet &sheet) {
          return sheet.Side == Layout.Side && (sheet.Virtual || sheet.Postings >= 2) &&
                 sheet.Nodes.size() == Layout.NodeCount();
        }));
    const size_t nodeCount = validSheets * Layout.NodeCount();
    SourceSheets.reserve(validSheets);
    Positions.reserve(nodeCount);
    HeightsM.reserve(nodeCount);
    PreviousM.reserve(nodeCount);
  }

  void GatherSheet(size_t sheetAt) {
    const Sheet &sheet = Candidate->Sheets[sheetAt];
    if (sheet.Side != Layout.Side || (!sheet.Virtual && sheet.Postings < 2) ||
        sheet.Nodes.size() != Layout.NodeCount()) {
      return;
    }
    SourceSheets.push_back(sheetAt);
    for (int row = -Layout.Halo; row < Layout.Side + Layout.Halo; ++row) {
      const double rowFraction = Layout.FractionAt(sheet, row);
      for (int column = -Layout.Halo; column < Layout.Side + Layout.Halo; ++column) {
        const double columnFraction = Layout.FractionAt(sheet, column);
        const Ground::Geo geo =
            Ground::TileFracToGeo({.X = static_cast<double>(sheet.Tile.X) + columnFraction,
                                   .Y = static_cast<double>(sheet.Tile.Y) + rowFraction},
                                  sheet.Tile.Zoom);
        const size_t node = Layout.NodeAt(column, row);
        const EastNorthUp placed = Frame.Place({.LongitudeDeg = geo.LongitudeDeg,
                                                .LatitudeDeg = geo.LatitudeDeg,
                                                .HeightM = static_cast<double>(sheet.Nodes[node])});
        Positions.push_back({.EastM = placed.EastM, .NorthM = placed.NorthM});
        HeightsM.push_back(placed.UpM);
        PreviousM.push_back(placed.UpM);
      }
    }
  }

  [[nodiscard]] std::pair<size_t, size_t> SourceOf(size_t point) const {
    const size_t nodesPerSheet = Layout.NodeCount();
    return {SourceSheets[point / nodesPerSheet], point % nodesPerSheet};
  }

  void WritePoint(size_t point) {
    if (HeightsM[point] == PreviousM[point]) { return; }
    Result.DeepestM = std::max(Result.DeepestM, PreviousM[point] - HeightsM[point]);
    Result.RaisedM = std::max(Result.RaisedM, HeightsM[point] - PreviousM[point]);
    const Vec3 &origin = Frame.OriginEcef();
    const Vec3 &east = Frame.EastEcef();
    const Vec3 &north = Frame.NorthEcef();
    const Vec3 &up = Frame.UpEcef();
    const double e = Positions[point].EastM;
    const double n = Positions[point].NorthM;
    Vec3 ecef;
    for (int axis = 0; axis < 3; ++axis) {
      ecef[axis] = origin[axis] + e * east[axis] + n * north[axis] + HeightsM[point] * up[axis];
    }
    const Ground::Geo geo = Ground::EcefToGeoWgs84({.X = ecef[0], .Y = ecef[1], .Z = ecef[2]});
    const auto [sheetAt, node] = SourceOf(point);
    Candidate->Sheets[sheetAt].Nodes[node] = static_cast<float>(geo.HeightM);
  }

  void ReprojectPoint(size_t point) {
    const auto [sheetAt, node] = SourceOf(point);
    const Sheet &sheet = Candidate->Sheets[sheetAt];
    const size_t pageSide = Layout.PageSide();
    const int column = static_cast<int>(node % pageSide) - Layout.Halo;
    const int row = static_cast<int>(node / pageSide) - Layout.Halo;
    const Ground::Geo geo = Ground::TileFracToGeo(
        {.X = static_cast<double>(sheet.Tile.X) + Layout.FractionAt(sheet, column),
         .Y = static_cast<double>(sheet.Tile.Y) + Layout.FractionAt(sheet, row)},
        sheet.Tile.Zoom);
    HeightsM[point] = Frame
                          .Place({.LongitudeDeg = geo.LongitudeDeg,
                                  .LatitudeDeg = geo.LatitudeDeg,
                                  .HeightM = static_cast<double>(sheet.Nodes[node])})
                          .UpM;
  }

  [[nodiscard]] size_t HeapBytes() const noexcept {
    size_t bytes = Stamps.capacity() * sizeof(EarthworkStamp) +
                   Positions.capacity() * sizeof(EastNorth) +
                   (HeightsM.capacity() + PreviousM.capacity()) * sizeof(double) +
                   SourceSheets.capacity() * sizeof(size_t) +
                   PressResult.Refused.capacity() * sizeof(uint8_t) +
                   PressResult.DecidedBy.capacity() * sizeof(uint32_t) +
                   PressResult.Inside.capacity() * sizeof(EarthworkPointClaim);
    for (const EarthworkStamp &one : Stamps) { bytes += one.HeapBytes(); }
    if (PressJob) { bytes += PressJob->HeapBytes(); }
    return bytes;
  }
};

TerrainPressJob::TerrainPressJob(std::vector<EarthworkStamp> yields,
                                 Patchwork &candidate,
                                 TangentFrame frame,
                                 TerrainPageLayout layout,
                                 double mostEarthworkM)
    : State_(std::make_unique<State>(std::move(yields), candidate, frame, layout, mostEarthworkM)) {
}

TerrainPressJob::~TerrainPressJob() = default;
TerrainPressJob::TerrainPressJob(TerrainPressJob &&) noexcept = default;
TerrainPressJob &TerrainPressJob::operator=(TerrainPressJob &&) noexcept = default;

bool TerrainPressJob::Advance(size_t sheetsMost, size_t pointsMost) {
  State &state = *State_;
  if (state.Current == State::Phase::Done) { return true; }
  const auto began = std::chrono::steady_clock::now();
  switch (state.Current) {
    case State::Phase::Gather: {
      if (sheetsMost == 0) { return false; }
      const size_t end =
          state.NextSheet + std::min(sheetsMost, state.Candidate->Sheets.size() - state.NextSheet);
      for (; state.NextSheet < end; ++state.NextSheet) { state.GatherSheet(state.NextSheet); }
      state.Result.LongestGatherMs =
          std::max(state.Result.LongestGatherMs, State::Measures(state.Result.GatherMs, began));
      if (state.NextSheet < state.Candidate->Sheets.size()) { return false; }
      state.PressJob = std::make_unique<EarthworkPressJob>(
          state.Stamps, state.Positions, state.HeightsM, state.MostEarthworkM);
      state.Current = State::Phase::Decide;
      return false;
    }
    case State::Phase::Decide: {
      if (!state.PressJob->Advance(pointsMost)) {
        state.Result.LongestDecideMs =
            std::max(state.Result.LongestDecideMs, State::Measures(state.Result.DecideMs, began));
        return false;
      }
      state.Result.LongestDecideMs =
          std::max(state.Result.LongestDecideMs, State::Measures(state.Result.DecideMs, began));
      state.PressResult = state.PressJob->Take();
      state.PressJob.reset();
      state.Result.Nodes = state.PressResult.Moved;
      state.Result.Structures = state.PressResult.Structures;
      state.Result.Held = state.PressResult.Held;
      state.Result.BucketMs = state.PressResult.BucketMs;
      state.Result.RejectMs = state.PressResult.RejectMs;
      state.Result.ApplyMs = state.PressResult.ApplyMs;
      state.Result.LongestRejectMs = state.PressResult.LongestRejectMs;
      state.Result.LongestInitializeMs = state.PressResult.LongestInitializeMs;
      state.Result.LongestApplyMs = state.PressResult.LongestApplyMs;
      state.Current = state.PressResult.Moved == 0 ? State::Phase::Done : State::Phase::Write;
      return state.Current == State::Phase::Done;
    }
    case State::Phase::Write: {
      if (pointsMost == 0) { return false; }
      const size_t end =
          state.NextPoint + std::min(pointsMost, state.HeightsM.size() - state.NextPoint);
      for (; state.NextPoint < end; ++state.NextPoint) { state.WritePoint(state.NextPoint); }
      state.Result.LongestWriteMs =
          std::max(state.Result.LongestWriteMs, State::Measures(state.Result.WriteMs, began));
      if (state.NextPoint < state.HeightsM.size()) { return false; }
      state.NextPoint = 0;
      state.Current = State::Phase::Reproject;
      return false;
    }
    case State::Phase::Reproject: {
      if (pointsMost == 0) { return false; }
      const size_t end =
          state.NextPoint + std::min(pointsMost, state.HeightsM.size() - state.NextPoint);
      for (; state.NextPoint < end; ++state.NextPoint) { state.ReprojectPoint(state.NextPoint); }
      state.Result.LongestReprojectMs =
          std::max(state.Result.LongestReprojectMs, State::Measures(state.Result.WriteMs, began));
      if (state.NextPoint < state.HeightsM.size()) { return false; }
      state.Current = State::Phase::Measure;
      return false;
    }
    case State::Phase::Measure: {
      const EarthworkHeightView finalHeights{.WrittenM = state.HeightsM, .WasM = state.PreviousM};
      state.Result.Pads = MeasureEarthworkEffect(
          state.Stamps, state.PressResult, EarthworkKind::Pad, state.Positions, finalHeights);
      state.Result.Corridors = MeasureEarthworkEffect(
          state.Stamps, state.PressResult, EarthworkKind::Corridor, state.Positions, finalHeights);
      state.Result.LongestFloorsMs =
          std::max(state.Result.LongestFloorsMs, State::Measures(state.Result.FloorsMs, began));
      state.Current = State::Phase::Done;
      return true;
    }
    case State::Phase::Done: return true;
  }
  return false;
}

PressedTerrain TerrainPressJob::Take() noexcept {
  assert(State_ && State_->Current == State::Phase::Done);
  return State_->Result;
}

size_t TerrainPressJob::HeapBytes() const noexcept {
  return State_ ? State_->HeapBytes() : 0;
}

}
