#ifndef OUTSHINE_RENDER_STAGES_SUBJECTTYPES_H
#define OUTSHINE_RENDER_STAGES_SUBJECTTYPES_H

#include <cstdint>

#include "PunctualLight.h"
#include "UvTransform.h"

#include "DrawList.h"

namespace outshine::Render {

struct SubjectLight {
  outshine::PunctualLight Light;
  double PositionEcefM[3] = {0, 0, 0};
};

struct SubjectEnvironment {
  double RadianceLinear[3] = {0, 0, 0};
};

constexpr size_t kMaxSubjectLights = 16;

constexpr uint32_t kSubjectMaterialImages = 6;
constexpr uint32_t kSubjectImages = kSubjectMaterialImages + 1;
constexpr uint32_t kSubjectFragmentUniforms = 2;
constexpr uint32_t kSubjectStorageBuffers = 2;

enum class SubjectWrap { ClampToEdge, MirroredRepeat, Repeat };
enum class SubjectFilter { Nearest, Linear };

enum class SubjectMip { None, Nearest, Linear };

struct SubjectTexture {
  const uint8_t *Rgba = nullptr;
  uint32_t Width = 0;
  uint32_t Height = 0;
  SubjectWrap WrapU = SubjectWrap::Repeat;
  SubjectWrap WrapV = SubjectWrap::Repeat;
  SubjectFilter Magnify = SubjectFilter::Linear;

  SubjectFilter Minify = SubjectFilter::Linear;
  SubjectMip Mip = SubjectMip::Linear;

  outshine::UvTransform Uv;

  outshine::UvSet Set = outshine::UvSet::First;
};

struct SubjectMaterial {

  Material Row;
  SubjectTexture Colour;

  SubjectTexture SpecularStrength;
  SubjectTexture SpecularTint;

  SubjectTexture Normal;
  SubjectTexture MetalRough;
  SubjectTexture Emissive;

  float NormalScale = 1.0f;

  [[nodiscard]] SurfaceState State() const { return StateOf(Row); }
  [[nodiscard]] float Coverage() const { return Row.BaseColour[3]; }

  [[nodiscard]] bool ReadsSecondUv() const {
    return Colour.Set == outshine::UvSet::Second || Normal.Set == outshine::UvSet::Second ||
           MetalRough.Set == outshine::UvSet::Second || Emissive.Set == outshine::UvSet::Second ||
           SpecularStrength.Set == outshine::UvSet::Second ||
           SpecularTint.Set == outshine::UvSet::Second;
  }

  [[nodiscard]] bool ReadsAnyImage() const {
    return Colour.Rgba || Normal.Rgba || MetalRough.Rgba || Emissive.Rgba ||
           SpecularStrength.Rgba || SpecularTint.Rgba;
  }
};

struct SubjectPose {
  const float *Verts = nullptr;
  const float *Uv = nullptr;

  const float *Uv1 = nullptr;
  const float *Normals = nullptr;

  const float *Tangents = nullptr;

  const float *Colours = nullptr;
  const float *Emitted = nullptr;

  const float *PrevVerts = nullptr;
  uint32_t VertexCount = 0;
  double Anchor[3] = {0, 0, 0};
  double PrevAnchor[3] = {0, 0, 0};
  double Model[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

struct SubjectMesh : SubjectPose {
  const uint32_t *Indices = nullptr;
  uint32_t IndexCount = 0;
  const DrawList *Draws = nullptr;
};

}

#endif
