#include "MetalGroupImpostor.h"
#include "Box.h"
#include "ClipBounds.h"
#include "GroupImpostorView.h"
#include "ImpostorViewFrame.h"
#include "MetalProductRenderer.h"
#include "Vec3.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string>

namespace outshine::Render {
namespace {

constexpr size_t kShaderPatchBytes = 20;
constexpr size_t kShaderAxesBytes = 80;

struct GroupCoverage {
  float DirectionBegin;
  float DirectionWeight;
  uint32_t Fractional;
};

static_assert(sizeof(GroupImpostorPatch) == kShaderPatchBytes &&
              offsetof(GroupImpostorPatch, Depth) == 8 && sizeof(GroupImpostorView::Texel) == 4 &&
              sizeof(GroupAxes) == kShaderAxesBytes);
static_assert(sizeof(GroupCoverage) == 12 && offsetof(GroupCoverage, Fractional) == 8);

GroupAxes PackAxes(const MetalImpostorGroup &group,
                   const GroupImpostorView &view,
                   const ImpostorViewFrame &basis) {
  const double half = group.HalfExtentM;
  const double alongCell = 2.0 * half / view.Side;
  const auto region = view.TextureRegion();
  const auto row = [](const Vec3 &point) {
    return std::array{static_cast<float>(point[0]),
                      static_cast<float>(point[1]),
                      static_cast<float>(point[2]),
                      0.0f};
  };
  return {row(group.CentreM + (basis.Up - basis.Right - view.TowardEye * 2.0) * half),
          row(basis.Right * alongCell),
          row(basis.Up * -alongCell),
          row(view.TowardEye * (4.0 * half)),
          std::array{static_cast<float>(region.X),
                     static_cast<float>(region.Y),
                     1.0f / static_cast<float>(region.Width),
                     1.0f / static_cast<float>(region.Height)}};
}

Box GroupViewBounds(const GroupImpostorView &view, const GroupAxes &axes) {
  if (view.Patches.empty()) { return {}; }
  double near = std::numeric_limits<double>::infinity();
  double far = -near;
  for (const auto &patch : view.Patches) {
    const double depth = patch.Depth;
    const double dx = static_cast<double>(patch.SlopeX) * (patch.Right - patch.Left);
    const double dy = static_cast<double>(patch.SlopeY) * (patch.Bottom - patch.Top);
    const auto error = FloatRoundingMargin(std::abs(depth) + std::abs(dx) + std::abs(dy));
    near = std::min(near, depth + std::min(0.0, dx) + std::min(0.0, dy) - error);
    far = std::max(far, depth + std::max(0.0, dx) + std::max(0.0, dy) + error);
  }
  Box bounds;
  for (size_t axis = 0; axis < 3; ++axis) {
    const double origin = axes[0][axis];
    const double dx = static_cast<double>(axes[1][axis]) * view.Side;
    const double dy = static_cast<double>(axes[2][axis]) * view.Side;
    const double first = axes[3][axis] * near;
    const double last = axes[3][axis] * far;
    const auto error = FloatRoundingMargin(std::abs(origin) + std::abs(dx) + std::abs(dy) +
                                           std::max(std::abs(first), std::abs(last)));
    bounds.Min[axis] =
        origin + std::min(0.0, dx) + std::min(0.0, dy) + std::min(first, last) - error;
    bounds.Max[axis] =
        origin + std::max(0.0, dx) + std::max(0.0, dy) + std::max(first, last) + error;
  }
  return bounds;
}

id<MTLTexture> UploadGroupTexture(id<MTLDevice> device,
                                  MTLPixelFormat format,
                                  const GroupImpostorTextureRegion &region,
                                  std::span<const GroupImpostorView::Texel> texels) {
  auto *const descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                                              width:region.Width
                                                                             height:region.Height
                                                                          mipmapped:NO];
  descriptor.mipmapLevelCount = region.Levels;
  descriptor.storageMode = MTLStorageModeShared;
  const id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
  size_t offset = 0;
  for (size_t level = 0; level < region.Levels; ++level) {
    const size_t width = region.Width >> level;
    const size_t height = region.Height >> level;
    [texture replaceRegion:MTLRegionMake2D(0, 0, width, height)
               mipmapLevel:level
                 withBytes:texels.data() + offset
               bytesPerRow:width * sizeof(GroupImpostorView::Texel)];
    offset += width * height;
  }
  return texture;
}

}

