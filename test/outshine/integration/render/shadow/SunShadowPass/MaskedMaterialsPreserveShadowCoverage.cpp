#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "Geometry.h"
#include "Texture.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <expected>
#include <string>
#include <utility>
#include <vector>

namespace {
bool CastAt(const std::vector<float> &atlas, int x) {
  using namespace outshine::Render;
  if (atlas.size() != static_cast<size_t>(kShadowAtlasPx) * kShadowAtlasPx) { return false; }
  const int column = kShadowTilePx / 2 - x * kShadowTilePx / 512;
  return atlas[static_cast<size_t>(kShadowTilePx / 2) * kShadowAtlasPx +
               static_cast<size_t>(column)] > 0;
}

enum class MipMask {
  Split,
  OpaqueLower,
  TransparentLower,
  OpaqueLowerDisabled,
  TransparentLowerDisabled
};

struct MaskCase {
  int uvSet;
  float vertexAlpha;
  bool textured;
  bool reverse = false;
  bool instanced = false;
  MipMask mips = MipMask::Split;
};

bool MipsEnabled(MipMask mask) {
  return mask != MipMask::OpaqueLowerDisabled && mask != MipMask::TransparentLowerDisabled;
}

bool LowerOpaque(MipMask mask) {
  return mask == MipMask::OpaqueLower || mask == MipMask::OpaqueLowerDisabled;
}

std::expected<int, outshine::GeometryImageError> MaskImage(outshine::Geometry &geometry,
                                                           MipMask mask) {
  using namespace outshine;
  if (mask == MipMask::Split) {
    return geometry.addImage(2, 1, std::array<uint8_t, 8>{255, 255, 255, 255, 255, 255, 255, 0});
  }
  constexpr int dimension = 2048;
  const bool opaque = LowerOpaque(mask);
  std::vector<uint8_t> base(size_t{dimension} * dimension * 4, 255);
  for (size_t at = 3; at < base.size(); at += 4) { base[at] = opaque ? 0 : 255; }
  std::vector<uint8_t> lower;
  for (int side = dimension / 2; side > 0; side /= 2) {
    const size_t first = lower.size();
    lower.resize(first + static_cast<size_t>(side) * static_cast<size_t>(side) * 4, 255);
    for (size_t at = first + 3; at < lower.size(); at += 4) { lower[at] = opaque ? 255 : 0; }
  }
  ImageMipData prepared;
  prepared[static_cast<size_t>(ImageMipKind::Colour)] = std::move(lower);
  return geometry.addImage(ImageView{.WidthPx = dimension,
                                     .HeightPx = dimension,
                                     .Rgba = base,
                                     .LowerMips = ViewImageMips(prepared)});
}

void CheckCoverage(const std::vector<float> &atlas, MaskCase mask, bool covered) {
  const bool split = mask.mips == MipMask::Split;
  const bool left = (mask.uvSet == 0) != mask.reverse;
  const bool prepared = MipsEnabled(mask.mips) ? LowerOpaque(mask.mips) : !LowerOpaque(mask.mips);
  CHECK(CastAt(atlas, -40) == (covered && (!mask.textured || (split ? left : prepared))),
        "left shadow matches source alpha, selected UV and prepared mip coverage");
  CHECK(CastAt(atlas, 40) == (covered && (!mask.textured || (split ? !left : prepared))),
        "right shadow matches source alpha, selected UV and prepared mip coverage");
}

void CheckMask(MaskCase mask) {
  const auto [uvSet, vertexAlpha, textured, reverse, instanced, mips] = mask;
  constexpr float kMaterialAlpha = .75f;
  constexpr float kKeyLux = 10000;
  constexpr double kOwnerOffsetM = 10000;
  constexpr double kEyeHeightM = 100;
  constexpr double kFarM = 20000;
  constexpr double kNearM = .1;
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  Geometry geometry;
  Material material;
  material.Alpha = AlphaMode::Masked;
  material.BaseColour[3] = kMaterialAlpha;
  material.DoubleSided = true;
  if (textured) {
    const auto image = MaskImage(geometry, mips);
    CHECK(image.has_value(), "mask image exists");
    if (!image) { return; }
    material.BaseColourMap.Image = *image;
    material.BaseColourMap.Set = uvSet == 0 ? UvSet::Uv0 : UvSet::Uv1;
    if (mips != MipMask::Split) {
      material.BaseColourMap.Sampler.Mip = MipsEnabled(mips) ? MipFilter::Linear : MipFilter::None;
    }
    if (reverse) {
      material.BaseColourMap.Uv.ScaleUv = {{-1, 1}};
      material.BaseColourMap.Uv.OffsetUv = {{1, 0}};
    }
  }
  const auto surface = geometry.addSurface("masked caster", material);
  CHECK(surface.has_value(), "masked material exists");
  if (!surface) { return; }
  const auto part = geometry.addPart("card", *surface);
  CHECK(part.has_value(), "masked card exists");
  if (!part) { return; }
  constexpr std::array<float, 12> positions{-80, -80, 0, 80, -80, 0, 80, 80, 0, -80, 80, 0};
  constexpr std::array<uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
  CHECK(geometry.setPositions(*part, positions) && geometry.setTriangles(*part, indices),
        "card topology exists");
  if (textured) {
    CHECK(geometry.setTexture(*part, std::array<float, 8>{0, 0, 1, 0, 1, 1, 0, 1}), "UV0");
    if (uvSet == 1) {
      CHECK(geometry.setTexture(*part, std::array<float, 8>{1, 0, 0, 0, 0, 1, 1, 1}, 1),
            "UV1 reverses coverage");
    }
  }
  if (vertexAlpha < 1) {
    const std::array<float, 16> colours{
        1, 1, 1, vertexAlpha, 1, 1, 1, vertexAlpha, 1, 1, 1, vertexAlpha, 1, 1, 1, vertexAlpha};
    CHECK(geometry.setColours(*part, colours), "vertex coverage exists");
  }
  if (instanced) {
    Mat4 far;
    far[12] = kOwnerOffsetM;
    CHECK(geometry.setPlacement(*part, far).has_value(), "material owner is outside near tile");
  }
  Core::Declaration declaration;
  declaration.InitialGeometry = &geometry;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"surface", "sceneLinear", "shadowAtlas"};
  declaration.DrawsSky = true;
  declaration.ShadowRadiusM = 16384;
  declaration.KeyLux = kKeyLux;
  SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        "masked shadow world opens");
  if (!scene) { return; }
  Viewpoint eye;
  eye.EyeM = {{0, 0, kEyeHeightM}};
  eye.YfovRad = 1;
  eye.ZNearM = kNearM;
  eye.ZFarM = kFarM;
  scene->Eye(eye);
  renderer.SetShadowFrame({{0, 0, 1}}, {{0, 1, 0}}, 16384, true, 0);
  if (instanced) {
    std::array<StoredVertex, 4> vertices{};
    constexpr std::array<float, 8> uv{0, 0, 1, 0, 1, 1, 0, 1};
    for (size_t at = 0; at < vertices.size(); ++at) {
      vertices[at] =
          StoredVertex::Of({{positions[at * 3], positions[at * 3 + 1], positions[at * 3 + 2]}},
                           {{uv[at * 2], uv[at * 2 + 1]}},
                           {{0, 0, 1}});
    }
    const std::array<Mat4, 1> rows{};
    PieceMesh piece;
    piece.Verts = vertices;
    piece.Indices = indices;
    piece.Instances = rows;
    piece.Textured = true;
    CHECK(renderer.PlacePiece(piece, error) != kNoPiece, "masked instance is resident");
  }
  CHECK(scene->Draw(error), error.empty() ? "masked shadow draws" : error.c_str());
  renderer.WaitForGpu();
  std::vector<float> atlas;
  CHECK(renderer.ReadShadowAtlas(atlas) == ReadState::Ready, "shadow atlas reads back");
  const bool covered = kMaterialAlpha * vertexAlpha >= material.CoverageCut;
  CheckCoverage(atlas, mask, covered);
}
}

