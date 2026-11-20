#include "TerrainRefinement.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <ranges>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "HeightFieldError.h"
#include "TileGeodesy.h"
#include "math/RenderFrame.h"

namespace outshine::Generators {
namespace {

namespace Says {
constexpr std::string_view kTooManyPatches =
    "terrain needs {} height patches for its {} px error bound; the device holds {}";
constexpr std::string_view kDuplicatePatch =
    "terrain refinement has two equally authoritative sources for one patch";
constexpr std::string_view kInvalidCorridor = "terrain refinement has an invalid corridor";
}

struct PlanarBounds {
  double LowEastM = 0.0;
  double HighEastM = 0.0;
  double LowNorthM = 0.0;
  double HighNorthM = 0.0;
};

[[nodiscard]] PlanarBounds PatchPlanarBounds(Data::TileId tile, const TangentFrame &frame) {
  const Ground::GeoBounds geo = Ground::TileBounds(tile);
  PlanarBounds bounds{.LowEastM = std::numeric_limits<double>::max(),
                      .HighEastM = std::numeric_limits<double>::lowest(),
                      .LowNorthM = std::numeric_limits<double>::max(),
                      .HighNorthM = std::numeric_limits<double>::lowest()};
  for (const double latitudeDeg : {geo.MinLatDeg, geo.MaxLatDeg}) {
    for (const double longitudeDeg : {geo.MinLonDeg, geo.MaxLonDeg}) {
      const EastNorthUp at = frame.ToLocalPosition(
          {.LongitudeDeg = longitudeDeg, .LatitudeDeg = latitudeDeg, .HeightM = 0.0});
      bounds.LowEastM = std::min(bounds.LowEastM, at.EastM);
      bounds.HighEastM = std::max(bounds.HighEastM, at.EastM);
      bounds.LowNorthM = std::min(bounds.LowNorthM, at.NorthM);
      bounds.HighNorthM = std::max(bounds.HighNorthM, at.NorthM);
    }
  }
  return bounds;
}

[[nodiscard]] bool SegmentOverlaps(const TerrainRefinementCorridor &corridor,
                                   const PlanarBounds &bounds) {
  const std::array start{corridor.Start.EastM, corridor.Start.NorthM};
  const std::array end{corridor.End.EastM, corridor.End.NorthM};
  const std::array low{bounds.LowEastM - corridor.HalfWidthM,
                       bounds.LowNorthM - corridor.HalfWidthM};
  const std::array high{bounds.HighEastM + corridor.HalfWidthM,
                        bounds.HighNorthM + corridor.HalfWidthM};
  double entry = 0.0;
  double departure = 1.0;
  for (size_t axis = 0; axis < start.size(); ++axis) {
    const double delta = end[axis] - start[axis];
    if (std::abs(delta) < std::numeric_limits<double>::epsilon()) {
      if (start[axis] < low[axis] || start[axis] > high[axis]) { return false; }
      continue;
    }
    double first = (low[axis] - start[axis]) / delta;
    double last = (high[axis] - start[axis]) / delta;
    if (first > last) { std::swap(first, last); }
    entry = std::max(entry, first);
    departure = std::min(departure, last);
    if (entry > departure) { return false; }
  }
  return true;
}

[[nodiscard]] bool CorridorNeedsDetail(Data::TileId tile,
                                       const TangentFrame &frame,
                                       TerrainPageLayout layout,
                                       std::span<const TerrainRefinementCorridor> corridors) {
  if (corridors.empty()) { return false; }
  const PlanarBounds bounds = PatchPlanarBounds(tile, frame);
  const double postingM =
      std::max(bounds.HighEastM - bounds.LowEastM, bounds.HighNorthM - bounds.LowNorthM) /
      static_cast<double>(layout.Side - 1);
  return std::ranges::any_of(corridors, [&](const TerrainRefinementCorridor &corridor) {
    return postingM > corridor.MaximumPostingM && SegmentOverlaps(corridor, bounds);
  });
}

std::array<float, 2>
HeightRange(std::span<const float> reference, size_t side, HeightField::GridRegion region) {
  float low = std::numeric_limits<float>::max();
  float high = std::numeric_limits<float>::lowest();
  for (size_t row = region.Y; row <= region.Y + region.Cells; ++row) {
    const auto heights = reference.subspan(row * side + region.X, region.Cells + 1u);
    for (const float heightM : heights) {
      low = std::min(low, heightM);
      high = std::max(high, heightM);
    }
  }
  return {low, high};
}

double DistanceToPatch(Data::TileId tile,
                       std::array<float, 2> heights,
                       const TangentFrame &frame,
                       const Vec3 &eyeM) {
  const outshine::Ground::GeoBounds bounds = outshine::Ground::TileBounds(tile);
  Vec3 low = {{std::numeric_limits<double>::max(),
               std::numeric_limits<double>::max(),
               std::numeric_limits<double>::max()}};
  Vec3 high = {{-low[0], -low[1], -low[2]}};
  for (const double latitudeDeg :
       {bounds.MinLatDeg, 0.5 * (bounds.MinLatDeg + bounds.MaxLatDeg), bounds.MaxLatDeg}) {
    for (const double longitudeDeg :
         {bounds.MinLonDeg, 0.5 * (bounds.MinLonDeg + bounds.MaxLonDeg), bounds.MaxLonDeg}) {
      for (const double heightM : heights) {
        const EastNorthUp placed = frame.ToLocalPosition(
            {.LongitudeDeg = longitudeDeg, .LatitudeDeg = latitudeDeg, .HeightM = heightM});
        const Vec3 at = {{placed.EastM, placed.UpM, RenderFrame::ZOfNorth(placed.NorthM)}};
        for (size_t axis = 0; axis < 3; ++axis) {
          low[axis] = std::min(low[axis], at[axis]);
          high[axis] = std::max(high[axis], at[axis]);
        }
      }
    }
  }
  double squaredM = 0.0;
  for (size_t axis = 0; axis < 3; ++axis) {
    const double awayM = std::max({low[axis] - eyeM[axis], eyeM[axis] - high[axis], 0.0});
    squaredM += awayM * awayM;
  }
  return std::max(std::sqrt(squaredM), std::numeric_limits<double>::epsilon());
}

[[nodiscard]] std::vector<float> SampleReference(const Ground::TerrainField &heights, size_t cells);

struct PatchSelection {
  const Ground::TerrainField &Heights;
  std::vector<TerrainPatchError> *Errors;
  size_t Cells;
  int SourceZoom;
  const TangentFrame &Frame;
  TerrainPageLayout Layout;
  TerrainRefinementDetail Detail;
  std::span<const TerrainRefinementCorridor> Corridors;
  std::vector<Sheet> &Selected;
  std::vector<float> Reference;

