#ifndef OUTSHINE_RENDER_PRODUCTS_METALGROUPIMPOSTOR_H
#define OUTSHINE_RENDER_PRODUCTS_METALGROUPIMPOSTOR_H

#include "Mat4.h"
#include "Vec3.h"
#import <Metal/Metal.h>
#include <array>
#include <cstddef>
#include <expected>
#include <string>
#include <vector>

namespace outshine::Render {

struct MetalImpostorGroup;
using GroupAxes = std::array<std::array<float, 4>, 4>;

struct GroupViewTextures {
  Vec3 TowardEye;
  GroupAxes Axes;
  id<MTLBuffer> Patches;
  id<MTLTexture> AlbedoCoverage;
  id<MTLTexture> NormalMaterial;
  size_t Count;
};

struct GroupResources {
  std::vector<GroupViewTextures> Views;
  Mat4 WorldFromTile;
  Vec3 CentreM;
  std::array<float, 16> Light;
};

[[nodiscard]] std::expected<GroupResources, std::string>
UploadMetalGroup(id<MTLDevice> device, const MetalImpostorGroup &group, size_t &uploaded);

[[nodiscard]] size_t EncodeMetalGroup(id<MTLRenderCommandEncoder> draw,
                                      const GroupResources &group,
                                      const Vec3 &localEye);

}
#endif
