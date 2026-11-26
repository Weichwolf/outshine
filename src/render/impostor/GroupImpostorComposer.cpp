#include "GroupImpostorComposer.h"
#include "ImpostorAtlas.h"
#include "ImpostorViewFrame.h"
#include "Vec3.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {

struct Projection {
  double CentreX, CentreY, CentreZ;
  double HalfExtentM;
};

struct Interval {
  int Begin;
  int End;
};

Interval Footprint(double centre, double half, const GroupImpostorLayout &layout) {
  const auto pixel = [&](double offset) {
    return (offset / layout.HalfExtentM + 1.0) * layout.Pixels * 0.5 - 0.5;
  };
  const auto inset = [&](double index) {
    return static_cast<int>(std::clamp(index, 0.0, static_cast<double>(layout.Pixels)));
  };
  return {.Begin = inset(std::floor(pixel(centre - half))),
          .End = inset(std::ceil(pixel(centre + half)) + 1.0)};
}

struct SourceAxis {
  double Scale;
  double Offset;
  double Limit;

  [[nodiscard]] int At(int pixel) const {
    const double index = Scale * pixel + Offset;
    return index >= 0 && index < Limit ? static_cast<int>(index) : -1;
  }
};

void RasterInstance(ImpostorAtlas::View &target,
                    const GroupImpostorLayout &layout,
                    const ImpostorAtlas::View &source,
                    int sourceSide,
                    const Projection &view) {
  const auto rows = Footprint(view.CentreY, view.HalfExtentM, layout);
  const auto columns = Footprint(view.CentreX, view.HalfExtentM, layout);
  const auto sourceAxis = [&](double centre) {
    const double scale = layout.HalfExtentM * sourceSide / (layout.Pixels * view.HalfExtentM);
    const double left = layout.HalfExtentM + centre;
    return SourceAxis{.Scale = scale,
                      .Offset = (1.0 - left / view.HalfExtentM) * (sourceSide * 0.5) + 0.5 * scale,
                      .Limit = static_cast<double>(sourceSide)};
  };
  const auto row = sourceAxis(view.CentreY);
  const auto column = sourceAxis(view.CentreX);
  for (int y = rows.Begin; y < rows.End; ++y) {
    const int v = row.At(y);
    if (v < 0) { continue; }
    for (int x = columns.Begin; x < columns.End; ++x) {
      const int u = column.At(x);
      if (u < 0) { continue; }
      const size_t from =
          static_cast<size_t>(v) * static_cast<size_t>(sourceSide) + static_cast<size_t>(u);
      const auto &sample = source.Texels[from];
      if (sample.Surface == 0) { continue; }
      const size_t to =
          static_cast<size_t>(y) * static_cast<size_t>(layout.Pixels) + static_cast<size_t>(x);
      const double distance = (4.0 * sample.Depth - 2.0) * view.HalfExtentM + view.CentreZ;
      const auto depth = static_cast<float>(0.5 + 0.25 * distance / layout.HalfExtentM);
      if (depth <= target.Texels[to].Depth) { continue; }
      target.Texels[to] = {.Normal = sample.Normal, .Depth = depth, .Surface = sample.Surface};
      if (!target.Materials.empty()) { target.Materials[to] = source.Materials[from]; }
    }
  }
}

bool ValidPlacement(const GroupImpostorInstance &instance,
                    const GroupImpostorLayout &layout,
                    const ImpostorAtlas &prototype) {
  if (instance.Scale <= 0 || !std::isfinite(instance.Scale)) { return false; }
  const auto offset = instance.RootM + prototype.CentreM() * instance.Scale - layout.CentreM;
  return Length(offset) <= 2.0 * layout.HalfExtentM &&
         instance.Scale * prototype.HalfExtentM() <= layout.HalfExtentM;
}

std::optional<ImpostorAtlas::View> ComposeView(const ImpostorAtlas &prototype,
                                               const ImpostorAtlas::View &source,
                                               const GroupImpostorLayout &layout) {
  const auto frame = ImpostorViewFrame::From(source.TowardEye);
  if (!frame) { return std::nullopt; }
  const auto side = static_cast<size_t>(layout.Pixels);
  ImpostorAtlas::View target{.TowardEye = source.TowardEye,
                             .Texels = std::vector<ImpostorAtlas::Texel>(side * side),
                             .Materials = {}};
  if (!source.Materials.empty()) { target.Materials.resize(side * side); }
  for (const auto &instance : layout.Instances) {
    const auto offset = instance.RootM + prototype.CentreM() * instance.Scale - layout.CentreM;
    RasterInstance(target,
                   layout,
                   source,
                   prototype.Pixels(),
                   {.CentreX = Dot(offset, frame->Right),
                    .CentreY = -Dot(offset, frame->Up),
                    .CentreZ = Dot(offset, source.TowardEye),
                    .HalfExtentM = prototype.HalfExtentM() * instance.Scale});
  }
  return target;
}

}

std::optional<ImpostorAtlas> ComposeGroupImpostor(const ImpostorAtlas &prototype,
                                                  const GroupImpostorLayout &layout,
                                                  std::string &error) {
  constexpr int kMaximumSide = 512;
  if (layout.Instances.empty() || layout.Pixels < 3 || layout.Pixels > kMaximumSide ||
      layout.HalfExtentM <= 0 || !std::isfinite(layout.HalfExtentM) ||
      !std::ranges::all_of(layout.Instances, [&](const GroupImpostorInstance &instance) {
        return ValidPlacement(instance, layout, prototype);
      })) {
    error = "invalid group composition layout";
    return std::nullopt;
  }
  std::vector<ImpostorAtlas::View> composed;
  composed.reserve(prototype.Views().size());
  for (const auto &view : prototype.Views()) {
    auto projected = ComposeView(prototype, view, layout);
    if (!projected) {
      error = "invalid group composition view";
      return std::nullopt;
    }
    composed.push_back(std::move(*projected));
  }
  return ImpostorAtlas::Create(layout.Pixels,
                               layout.CentreM,
                               layout.HalfExtentM,
                               prototype.Surfaces(),
                               std::move(composed),
                               error);
}
}