  [[nodiscard]] TerrainPatchError Evaluate(HeightField::GridRegion region, size_t grid) {
    if (Reference.empty()) { Reference = SampleReference(Heights, Cells); }
    const size_t side = Cells + 1u;
    const double deviation = HeightField::InterpolationErrorM(Reference, side, region, grid);
    return {.DeviationM = deviation,
            .HeightsM =
                deviation > 0.0 ? HeightRange(Reference, side, region) : std::array<float, 2>{}};
  }

  [[nodiscard]] TerrainPatchError
  ErrorAt(size_t index, HeightField::GridRegion region, size_t grid) {
    if (Errors == nullptr) { return Evaluate(region, grid); }
    if (index >= Errors->size()) { Errors->resize(index + 1u); }
    TerrainPatchError &error = (*Errors)[index];
    if (error.DeviationM < 0.0) { error = Evaluate(region, grid); }
    return error;
  }

  [[nodiscard]] double
  ScreenError(Data::TileId tile, size_t index, HeightField::GridRegion region, size_t grid) {
    const TerrainPatchError error = ErrorAt(index, region, grid);
    if (Detail.OrthographicPxPerM > 0.0) { return error.DeviationM * Detail.OrthographicPxPerM; }
    if (error.DeviationM == 0.0) { return 0.0; }
    return HeightField::ProjectedErrorPx(error.DeviationM,
                                         Detail.FocalPx,
                                         DistanceToPatch(tile, error.HeightsM, Frame, Detail.EyeM));
  }

