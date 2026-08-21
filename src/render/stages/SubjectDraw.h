#ifndef SUBJECTDRAW_H
#define SUBJECTDRAW_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "TexelChain.h"
#include "TriangleBvh.h"
#include "PunctualLight.h"
#include "UvTransform.h"

#include "DrawList.h"
#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"

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

class SubjectDraw {
public:

  [[nodiscard]] bool Configure(const Gpu &gpu, std::string &error);

  void SeeThroughTo(SDL_GPUTexture *behind, SDL_GPUSampler *exact) {
    Behind = behind;
    BehindSampler = exact;
  }

  void GlassIsDrawnElsewhere() { GlassDrawnElsewhere_ = true; }

  [[nodiscard]] bool SetMaterials(const std::vector<SubjectMaterial> &materials,
                                  std::string &error);

  [[nodiscard]] bool SetMesh(const SubjectMesh &mesh, std::string &error);

  [[nodiscard]] bool SetPose(const SubjectPose &pose, std::string &error);

private:
  [[nodiscard]] bool HandVisibility(bool deferred, std::string &error);
  [[nodiscard]] bool HandStreams(const SubjectPose &pose, bool deferred, std::string &error);

  TriangleBvh Visibility_;

public:

  [[nodiscard]] bool SetLights(const std::vector<SubjectLight> &lights, std::string &error);

  void SetEnvironment(const SubjectEnvironment &environment) { Environment = environment; }

  void Encode(const FrameContext &ctx, const PassRecording &into);

  uint32_t VertexCount() const { return NVerts; }
  long TriangleCount() const { return (long)NIdx / 3; }

  uint32_t BatchCount() const { return (uint32_t)Batches.size(); }
  uint32_t DrawCount() const;

  uint32_t PipelineCount() const { return Built; }

  float ShadowNearM() const { return ShadowNearM_; }

private:
  static constexpr int kUniFloats = 40;

  static constexpr int kSurfaceScalars = 35;

  static constexpr int kUvMatrixFloats = 6;

  static constexpr int kUvSetFloats = 1;
  static constexpr int kSurfaceFloats =
      kSurfaceScalars + (kUvMatrixFloats + kUvSetFloats) * (int)kSubjectMaterialImages;

  static constexpr int kLightVec4s = 4;

  static constexpr int kLightFloats = 8 + 4 * kLightVec4s * (int)kMaxSubjectLights;

  struct BoundImage {
    OwnedTexture Image;
    OwnedSampler Sample;
  };

  struct SurfaceSlot {
    BoundImage Colour;
    BoundImage Normal;
    BoundImage MetalRough;
    BoundImage Emissive;

    BoundImage SpecularStrength;
    BoundImage SpecularTint;

    std::array<float, kSurfaceFloats> Row{};
    SurfaceKind Kind = SurfaceKind::Opaque;
    bool CullsBack = true;

    bool ReadsSecondUv = false;
  };

  SDL_GPUTexture *Behind = nullptr;
  SDL_GPUSampler *BehindSampler = nullptr;

  bool GlassDrawnElsewhere_ = false;

  void BindSurface(const SubjectMaterial &material);

  enum class Transfer { Srgb, Linear };
  [[nodiscard]] BoundImage Upload(const SubjectTexture &texture, Transfer decode, TexelKind kind);

  [[nodiscard]] OwnedBuffer Fill(SDL_GPUBufferUsageFlags usage, const void *from, uint32_t bytes);

  struct Crossing {
    OwnedBuffer *Into = nullptr;
    uint32_t *Held = nullptr;
    SDL_GPUBufferUsageFlags Usage = 0;
    const void *From = nullptr;
    uint32_t Bytes = 0;
  };

  [[nodiscard]] bool Cross(Crossing *what, size_t count, bool deferred, std::string &error);
  [[nodiscard]] bool Submit(Crossing *what, size_t count, uint32_t total, std::string &error);

public:
  void FlushCrossings(SDL_GPUCommandBuffer *commands);

private:

  [[nodiscard]] std::array<float, kLightFloats> PackedLights(const FrameContext &ctx) const;

  [[nodiscard]] static size_t PipelineAt(VertexLayout layout, SurfaceKind kind, bool cullsBack);

  static constexpr size_t kSurfaceKinds = 5;

  static constexpr size_t kVertexLayoutCount = kVertexLayouts.size();
  static constexpr size_t kPipelines = kVertexLayoutCount * 2 * kSurfaceKinds;

  SDL_GPUDevice *Device = nullptr;
  std::array<OwnedPipeline, kPipelines> Pipelines;
  uint32_t Built = 0;

  std::vector<SurfaceSlot> Slots;
  std::vector<DrawBatch> Batches;

  std::vector<VertexLayout> BatchLayout;

  std::vector<Resource> Colours;
  OwnedBuffer Vtx, Uv, Uv1, Nrm, Tan, Col, Emit, Idx, Prev;

  enum class Stream : uint8_t {
    Vertex, Emitted, Normal, Tangent, Uv, Uv1, Colour, Previous, BvhNodes, BvhTriangles, Count
  };
  std::array<uint32_t, (size_t)Stream::Count> Held_{};

  static constexpr size_t kStagingRing = 3;
  std::array<OwnedTransfer, kStagingRing> Staging_{};
  uint32_t StagingBytes_ = 0;
  uint32_t StagingUsed_ = 0;
  size_t StagingAt_ = 0;
  struct Staged {
    SDL_GPUBuffer *Into = nullptr;
    uint32_t From = 0;
    uint32_t Bytes = 0;
    SDL_GPUTransferBuffer *Staging = nullptr;
  };
  static constexpr size_t kStagedCrossings = 32;
  std::array<Staged, kStagedCrossings> Staged_{};
  size_t StagedCount_ = 0;

  OwnedBuffer BvhNodes, BvhTris;

  float ShadowNearM_ = 0.0f;
  std::vector<SubjectLight> Placed;
  SubjectEnvironment Environment;
  uint32_t NVerts = 0, NIdx = 0;
  bool HasUv = false;
  bool HasUv1 = false;
  bool HasNormal = false;
  bool HasTangent = false;
  bool HasColour = false;
  bool FiltersFloat32 = false;
  double Anchor[3] = {0, 0, 0};
  double PrevAnchor[3] = {0, 0, 0};
  double Model[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

  bool WritesVelocity = false;
};

}
#endif
