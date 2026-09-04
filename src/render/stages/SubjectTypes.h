#ifndef OUTSHINE_RENDER_STAGES_SUBJECTTYPES_H
#define OUTSHINE_RENDER_STAGES_SUBJECTTYPES_H

#include <array>
#include <cstdint>
#include "math/Mat4.h"
#include "ClusterDag.h"
#include "StoredVertex.h"
#include <span>

#include "scene/PunctualLight.h"
#include "scene/UvTransform.h"

#include "DrawList.h"
#include "math/Vec3.h"

namespace outshine::Render {

struct SubjectLight {
  outshine::PunctualLight Light;
  Vec3 PositionEcefM = {{0, 0, 0}};
};

struct SubjectEnvironment {
  Vec3 RadianceLinear = {{0, 0, 0}};
  Vec3 GroundLinear = {{0, 0, 0}};
  Vec3 UpUnit = {{0, 1, 0}};

  double SkyLux = 0.0;
  double CosSunZenith = 0.0;
};

constexpr size_t kMaxSubjectLights = 16;

constexpr uint32_t kSubjectMaterialImages = 6;
constexpr uint32_t kSubjectImages = kSubjectMaterialImages + 2;
constexpr uint32_t kSubjectFragmentUniforms = 2;

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

enum class SurfaceDomain : std::uint8_t { Subject, Ground };

inline constexpr std::size_t kSurfaceDomains = 2;

struct SubjectMaterial {
  Material Row;
  SurfaceDomain Domain = SurfaceDomain::Subject;
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
    return (Colour.Rgba != nullptr) || (Normal.Rgba != nullptr) || (MetalRough.Rgba != nullptr) ||
           (Emissive.Rgba != nullptr) || (SpecularStrength.Rgba != nullptr) ||
           (SpecularTint.Rgba != nullptr);
  }
};

struct SubjectStream {
  const float *From = nullptr;
  void (*Writes)(const void *carrying, float *into, uint32_t floats) = nullptr;
  const void *Carrying = nullptr;

  [[nodiscard]] bool Stands() const { return From != nullptr || Writes != nullptr; }
};

struct SubjectPose {
  SubjectStream Verts;

  std::span<const float> Positions;
  SubjectStream Uv;

  SubjectStream Uv1;
  SubjectStream Normals;

  SubjectStream Tangents;

  SubjectStream Colours;
  SubjectStream Emitted;

  SubjectStream PrevVerts;
  uint32_t VertexCount = 0;
  Vec3 Anchor = {{0, 0, 0}};
  Mat4 Model = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
};

struct PieceMesh {
  std::span<const StoredVertex> Verts;
  std::span<const uint32_t> Indices;
  std::span<const DagCluster> Clusters;
  std::span<const float> Colours;
  Mat4 Row = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  uint32_t Surface = 0;
  bool Textured = false;
};

using PieceId = uint32_t;
inline constexpr PieceId kNoPiece = ~0u;

struct GroundInstance {
  Mat4f Row = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  std::array<float, 8> Corners = {{}};
  float Page = 0.0f;
  float SagInv = 0.0f;
  float StepE = 0.0f;
  float StepN = 0.0f;
};

inline constexpr uint32_t kGroundInstanceFloats = 28;
static_assert(sizeof(GroundInstance) == kGroundInstanceFloats * sizeof(float),
              "an instance is the twenty-eight floats the lattice's vertex shader reads");

struct GroundTile {
  GroundInstance Instance;
  float LowM = 0.0f;
  float HighM = 0.0f;
};

using PageId = uint32_t;
inline constexpr PageId kNoPage = ~0u;
inline constexpr uint32_t kNoSlot = ~0u;

struct SubjectMesh : SubjectPose {
  const uint32_t *Indices = nullptr;
  uint32_t IndexCount = 0;
  const DrawList *Draws = nullptr;

  std::span<const DagCluster> Clusters;

  std::span<const float> ClusterSpheres;
};

} // namespace outshine::Render

#endif
