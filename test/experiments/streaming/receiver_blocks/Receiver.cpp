#include "Receiver.h"
#include "Projection.h"
#include "receiver_coverage/Coverage.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace ReceiverBlocks {
namespace {
using ReceiverCoverage::Rectangle;
constexpr uint32_t Cell = 16;
constexpr uint32_t Halo = 8;
constexpr uint32_t Tile = 16;
constexpr uint64_t MaximumArea = 16;

std::array<double, 4> Transform(const Matrix &matrix, const std::array<double, 3> &point) noexcept {
  std::array<double, 4> result{};
  for (size_t row = 0; row < result.size(); ++row) {
    result[row] = matrix[12 + row];
    for (size_t column = 0; column < point.size(); ++column) {
      result[row] += matrix[4 * column + row] * point[column];
    }
  }
  return result;
}

std::optional<Rectangle> Cells(const Domain &box, const Region &region, uint32_t atlas) noexcept {
  if (box[0].Lo > 1 || box[0].Hi < -1 || box[1].Lo > 1 || box[1].Hi < -1 || box[2].Lo > 1 ||
      box[2].Hi < 0) {
    return std::nullopt;
  }
  const double side = static_cast<double>(atlas) / 2;
  const auto px = [side](double x) { return (std::clamp(x, -1., 1.) + 1) * .5 * side; };
  const auto py = [side](double y) { return (1 - std::clamp(y, -1., 1.)) * .5 * side; };
  const auto low = [side](double x) {
    return static_cast<uint32_t>(std::clamp(std::floor(x) - Halo, 0., side - 1));
  };
  const auto high = [side](double x) {
    return static_cast<uint32_t>(std::clamp(std::ceil(x) + Halo, 0., side - 1));
  };
  return Rectangle{.Left = (low(px(box[0].Lo)) + region.X) / Cell,
                   .Right = (high(px(box[0].Hi)) + region.X) / Cell,
                   .Top = (low(py(box[1].Hi)) + region.Y) / Cell,
                   .Bottom = (high(py(box[1].Lo)) + region.Y) / Cell};
}

class Walker {
public:
  const Camera &View;
  std::span<const float> Depth;
  std::span<const Region> Regions;
  uint32_t Atlas;
  ReceiverCoverage::RowMask &Mask;
  Costs &Work;
  const Prepared &Model;

  void Mark(Rectangle r) const noexcept {
    Mask.Mark(r);
    const uint64_t rows = r.Bottom - r.Top + 1;
    Work.WordUpdates += rows * (r.Right / 64 - r.Left / 64 + 1);
    Work.MarkedCellArea += rows * (r.Right - r.Left + 1);
  }

  void Point(uint32_t x, uint32_t y, float depth) const noexcept {
    ++Work.Points;
    const std::array ndc{2 * (static_cast<double>(x) + .5) / View.Width - 1,
                         1 - 2 * (static_cast<double>(y) + .5) / View.Height,
                         1 - 2 * static_cast<double>(depth)};
    const auto point = Transform(View.Inverse, ndc);
    if (!std::isfinite(point[3]) || point[3] <= 0) {
      ++Work.Unknown;
      Mark({.Left = 0, .Right = Atlas / Cell - 1, .Top = 0, .Bottom = Atlas / Cell - 1});
      return;
    }
    std::array<double, 3> world{};
    for (size_t i = 0; i < world.size(); ++i) { world[i] = point[i] / point[3] - View.Eye[i]; }
    for (const auto &region : Regions) {
      const auto light = Transform(region.Clip, world);
      Domain box{};
      for (size_t i = 0; i < box.size(); ++i) {
        const double value = light[i] / light[3];
        box[i] = {.Lo = value, .Hi = value};
      }
      if (const auto r = Cells(box, region, Atlas)) { Mark(*r); }
    }
  }

  struct PixelBox {
    uint32_t Left;
    uint32_t Right;
    uint32_t Top;
    uint32_t Bottom;
  };

  [[nodiscard]] std::array<float, 2> DepthRange(PixelBox pixels) const noexcept {
    const auto [left, right, top, bottom] = pixels;
    float minimum = std::numeric_limits<float>::infinity();
    float maximum = 0;
    for (uint32_t y = top; y < bottom; ++y) {
      for (uint32_t x = left; x < right; ++x) {
        ++Work.DepthReads;
        const float d = Depth[size_t{y} * View.Width + x];
        if (std::isfinite(d) && d > 0 && d <= 1) {
          minimum = std::min(minimum, d);
          maximum = std::max(maximum, d);
        }
      }
    }
    return {minimum, maximum};
  }

  struct Plan {
    bool Known = false;
    bool Covered = false;
    uint64_t Area = 0;
    std::array<std::optional<Rectangle>, 4> Rectangles{};
  };

