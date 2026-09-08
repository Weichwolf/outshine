#include "HeightSheets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <string_view>
#include <string>
#include <span>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <utility>

#include "GroundLattice.h"
#include "HeightFieldError.h"
#include "TileGeodesy.h"
#include "math/RenderFrame.h"

namespace outshine {
namespace {

namespace Says {
constexpr std::string_view kTooManyPatches =
    "terrain needs {} height patches for its {} px error bound; the device holds {}";
}

constexpr size_t kGrid = Render::GroundLattice::kSide - 1;

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
                   HeightField::GridRegion region) {
  const size_t index = tree.size();
  ErrorPatch patch{.Tile = tile, .Region = region};
  patch.ErrorM = HeightField::InterpolationErrorM(reference, side, region, kGrid);
  patch.LowM = std::numeric_limits<double>::max();
  patch.HighM = std::numeric_limits<double>::lowest();
  for (size_t y = region.Y; y <= region.Y + region.Cells; ++y) {
    for (size_t x = region.X; x <= region.X + region.Cells; ++x) {
      const double height = reference[y * side + x];
      patch.LowM = std::min(patch.LowM, height);
      patch.HighM = std::max(patch.HighM, height);
    }
  }
  tree.push_back(patch);
  if (region.Cells > kGrid) {
    const size_t half = region.Cells / 2;
    for (uint32_t child = 0; child < patch.Children.size(); ++child) {
      const uint32_t x = child % 2;
      const uint32_t y = child / 2;
      patch.Children[child] =
          BuildErrors(tree,
                      reference,
                      side,
                      {.Zoom = tile.Zoom + 1, .X = tile.X * 2 + x, .Y = tile.Y * 2 + y},
                      {.X = region.X + x * half, .Y = region.Y + y * half, .Cells = half});
      patch.ErrorM = std::max(patch.ErrorM, tree[patch.Children[child]].ErrorM);
    }
  }
  tree[index] = patch;
  return index;
}

double DistanceToPatch(const ErrorPatch &patch, const TangentFrame &frame, const Vec3 &eye) {
  const Ground::GeoBounds bounds = Ground::TileBounds(patch.Tile);
  Vec3 low = {{std::numeric_limits<double>::max(),
               std::numeric_limits<double>::max(),
               std::numeric_limits<double>::max()}};
  Vec3 high = {{-low[0], -low[1], -low[2]}};
  for (const double lat :
       {bounds.MinLatDeg, 0.5 * (bounds.MinLatDeg + bounds.MaxLatDeg), bounds.MaxLatDeg}) {
    for (const double lon :
         {bounds.MinLonDeg, 0.5 * (bounds.MinLonDeg + bounds.MaxLonDeg), bounds.MaxLonDeg}) {
      for (const double height : {patch.LowM, patch.HighM}) {
        const EastNorthUp placed =
            frame.Place({.LongitudeDeg = lon, .LatitudeDeg = lat, .HeightM = height});
        const Vec3 at = {{placed.EastM, placed.UpM, RenderFrame::ZOfNorth(placed.NorthM)}};
        for (size_t axis = 0; axis < 3; ++axis) {
          low[axis] = std::min(low[axis], at[axis]);
          high[axis] = std::max(high[axis], at[axis]);
        }
      }
    }
  }
  double squared = 0.0;
  for (size_t axis = 0; axis < 3; ++axis) {
    const double away = std::max({low[axis] - eye[axis], eye[axis] - high[axis], 0.0});
    squared += away * away;
  }
  return std::max(std::sqrt(squared), std::numeric_limits<double>::epsilon());
}

void SelectPatches(std::span<const ErrorPatch> tree,
                   const ErrorPatch &patch,
                   int sourceZoom,
                   const TangentFrame &frame,
                   HeightSheets::Detail detail,
                   std::vector<Sheet> &selected) {
  const double errorPx =
      detail.OrthographicPxPerM > 0.0
          ? patch.ErrorM * detail.OrthographicPxPerM
          : HeightField::ProjectedErrorPx(
                patch.ErrorM, detail.FocalPx, DistanceToPatch(patch, frame, detail.EyeM));
  if (patch.Region.Cells > kGrid && errorPx > detail.ErrorPx) {
    for (const size_t child : patch.Children) {
      SelectPatches(tree, tree[child], sourceZoom, frame, detail, selected);
    }
    return;
  }
  selected.push_back({.Tile = patch.Tile,
                      .Side = Render::GroundLattice::kSide,
                      .Postings = static_cast<uint32_t>(kGrid + 1),
                      .Virtual = patch.Tile.Zoom > sourceZoom,
                      .SourceZoom = sourceZoom});
}

}

bool HeightSheets::RefineByError(Patchwork &laid,
                                 const Ground::GroundStream &ground,
                                 Detail detail,
                                 std::string &error) {
  std::vector<Sheet> selected;
  for (const Sheet &sheet : laid.Sheets) {
    if (sheet.Virtual || sheet.Side != Render::GroundLattice::kSide) {
      selected.push_back(sheet);
      continue;
    }
    const auto *field = FieldAt(ground, sheet.Tile);
    if (field == nullptr || !field->Meshable()) {
      selected.push_back(sheet);
      continue;
    }
    size_t cells = kGrid;
    while (cells + 1 < std::max(field->Rows(), field->Cols())) { cells *= 2; }
    const size_t side = cells + 1;
    std::vector<float> reference(side * side);
    for (size_t y = 0; y < side; ++y) {
      for (size_t x = 0; x < side; ++x) {
        reference[y * side + x] =
            field->PostingM({.Col = static_cast<double>(x) / static_cast<double>(cells),
                             .Row = static_cast<double>(y) / static_cast<double>(cells)});
      }
    }
    std::vector<ErrorPatch> tree;
    (void)BuildErrors(tree, reference, side, sheet.Tile, {.Cells = cells});
    SelectPatches(tree, tree.front(), sheet.Tile.Zoom, Frame_, detail, selected);
  }
  if (selected.size() > Render::GroundLattice::kPages) {
    error = std::format(
        Says::kTooManyPatches, selected.size(), detail.ErrorPx, Render::GroundLattice::kPages);
    return false;
  }
  laid.Sheets = std::move(selected);
  return true;
}

}
