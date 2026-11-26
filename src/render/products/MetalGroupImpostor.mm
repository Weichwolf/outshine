#include "MetalGroupImpostor.h"
#include "GroupImpostorView.h"
#include "ImpostorViewFrame.h"
#include "MetalProductRenderer.h"
#include "Vec3.h"
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
static_assert(sizeof(GroupImpostorPatch) == kShaderPatchBytes &&
              offsetof(GroupImpostorPatch, Depth) == 8 && sizeof(GroupImpostorView::Texel) == 4 &&
              sizeof(GroupAxes) == 64);

GroupAxes PackAxes(const MetalImpostorGroup &group,
                   const GroupImpostorView &view,
                   const ImpostorViewFrame &basis) {
  const double half = group.HalfExtentM;
  const double alongCell = 2.0 * half / view.Side;
  const auto row = [](const Vec3 &point, float fourth = 0) {
    return std::array{static_cast<float>(point[0]),
                      static_cast<float>(point[1]),
                      static_cast<float>(point[2]),
                      fourth};
  };
  return {row(group.CentreM + (basis.Up - basis.Right - view.TowardEye * 2.0) * half,
              1.0f / static_cast<float>(view.Side)),
          row(basis.Right * alongCell),
          row(basis.Up * -alongCell),
          row(view.TowardEye * (4.0 * half))};
}

id<MTLTexture> UploadGroupTexture(id<MTLDevice> device,
                                  MTLPixelFormat format,
                                  uint32_t side,
                                  std::span<const GroupImpostorView::Texel> texels) {
  auto *const descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                                              width:side
                                                                             height:side
                                                                          mipmapped:YES];
  descriptor.storageMode = MTLStorageModeShared;
  const id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
  size_t offset = 0;
  for (size_t level = 0, dimension = side; dimension > 0; ++level, dimension /= 2) {
    [texture replaceRegion:MTLRegionMake2D(0, 0, dimension, dimension)
               mipmapLevel:level
                 withBytes:texels.data() + offset
               bytesPerRow:dimension * sizeof(GroupImpostorView::Texel)];
    offset += dimension * dimension;
  }
  return texture;
}

}

std::expected<GroupResources, std::string>
UploadMetalGroup(id<MTLDevice> device, const MetalImpostorGroup &group, size_t &uploaded) {
  if (group.HalfExtentM <= 0 || !std::isfinite(group.HalfExtentM) || group.Views.size() < 2) {
    return std::unexpected("invalid group impostor bound or direction count");
  }
  GroupResources result{.Views = {},
                        .WorldFromTile = group.WorldFromTile,
                        .CentreM = group.CentreM,
                        .Light = {},
                        .Handoff = group.Handoff};
  result.Views.reserve(group.Views.size());
  for (const auto &view : group.Views) {
    const auto basis = ImpostorViewFrame::From(view.TowardEye);
    if (!basis || !GroupImpostorView::ValidSide(view.Side)) {
      return std::unexpected("invalid group impostor view");
    }
    const size_t count = GroupImpostorView::MipTexels(view.Side);
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
        UploadGroupTexture(device, MTLPixelFormatRGBA8Unorm_sRGB, view.Side, view.AlbedoCoverage);
    const auto material =
        UploadGroupTexture(device, MTLPixelFormatRGBA8Unorm, view.Side, view.NormalMaterial);
    if (albedo == nil || material == nil || (!patches.empty() && buffer == nil)) {
      return std::unexpected("Metal group impostor upload failed");
    }
    result.Views.push_back({.TowardEye = view.TowardEye,
                            .Axes = PackAxes(group, view, *basis),
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

}

size_t EncodeMetalGroup(id<MTLRenderCommandEncoder> draw,
                        const GroupResources &group,
                        const Vec3 &localEye) {
  const auto views = VisibleGroupViews(group, localEye);
  size_t triangles = 0;
  for (const auto *view : views) {
    if (view == nullptr || view->Count == 0) { continue; }
    [draw setVertexBuffer:view->Patches offset:0 atIndex:0];
    [draw setVertexBytes:view->Axes.data() length:sizeof(view->Axes) atIndex:2];
    [draw setFragmentTexture:view->AlbedoCoverage atIndex:0];
    [draw setFragmentTexture:view->NormalMaterial atIndex:1];
    [draw drawPrimitives:MTLPrimitiveTypeTriangle
             vertexStart:0
             vertexCount:6
           instanceCount:view->Count];
    triangles += 2 * view->Count;
  }
  return triangles;
}

}