std::expected<GroupResources, std::string>
UploadMetalGroup(id<MTLDevice> device, const MetalImpostorGroup &group, size_t &uploaded) {
  if (group.HalfExtentM <= 0 || !std::isfinite(group.HalfExtentM) || group.Views.size() < 2 ||
      group.Views.size() > GroupImpostorView::MaximumViews) {
    return std::unexpected("invalid group impostor bound or direction count");
  }
  GroupResources result{.Views = {},
                        .WorldFromTile = group.WorldFromTile,
                        .CentreM = group.CentreM,
                        .Bounds = {},
                        .Light = {},
                        .Handoff = group.Handoff,
                        .FractionalCoverage = group.FractionalCoverage};
  result.Views.reserve(group.Views.size());
  for (const auto &view : group.Views) {
    const auto basis = ImpostorViewFrame::From(view.TowardEye);
    const auto region = view.TextureRegion();
    if (!basis || !region.Valid(view.Side)) {
      return std::unexpected("invalid group impostor view");
    }
    const size_t count = region.Texels();
    if (view.AlbedoCoverage.size() != count || view.NormalMaterial.size() != count) {
      return std::unexpected("incomplete group impostor mip chain");
    }
    const auto patches = std::as_bytes(std::span(view.Patches));
    const id<MTLBuffer> buffer = patches.empty()
                                     ? nil
                                     : [device newBufferWithBytes:patches.data()
                                                           length:patches.size()
                                                          options:MTLResourceStorageModeShared];
    const auto albedo =
        UploadGroupTexture(device, MTLPixelFormatRGBA8Unorm_sRGB, region, view.AlbedoCoverage);
    const auto material =
        UploadGroupTexture(device, MTLPixelFormatRGBA8Unorm, region, view.NormalMaterial);
    if (albedo == nil || material == nil || (!patches.empty() && buffer == nil)) {
      return std::unexpected("Metal group impostor upload failed");
    }
    const auto axes = PackAxes(group, view, *basis);
    result.Bounds.Cover(GroupViewBounds(view, axes));
    result.Views.push_back({.TowardEye = view.TowardEye,
                            .Axes = axes,
                            .Patches = buffer,
                            .AlbedoCoverage = albedo,
                            .NormalMaterial = material,
                            .Count = view.Patches.size()});
    uploaded += patches.size() + 2 * count * sizeof(GroupImpostorView::Texel);
  }
  return result;
}

namespace {

std::array<const GroupViewTextures *, 2> VisibleGroupViews(const GroupResources &group,
                                                           const Vec3 &eye) {
  std::array<const GroupViewTextures *, 2> result{};
  std::array<double, 2> best{-std::numeric_limits<double>::infinity(),
                             -std::numeric_limits<double>::infinity()};
  const auto dx = eye - group.CentreM;
  for (const auto &view : group.Views) {
    const double score = Dot(dx, view.TowardEye);
    if (score > best[0]) {
      best[1] = best[0];
      result[1] = result[0];
      best[0] = score;
      result[0] = &view;
    } else if (score > best[1]) {
      best[1] = score;
      result[1] = &view;
    }
  }
  return result;
}

size_t EncodeGroupView(id<MTLRenderCommandEncoder> draw,
                       const GroupViewTextures &view,
                       GroupCoverage coverage) {
  if (view.Count == 0 || coverage.DirectionWeight <= 0) { return 0; }
  [draw setVertexBuffer:view.Patches offset:0 atIndex:0];
  [draw setVertexBytes:view.Axes.data() length:sizeof(view.Axes) atIndex:2];
  [draw setFragmentBytes:&coverage length:sizeof(coverage) atIndex:1];
  [draw setFragmentTexture:view.AlbedoCoverage atIndex:0];
  [draw setFragmentTexture:view.NormalMaterial atIndex:1];
  [draw drawPrimitives:MTLPrimitiveTypeTriangle
           vertexStart:0
           vertexCount:6
         instanceCount:view.Count];
  return 2 * view.Count;
}

size_t
EncodeCanopyViews(id<MTLRenderCommandEncoder> draw, const GroupResources &group, const Vec3 &eye) {
  auto towardEye = eye - group.CentreM;
  if (!Normalise(towardEye)) { towardEye = {{0, 1, 0}}; }
  std::array<double, GroupImpostorView::MaximumViews> scores;
  const auto active = std::span{scores}.first(group.Views.size());
  std::ranges::transform(group.Views, active.begin(), [&](const auto &view) {
    return Dot(towardEye, view.TowardEye);
  });
  const double cut = ImpostorDirectionCut(active);
  size_t triangles = 0;
  double prefix = 0;
  for (size_t index = 0; index < active.size(); ++index) {
    const auto weight = ImpostorDirectionWeight(active[index] - cut);
    const auto begin = static_cast<float>(prefix - std::floor(prefix));
    triangles +=
        EncodeGroupView(draw,
                        group.Views[index],
                        {.DirectionBegin = begin, .DirectionWeight = weight, .Fractional = 1});
    prefix += weight;
  }
  return triangles;
}

}

size_t EncodeMetalGroup(id<MTLRenderCommandEncoder> draw,
                        const GroupResources &group,
                        const Vec3 &localEye) {
  if (group.FractionalCoverage) { return EncodeCanopyViews(draw, group, localEye); }
  const auto views = VisibleGroupViews(group, localEye);
  size_t triangles = 0;
  for (const auto *view : views) {
    if (view != nullptr) {
      triangles += EncodeGroupView(
          draw, *view, {.DirectionBegin = 0, .DirectionWeight = 1, .Fractional = 0});
    }
  }
  return triangles;
}

}