  void Append(Data::TileId tile, HeightField::GridRegion region, size_t index) {
    const auto grid = static_cast<size_t>(Layout.Side - 1);
    if (region.Cells > grid && (ScreenError(tile, index, region, grid) > Detail.ErrorPx ||
                                CorridorNeedsDetail(tile, Frame, Layout, Corridors))) {
      const size_t half = region.Cells / 2u;
      for (uint32_t child = 0; child < 4; ++child) {
        const uint32_t column = child % 2u;
        const uint32_t row = child / 2u;
        Append({.Zoom = tile.Zoom + 1, .X = tile.X * 2u + column, .Y = tile.Y * 2u + row},
               {.X = region.X + column * half, .Y = region.Y + row * half, .Cells = half},
               index * 4u + 1u + child);
      }
      return;
    }
    Selected.push_back({.Tile = tile,
                        .Nodes = {},
                        .Side = Layout.Side,
                        .Postings = static_cast<uint32_t>(Layout.Side),
                        .Virtual = tile.Zoom > SourceZoom,
                        .SourceZoom = SourceZoom});
  }
};

struct AxisSample {
  size_t Low, High;
  double Fraction;
};

std::vector<AxisSample> SamplingAxis(size_t postings, size_t cells) {
  std::vector<AxisSample> locations;
  locations.reserve(cells + 1u);
  for (size_t at = 0; at <= cells; ++at) {
    const double fraction =
        static_cast<double>(at) / static_cast<double>(cells) * static_cast<double>(postings - 1u);
    const auto low = static_cast<size_t>(fraction);
    locations.push_back({.Low = low,
                         .High = std::min(low + 1u, postings - 1u),
                         .Fraction = fraction - static_cast<double>(low)});
  }
  return locations;
}

std::vector<float> SampleReference(const Ground::TerrainField &heights, size_t cells) {
  const size_t side = cells + 1u;
  const auto columns = SamplingAxis(heights.Cols(), cells);
  const auto rows = SamplingAxis(heights.Rows(), cells);
  const auto field = heights.Field();
  std::vector<float> reference(side * side);
  for (size_t row = 0; row < side; ++row) {
    const auto y = rows[row];
    for (size_t column = 0; column < side; ++column) {
      const auto x = columns[column];
      const double top =
          field[y.Low, x.Low] * (1.0 - x.Fraction) + field[y.Low, x.High] * x.Fraction;
      const double bottom =
          field[y.High, x.Low] * (1.0 - x.Fraction) + field[y.High, x.High] * x.Fraction;
      reference[row * side + column] =
          static_cast<float>(top * (1.0 - y.Fraction) + bottom * y.Fraction);
    }
  }
  return reference;
}

void SelectSourcePatches(const TerrainRefinementSource &source,
                         const TangentFrame &frame,
                         TerrainPageLayout layout,
                         TerrainRefinementDetail detail,
                         std::span<const TerrainRefinementCorridor> corridors,
                         std::vector<Sheet> &selected) {
  if (source.Page == nullptr) { return; }
  const Sheet &sheet = *source.Page;
  if (sheet.Virtual || sheet.Side != layout.Side || source.Heights == nullptr ||
      !source.Heights->Meshable()) {
    selected.push_back(sheet);
    return;
  }
  const auto grid = static_cast<size_t>(layout.Side - 1);
  size_t cells = grid;
  while (cells + 1u < std::max(source.Heights->Rows(), source.Heights->Cols())) { cells *= 2u; }
  std::unique_lock<std::mutex> errorsLock;
  if (source.Errors) { errorsLock = std::unique_lock(source.Errors->Mutex); }
  PatchSelection selection{.Heights = *source.Heights,
                           .Errors = source.Errors ? &source.Errors->Nodes : nullptr,
                           .Cells = cells,
                           .SourceZoom = sheet.Tile.Zoom,
                           .Frame = frame,
                           .Layout = layout,
                           .Detail = detail,
                           .Corridors = corridors,
                           .Selected = selected,
                           .Reference = {}};
  selection.Append(sheet.Tile, {.Cells = cells}, 0);
}

std::expected<std::vector<Sheet>, std::string> UniquePatches(std::vector<Sheet> selected) {
  std::vector<Sheet> unique;
  unique.reserve(selected.size());
  std::map<std::tuple<int, uint32_t, uint32_t>, size_t> indices;
  for (Sheet &sheet : selected) {
    const auto key = std::tuple{sheet.Tile.Zoom, sheet.Tile.X, sheet.Tile.Y};
    const auto [at, inserted] = indices.emplace(key, unique.size());
    if (inserted) {
      unique.push_back(std::move(sheet));
      continue;
    }
    Sheet &held = unique[at->second];
    if (held.Virtual == sheet.Virtual) {
      if (!held.Virtual || held.SourceZoom == sheet.SourceZoom) {
        return std::unexpected(std::string(Says::kDuplicatePatch));
      }
      if (sheet.SourceZoom > held.SourceZoom) { held = std::move(sheet); }
      continue;
    }
    if (held.Virtual) { held = std::move(sheet); }
  }
  return unique;
}

}

std::shared_ptr<TerrainPatchErrors>
TerrainRefinementCache::Retain(const std::shared_ptr<const Ground::TerrainField> &field,
                               TerrainRefinementCache &previous) {
  const std::weak_ptr key{field};
  if (Side_ == previous.Side_) {
    auto retained = previous.Fields_.extract(key);
    if (!retained.empty()) { return Fields_.insert(std::move(retained)).position->second; }
  }
  const auto [at, inserted] = Fields_.try_emplace(key);
  if (inserted) { at->second = std::make_shared<TerrainPatchErrors>(); }
  return at->second;
}

TerrainRefinementJob::TerrainRefinementJob(std::span<const TerrainRefinementSource> sources,
                                           TangentFrame frame,
                                           TerrainPageLayout layout,
                                           TerrainRefinementDetail detail,
                                           size_t maximumPatches,
                                           std::span<const TerrainRefinementCorridor> corridors)
    : Sources_(sources.begin(), sources.end()),
      Corridors_(corridors.begin(), corridors.end()),
      Frame_(frame),
      Layout_(layout),
      Detail_(detail),
      MaximumPatches_(maximumPatches) {}

std::expected<bool, std::string> TerrainRefinementJob::Advance(size_t sourcesMost) {
  if (Complete_) { return true; }
  if (!Layout_.Valid()) {
    Complete_ = true;
    return true;
  }
  if (!std::ranges::all_of(Corridors_, [](const TerrainRefinementCorridor &corridor) {
        return std::isfinite(corridor.Start.EastM) && std::isfinite(corridor.Start.NorthM) &&
               std::isfinite(corridor.End.EastM) && std::isfinite(corridor.End.NorthM) &&
               std::isfinite(corridor.HalfWidthM) && corridor.HalfWidthM >= 0.0 &&
               std::isfinite(corridor.MaximumPostingM) && corridor.MaximumPostingM > 0.0;
      })) {
    return std::unexpected(std::string(Says::kInvalidCorridor));
  }
  const size_t count = std::min(sourcesMost, Sources_.size() - NextSource_);
  const size_t end = NextSource_ + count;
  for (; NextSource_ < end; ++NextSource_) {
    const auto sourceAt = std::chrono::steady_clock::now();
    SelectSourcePatches(Sources_[NextSource_], Frame_, Layout_, Detail_, Corridors_, Selected_);
    LongestSourceMs_ = std::max(
        LongestSourceMs_,
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - sourceAt)
            .count());
  }
  if (NextSource_ < Sources_.size()) { return false; }
  const auto deduplicationAt = std::chrono::steady_clock::now();
  auto unique = UniquePatches(std::move(Selected_));
  DeduplicationMs_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - deduplicationAt)
          .count();
  if (!unique) { return std::unexpected(std::move(unique.error())); }
  if (unique->size() > MaximumPatches_) {
    uint32_t sourceSide = 0;
    for (const TerrainRefinementSource &source : Sources_) {
      if (source.Heights != nullptr) {
        sourceSide = std::max({sourceSide, source.Heights->Rows(), source.Heights->Cols()});
      }
    }
    return std::unexpected(
        std::format(Says::kTooManyPatches, unique->size(), Detail_.ErrorPx, MaximumPatches_) +
        std::format("; {} sources, up to {} postings, {} grid nodes, focal {:.1f} px, eye ({:.1f}, "
                    "{:.1f}, {:.1f}) m",
                    Sources_.size(),
                    sourceSide,
                    Layout_.Side,
                    Detail_.FocalPx,
                    Detail_.EyeM[0],
                    Detail_.EyeM[1],
                    Detail_.EyeM[2]));
  }
  Result_ = std::move(*unique);
  Complete_ = true;
  return true;
}

std::vector<Sheet> TerrainRefinementJob::Take() noexcept {
  assert(Complete_);
  return std::move(Result_);
}

std::expected<std::vector<Sheet>, std::string>
RefineTerrain(std::span<const TerrainRefinementSource> sources,
              const TangentFrame &frame,
              TerrainPageLayout layout,
              TerrainRefinementDetail detail,
              size_t maximumPatches,
              std::span<const TerrainRefinementCorridor> corridors) {
  TerrainRefinementJob job(sources, frame, layout, detail, maximumPatches, corridors);
  while (true) {
    auto advanced = job.Advance(sources.size());
    if (!advanced) { return std::unexpected(std::move(advanced.error())); }
    if (*advanced) { return std::move(job).Take(); }
  }
}

}
