#include "GroundPatchSelection.h"
#include "GroundLattice.h"
#include "GroundPatchError.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace outshine::Render {
namespace {
constexpr float kCells = static_cast<float>(GroundLattice::kSide - 1);
constexpr size_t kPatchCells = GroundLattice::kGroundPatchCells;
constexpr size_t kPatchColumns = (GroundLattice::kSide - 1) / kPatchCells;
constexpr std::array<size_t, 4> kAffineRow{3, 7, 11, 15};
using Interval = std::array<double, 2>;

struct Volume {
  std::array<double, 3> Low;
  std::array<double, 3> High;
};

Interval Coordinates(std::span<const float> fractions, size_t first, bool uniform) {
  if (uniform) {
    return {static_cast<float>(first) / kCells, static_cast<float>(first + kPatchCells) / kCells};
  }
  const auto [low, high] = std::ranges::minmax_element(fractions.subspan(first, kPatchCells + 1));
  return {*low, *high};
}

Volume LocalVolume(const GroundInstance &tile,
                   const std::array<Interval, 2> &uv,
                   const std::array<float, 2> &heights) {
  constexpr double infinity = std::numeric_limits<double>::infinity();
  Volume box{.Low = {infinity, infinity, infinity}, .High = {-infinity, -infinity, -infinity}};
  for (const double a : uv[0]) {
    for (const double b : uv[1]) {
      for (size_t axis = 0; axis < 2; ++axis) {
        const double north = (1 - a) * tile.Corners[axis] + a * tile.Corners[axis + 2];
        const double south = (1 - a) * tile.Corners[axis + 4] + a * tile.Corners[axis + 6];
        const double value = (1 - b) * north + b * south;
        box.Low[axis] = std::min(box.Low[axis], value);
        box.High[axis] = std::max(box.High[axis], value);
      }
    }
  }
  double reach = 0;
  for (size_t axis = 0; axis < 2; ++axis) {
    reach += std::max(box.Low[axis] * box.Low[axis], box.High[axis] * box.High[axis]);
  }
  const double sag = 0.5 * reach * tile.SagInv;
  box.Low[2] = heights[0] - std::max(sag, 0.0);
  box.High[2] = heights[1] - std::min(sag, 0.0);
  return box;
}

GroundPatchSelection::Bounds WorldVolume(const GroundInstance &tile, const Volume &box) {
  GroundPatchSelection::Bounds bounds;
  bounds.Low[3] = 1;
  for (size_t axis = 0; axis < 3; ++axis) {
    double low = tile.Row[12 + axis];
    double high = low;
    double scale = std::abs(low);
    for (size_t k = 0; k < 3; ++k) {
      const double a = box.Low[k] * tile.Row[k * 4u + axis];
      const double b = box.High[k] * tile.Row[k * 4u + axis];
      low += std::min(a, b);
      high += std::max(a, b);
      scale += std::max(std::abs(a), std::abs(b));
    }
    const double margin = 32.0 * std::numeric_limits<float>::epsilon() * (scale + 1);
    bounds.Low[axis] = static_cast<float>(low - margin);
    bounds.High[axis] = static_cast<float>(high + margin);
    if (!std::isfinite(bounds.Low[axis]) || !std::isfinite(bounds.High[axis])) {
      bounds.Low[3] = 0;
    }
  }
  return bounds;
}
}

GroundPatchSelection::GroundPatchSelection() {
  Fractions({});
}

void GroundPatchSelection::Fractions(std::span<const float> fractions) {
  AlignedFractions_ = true;
  UniformFractions_ = true;
  FiniteFractions_ = true;
  for (size_t i = 0; i < Fractions_.size(); ++i) {
    Fractions_[i] = fractions.empty() ? static_cast<float>(i) / kCells : fractions[i];
    UniformFractions_ &= Fractions_[i] == static_cast<float>(i) / kCells;
    FiniteFractions_ &= std::isfinite(Fractions_[i]);
    AlignedFractions_ &=
        std::isfinite(Fractions_[i]) && std::round(Fractions_[i] * kCells) == static_cast<float>(i);
  }
}

void GroundPatchSelection::Page(PageId page, std::span<const float> heights) {
  if (page >= Pages_.size()) { Pages_.resize(static_cast<size_t>(page) + 1u); }
  auto &range = Pages_[page];
  range.Revision = ++Revision_;
  range.Finite = std::ranges::all_of(heights, [](float h) { return std::isfinite(h); });
  const auto [low, high] = std::ranges::minmax_element(heights);
  range.Whole = {*low, *high};
  constexpr size_t side = GroundLattice::kPageSide;
  constexpr size_t cells = GroundLattice::kGroundPatchCells;
  for (size_t patch = 0; patch < kPatchCount; ++patch) {
    range.Errors[patch] = GroundPatchHeightErrors(heights, patch);
    auto &bounds = range.Patches[patch];
    bounds = {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
    for (size_t y = 0; y <= cells; ++y) {
      for (size_t x = 0; x <= cells; ++x) {
        const float h = heights[(patch / 4u * cells + y + 1u) * side + patch % 4u * cells + x + 1u];
        bounds[0] = std::min(bounds[0], h);
        bounds[1] = std::max(bounds[1], h);
      }
    }
  }
}

void GroundPatchSelection::Sources(std::span<const GroundInstance> tiles, uint32_t real) {
  Tiles_.clear();
  Tiles_.reserve(tiles.size());
  for (size_t i = 0; i < tiles.size(); ++i) { Tiles_.push_back(BoundsFor(tiles[i], i >= real)); }
  Ready_ = false;
}

void GroundPatchSelection::BeginFrame() {
  Seen_.clear();
  Ready_ = false;
}

void GroundPatchSelection::Include(size_t tile) {
  const auto &source = Tiles_[tile];
  const bool current =
      source.Page < Pages_.size() && Pages_[source.Page].Revision == source.Revision;
  for (const auto &bound : source.Patches) { Seen_.push_back(current ? bound : Bounds{}); }
}

GroundPatchSelection::TileRange GroundPatchSelection::BoundsFor(const GroundInstance &tile,
                                                                bool uniform) const {
  TileRange result;
  if (!std::isfinite(tile.Page) || tile.Page < 0 ||
      static_cast<double>(tile.Page) >= static_cast<double>(Pages_.size()) ||
      tile.Row[kAffineRow[0]] != 0 || tile.Row[kAffineRow[1]] != 0 ||
      tile.Row[kAffineRow[2]] != 0 || tile.Row[kAffineRow[3]] != 1 ||
      (!uniform && !FiniteFractions_)) {
    return result;
  }
  result.Page = static_cast<PageId>(tile.Page);
  const auto &page = Pages_[result.Page];
  result.Revision = page.Revision;
  if (!page.Finite) { return result; }
  for (size_t patch = 0; patch < kPatchCount; ++patch) {
    const auto u = Coordinates(Fractions_, patch % kPatchColumns * kPatchCells, uniform);
    const auto v = Coordinates(Fractions_, patch / kPatchColumns * kPatchCells, uniform);
    const auto heights = uniform || AlignedFractions_ ? page.Patches[patch] : page.Whole;
    result.Patches[patch] = WorldVolume(tile, LocalVolume(tile, {u, v}, heights));
    result.Patches[patch].Errors = uniform || UniformFractions_
                                       ? GroundPatchWorldErrors(tile, patch, page.Errors[patch])
                                       : std::array{0.0f,
                                                    std::numeric_limits<float>::infinity(),
                                                    std::numeric_limits<float>::infinity(),
                                                    std::numeric_limits<float>::infinity()};
  }
  return result;
}
}