int main() {
  using namespace outshine::Test;
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "video initializes for masked shadows");
  if (!initialized) {
    Note(SDL_GetError());
    return Report();
  }
  for (const int uv : {0, 1}) {
    CheckMask({.uvSet = uv, .vertexAlpha = 1, .textured = true});
    CheckMask({.uvSet = uv, .vertexAlpha = .5f, .textured = true});
  }
  CheckMask({.uvSet = 0, .vertexAlpha = 1, .textured = false});
  CheckMask({.uvSet = 0, .vertexAlpha = .5f, .textured = false});
  CheckMask({.uvSet = 0, .vertexAlpha = 1, .textured = true, .reverse = true});
  CheckMask({.uvSet = 1, .vertexAlpha = 1, .textured = true, .reverse = true});
  CheckMask({.uvSet = 0, .vertexAlpha = 1, .textured = true, .reverse = false, .instanced = true});
  CheckMask({.uvSet = 0, .vertexAlpha = 1, .textured = true, .reverse = true, .instanced = true});
  for (const auto mips : {MipMask::OpaqueLower,
                          MipMask::TransparentLower,
                          MipMask::OpaqueLowerDisabled,
                          MipMask::TransparentLowerDisabled}) {
    CheckMask({.uvSet = 0, .vertexAlpha = 1, .textured = true, .mips = mips});
  }
  SDL_Quit();
  return Report();
}
