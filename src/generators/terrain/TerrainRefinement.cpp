#include "TerrainRefinement.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "HeightFieldError.h"
#include "TileGeodesy.h"
#include "math/RenderFrame.h"

namespace outshine::Generators {
namespace {

namespace Says {
constexpr std::string_view kTooManyPatches =
    "terrain needs {} height patches for its {} px error bound; the device holds {}";
}

struct ErrorPatch {
  Data::TileId Tile;
  HeightField::GridRegion Region;
  double ErrorM = 0.0;
  double LowM = 0.0;
  double HighM = 0.0;
  std::array<size_t, 4> Children{};
};

size_t BuildErrors(std::vector<ErrorPatch> &tree,
                   std::span<const float> reference,
                   size_t side,
                   Data::TileId tile,
                   HeightField::GridRegion region,
                   size_t grid) {
  const size_t index = tree.size();
  ErrorPatch patch{.Tile = tile, .Region = region};
  patch.ErrorM = HeightField::InterpolationErrorM(reference, side, region, grid);
  patch.LowM = std::numeric_limits<double>::max();
  patch.HighM = std::numeric_limits<double>::lowest();
  for (size_t row = region.Y; row <= region.Y + region.Cells; ++row) {
    for (size_t column = region.X; column <= region.X + region.Cells; ++column) {
      const double heightM = reference[row * side + column];
      patch.LowM = std::min(patch.LowM, heightM);
      patch.HighM = std::max(patch.HighM, heightM);
    }
  }
  tree.push_back(patch);
  if (region.Cells > grid) {
    const size_t half = region.Cells / 2u;
    for (uint32_t child = 0; child < patch.Children.size(); ++child) {
      const uint32_t column = child % 2u;
      const uint32_t row = child / 2u;
      patch.Children[child] =
          BuildErrors(tree,
                      reference,
                      side,
                      {.Zoom = tile.Zoom + 1, .X = tile.X * 2u + column, .Y = tile.Y * 2u + row},
                      {.X = region.X + column * half, .Y = region.Y + row * half, .Cells = half},
                      grid);
      patch.ErrorM = std::max(patch.ErrorM, tree[patch.Children[child]].ErrorM);
    }
  }
  tree[index] = patch;
  return index;
}

double DistanceToPatch(const ErrorPatch &patch, const TangentFrame &frame, const Vec3 &eyeM) {
  const outshine::Ground::GeoBounds bounds = outshine::Ground::TileBounds(patch.Tile);
  Vec3 low = {{std::numeric_limits<double>::max(),
               std::numeric_limits<double>::max(),
               std::numeric_limits<double>::max()}};
  Vec3 high = {{-low[0], -low[1], -low[2]}};
  for (const double latitudeDeg :
       {bounds.MinLatDeg, 0.5 * (bounds.MinLatDeg + bounds.MaxLatDeg), bounds.MaxLatDeg}) {
    for (const double longitudeDeg :
         {bounds.MinLonDeg, 0.5 * (bounds.MinLonDeg + bounds.MaxLonDeg), bounds.MaxLonDeg}) {
      for (const double heightM : {patch.LowM, patch.HighM}) {
        const EastNorthUp placed = frame.Place(
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

void SelectPatches(std::span<const ErrorPatch> tree,
                   const ErrorPatch &patch,
                   int sourceZoom,
                   const TangentFrame &frame,
                   TerrainPageLayout layout,
                   TerrainRefinementDetail detail,
                   std::vector<Sheet> &selected) {
  const double errorPx =
      detail.OrthographicPxPerM > 0.0
          ? patch.ErrorM * detail.OrthographicPxPerM
          : HeightField::ProjectedErrorPx(
                patch.ErrorM, detail.FocalPx, DistanceToPatch(patch, frame, detail.EyeM));
  const auto grid = static_cast<size_t>(layout.Side - 1);
  if (patch.Region.Cells > grid && errorPx > detail.ErrorPx) {
    for (const size_t child : patch.Children) {
      SelectPatches(tree, tree[child], sourceZoom, frame, layout, detail, selected);
    }
    return;
  }
  selected.push_back({.Tile = patch.Tile,
                      .Nodes = {},
                      .Side = layout.Side,
                      .Postings = static_cast<uint32_t>(layout.Side),
                      .Virtual = patch.Tile.Zoom > sourceZoom,
                      .SourceZoom = sourceZoom});
}

}

std::expected<std::vector<Sheet>, std::string>
RefineTerrain(std::span<const TerrainRefinementSource> sources,
              const TangentFrame &frame,
              TerrainPageLayout layout,
              TerrainRefinementDetail detail,
              size_t maximumPatches) {
  std::vector<Sheet> selected;
  if (!layout.Valid()) { return selected; }
  const auto grid = static_cast<size_t>(layout.Side - 1);
  for (const TerrainRefinementSource &source : sources) {
    if (source.Page == nullptr) { continue; }
    const Sheet &sheet = *source.Page;
    if (sheet.Virtual || sheet.Side != layout.Side || source.Heights == nullptr ||
        !source.Heights->Meshable()) {
      selected.push_back(sheet);
      continue;
    }
    size_t cells = grid;
    while (cells + 1u < std::max(source.Heights->Rows(), source.Heights->Cols())) { cells *= 2u; }
    const size_t side = cells + 1u;
    std::vector<float> reference(side * side);
    for (size_t row = 0; row < side; ++row) {
      for (size_t column = 0; column < side; ++column) {
        reference[row * side + column] = source.Heights->PostingM(
            {.Col = static_cast<double>(column) / static_cast<double>(cells),
             .Row = static_cast<double>(row) / static_cast<double>(cells)});
      }
    }
    std::vector<ErrorPatch> tree;
    (void)BuildErrors(tree, reference, side, sheet.Tile, {.Cells = cells}, grid);
    SelectPatches(tree, tree.front(), sheet.Tile.Zoom, frame, layout, detail, selected);
  }
  if (selected.size() > maximumPatches) {
    return std::unexpected(
        std::format(Says::kTooManyPatches, selected.size(), detail.ErrorPx, maximumPatches));
  }
  return selected;
}

}
