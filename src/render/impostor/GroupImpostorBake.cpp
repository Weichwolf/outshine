#include "GroupImpostorBake.h"
#include "DepthSurfacePatches.h"
#include "GroupImpostorView.h"
#include "ImpostorAtlas.h"
#include "Material.h"
#include "Vec3.h"
#include "math/Octahedral.h"
#include "math/Srgb.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {

struct Footprint {
  Vec3f Albedo;
  Vec3f Normal;
  float Roughness = 0;
  float Metalness = 0;
  float Depth = 0;
  uint32_t Covered = 0;

  void Add(const Footprint &other) {
    Albedo = Albedo + other.Albedo;
    Normal = Normal + other.Normal;
    Roughness += other.Roughness;
    Metalness += other.Metalness;
    Depth = std::max(Depth, other.Depth);
    Covered += other.Covered;
  }
};

Footprint CapturedSample(const ImpostorAtlas &atlas, const ImpostorAtlas::View &view, size_t at) {
  const auto &texel = view.Texels[at];
  if (texel.Surface == 0) { return {}; }
  const Material &material = atlas.Surfaces()[texel.Surface - 1];
  const auto sample = view.Materials.empty()
                          ? ImpostorAtlas::SurfaceSample{.BaseColour = {{material.BaseColour[0],
                                                                         material.BaseColour[1],
                                                                         material.BaseColour[2]}},
                                                         .Roughness = material.Roughness,
                                                         .Metalness = material.Metalness}
                          : view.Materials[at];
  return {.Albedo = sample.BaseColour,
          .Normal = texel.Normal,
          .Roughness = sample.Roughness,
          .Metalness = sample.Metalness,
          .Depth = texel.Depth,
          .Covered = 1};
}

std::vector<Footprint> BaseSamples(const ImpostorAtlas &atlas,
                                   const ImpostorAtlas::View &view,
                                   uint32_t side,
                                   uint32_t sourceSide) {
  const uint32_t factor = sourceSide / side;
  std::vector<Footprint> cells(static_cast<size_t>(side) * side);
  for (uint32_t y = 0; y < sourceSide; ++y) {
    for (uint32_t x = 0; x < sourceSide; ++x) {
      cells[static_cast<size_t>(y / factor) * side + x / factor].Add(
          CapturedSample(atlas, view, static_cast<size_t>(y) * sourceSide + x));
    }
  }
  return cells;
}

bool BakePatches(GroupImpostorView &view, const std::vector<Footprint> &cells) {
  std::vector<float> depth(cells.size());
  std::vector<uint32_t> surface(cells.size());
  for (size_t at = 0; at < cells.size(); ++at) {
    depth[at] = cells[at].Depth;
    surface[at] = cells[at].Covered;
  }
  const auto source = BuildDepthSurfacePatches({.Width = view.Side,
                                                .Height = view.Side,
                                                .Depth = depth,
                                                .Surface = surface,
                                                .AllowedError = 0.25 / view.Side,
                                                .MaximumSlope = 1.0 / view.Side});
  if (!source) { return false; }
  view.Patches.reserve(source->size());
  for (const auto &patch : *source) {
    view.Patches.push_back({.Left = static_cast<uint16_t>(patch.Left),
                            .Top = static_cast<uint16_t>(patch.Top),
                            .Right = static_cast<uint16_t>(patch.Right),
                            .Bottom = static_cast<uint16_t>(patch.Bottom),
                            .Depth = static_cast<float>(patch.At(patch.Left, patch.Top)),
                            .SlopeX = static_cast<float>(patch.SlopeX),
                            .SlopeY = static_cast<float>(patch.SlopeY)});
  }
  return true;
}

uint8_t Quantise(float value) {
  constexpr float highest = std::numeric_limits<uint8_t>::max();
  return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * highest));
}

void AppendMip(GroupImpostorView &view, const std::vector<Footprint> &cells, float capacity) {
  for (const auto &pixel : cells) {
    const auto toColour = [capacity](float channel) {
      return Quantise(ColourSpace::SrgbFromLinear(channel / capacity));
    };
    view.AlbedoCoverage.push_back({toColour(pixel.Albedo[0]),
                                   toColour(pixel.Albedo[1]),
                                   toColour(pixel.Albedo[2]),
                                   Quantise(static_cast<float>(pixel.Covered) / capacity)});
    auto normal = pixel.Normal;
    if (!Normalise(normal)) { normal = {{0, 1, 0}}; }
    const auto octahedral = OctFolded(normal.Axis);
    const auto count = static_cast<float>(std::max(1u, pixel.Covered));
    view.NormalMaterial.push_back({Quantise(0.5f + 0.5f * octahedral[0]),
                                   Quantise(0.5f + 0.5f * octahedral[1]),
                                   Quantise(pixel.Roughness / count),
                                   Quantise(pixel.Metalness / count)});
  }
}

void BakeMips(GroupImpostorView &view, std::vector<Footprint> cells, uint32_t sourceSide) {
  const size_t texels = GroupImpostorView::MipTexels(view.Side);
  view.AlbedoCoverage.reserve(texels);
  view.NormalMaterial.reserve(texels);
  for (size_t side = view.Side;; side /= 2) {
    const auto stride = static_cast<float>(sourceSide) / static_cast<float>(side);
    AppendMip(view, cells, stride * stride);
    if (side == 1) { break; }
    const size_t nextSide = side / 2;
    for (size_t y = 0; y < nextSide; ++y) {
      for (size_t x = 0; x < nextSide; ++x) {
        const size_t from = 2 * (y * side + x);
        auto combined = cells[from];
        combined.Add(cells[from + 1]);
        combined.Add(cells[from + side]);
        combined.Add(cells[from + side + 1]);
        cells[y * nextSide + x] = combined;
      }
    }
    cells.resize(nextSide * nextSide);
  }
}

}

std::optional<GroupImpostorView>
BakeGroupImpostorView(const ImpostorAtlas &atlas, size_t view, uint32_t side) {
  if (view >= atlas.Views().size() || !GroupImpostorView::ValidSide(side)) { return std::nullopt; }
  const auto sourceSide = static_cast<uint32_t>(atlas.Pixels());
  if (side > sourceSide || sourceSide % side != 0) { return std::nullopt; }
  const auto &source = atlas.Views()[view];
  GroupImpostorView result{.TowardEye = source.TowardEye,
                           .Side = side,
                           .Patches = {},
                           .AlbedoCoverage = {},
                           .NormalMaterial = {}};
  auto cells = BaseSamples(atlas, source, side, sourceSide);
  if (!BakePatches(result, cells)) { return std::nullopt; }
  BakeMips(result, std::move(cells), sourceSide);
  return result;
}

}