  [[nodiscard]] Plan Map(const Domain &input) const noexcept {
    std::array<Domain, 4> output{};
    ++Work.Projections;
    Plan result;
    result.Known = Model.Project(input, std::span(output).first(Regions.size()));
    result.Covered = result.Known;
    if (!result.Known) { return result; }
    for (size_t i = 0; i < Regions.size(); ++i) {
      const auto rectangle = Cells(output[i], Regions[i], Atlas);
      result.Rectangles[i] = rectangle;
      if (!rectangle) { continue; }
      const auto &r = *rectangle;
      result.Area = std::max(result.Area, uint64_t{r.Right - r.Left + 1} * (r.Bottom - r.Top + 1));
      if (result.Covered) {
        ++Work.CoversCalls;
        result.Covered = Mask.Covers(r);
      }
    }
    return result;
  }

  void Visit(uint32_t left, uint32_t right, uint32_t top, uint32_t bottom) const noexcept {
    ++Work.Nodes;
    const auto [minimum, maximum] =
        DepthRange({.Left = left, .Right = right, .Top = top, .Bottom = bottom});
    if (!std::isfinite(minimum)) { return; }
    if (right - left == 1 && bottom - top == 1) {
      Point(left, top, minimum);
      return;
    }
    const Domain input{
        {{.Lo = 2 * (static_cast<double>(left) + .5) / View.Width - 1,
          .Hi = 2 * (static_cast<double>(right) - .5) / View.Width - 1},
         {.Lo = 1 - 2 * (static_cast<double>(bottom) - .5) / View.Height,
          .Hi = 1 - 2 * (static_cast<double>(top) + .5) / View.Height},
         {.Lo = 1 - 2 * static_cast<double>(maximum), .Hi = 1 - 2 * static_cast<double>(minimum)}}};
    const auto plan = Map(input);
    if (plan.Covered) {
      ++Work.Covered;
      return;
    }
    if (!plan.Known || plan.Area > MaximumArea) {
      ++Work.Refined;
      const uint32_t mx = right - left > 1 ? (left + right) / 2 : right;
      const uint32_t my = bottom - top > 1 ? (top + bottom) / 2 : bottom;
      Visit(left, mx, top, my);
      if (mx < right) { Visit(mx, right, top, my); }
      if (my < bottom) { Visit(left, mx, my, bottom); }
      if (mx < right && my < bottom) { Visit(mx, right, my, bottom); }
      return;
    }
    for (const auto &rectangle : plan.Rectangles) {
      if (rectangle) { Mark(*rectangle); }
    }
  }
};
}

bool Append(const Camera &camera,
            std::span<const float> depth,
            std::span<const Region> regions,
            uint32_t atlasSide,
            ReceiverCoverage::RowMask &mask,
            Costs &costs) noexcept {
  if (camera.Width == 0 || camera.Height == 0 || camera.Width > 8192 || camera.Height > 8192 ||
      depth.size() != size_t{camera.Width} * camera.Height || regions.empty() ||
      regions.size() > 4 || atlasSide == 0 || atlasSide > 8192 || atlasSide % (2 * Cell) != 0 ||
      mask.Side() != atlasSide / Cell) {
    return false;
  }
  float minimum = std::numeric_limits<float>::infinity();
  float maximum = 0;
  uint64_t valid = 0;
  std::array<Matrix, 4> matrices{};
  for (size_t i = 0; i < regions.size(); ++i) {
    if (regions[i].X > atlasSide / 2 || regions[i].Y > atlasSide / 2) { return false; }
    matrices[i] = regions[i].Clip;
  }
  for (const float d : depth) {
    ++costs.DepthReads;
    if (std::isfinite(d) && d > 0 && d <= 1) {
      minimum = std::min(minimum, d);
      maximum = std::max(maximum, d);
      ++valid;
    }
  }
  costs.SourceSamples += valid;
  if (valid == 0) { return true; }
  const Domain frame{
      {{.Lo = -1, .Hi = 1},
       {.Lo = -1, .Hi = 1},
       {.Lo = 1 - 2 * static_cast<double>(maximum), .Hi = 1 - 2 * static_cast<double>(minimum)}}};
  const auto model =
      Prepared::Make(camera.Inverse, camera.Eye, std::span(matrices).first(regions.size()), frame);
  if (!model) {
    ++costs.Unknown;
    mask.Mark({.Left = 0, .Right = atlasSide / Cell - 1, .Top = 0, .Bottom = atlasSide / Cell - 1});
    return true;
  }
  ++costs.Modeled;
  const Walker walker{.View = camera,
                      .Depth = depth,
                      .Regions = regions,
                      .Atlas = atlasSide,
                      .Mask = mask,
                      .Work = costs,
                      .Model = *model};
  for (uint32_t y = 0; y < camera.Height; y += Tile) {
    for (uint32_t x = 0; x < camera.Width; x += Tile) {
      walker.Visit(x, std::min(camera.Width, x + Tile), y, std::min(camera.Height, y + Tile));
    }
  }
  return true;
}
}
