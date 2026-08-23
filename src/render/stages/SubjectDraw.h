#ifndef SUBJECTDRAW_H
#define SUBJECTDRAW_H

#include <span>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "SurfaceState.h"
#include "TriangleBvh.h"

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"

#include "KernelShape.h"
#include "SubjectResidency.h"
#include "SubjectTypes.h"

namespace outshine::Render {

class SubjectDraw {
public:
  struct SourceOptions {
    bool WritesVelocity = false;
    long NormalIndex = -1;
    long IdentityIndex = -1;
  };
  [[nodiscard]] static std::string ShaderSource(const SourceOptions &options);
  [[nodiscard]] static std::string ShaderSource(const SourceOptions &options, std::string &error);
  [[nodiscard]] static std::string DepthOnlySource();
  [[nodiscard]] static std::string DepthOnlySource(std::string &error);
  [[nodiscard]] static const char *VertexEntry(VertexLayout layout);
  [[nodiscard]] static const char *FragmentEntry(SurfaceKind kind, VertexLayout layout);
  static constexpr DrawShape ShaderShape{.VertexUniformBuffers = 1,
                                         .FragmentSamplers = kSubjectImages,
                                         .FragmentUniformBuffers = kSubjectFragmentUniforms,
                                         .FragmentStorageBuffers = kSubjectStorageBuffers};
  static constexpr DrawShape DepthOnlyShape{.VertexUniformBuffers = 1};

  [[nodiscard]] bool Configure(const Gpu &gpu, std::string &error);

  void SeeThroughTo(SDL_GPUTexture *behind, SDL_GPUSampler *exact) {
    Behind = behind;
    BehindSampler = exact;
  }

  void GlassIsDrawnElsewhere() { GlassDrawnElsewhere_ = true; }

  [[nodiscard]] bool SetPlacements(const double *models, size_t rows, std::string &error) {
    if (models == nullptr || rows == 0) {
      Placed_.clear();
      return true;
    }
    Placed_.assign(models, models + rows * 16u);
    for (const DrawBatch &batch : Batches) {
      if ((size_t)batch.ModelSlot < rows) { continue; }
      error = "a draw names placement slot " + std::to_string(batch.ModelSlot) +
              " over a table of " + std::to_string(rows) + " placements";
      Placed_.clear();
      return false;
    }
    return true;
  }

  [[nodiscard]] bool SetMaterials(std::span<const SubjectMaterial> materials,
                                  std::string &error);

  [[nodiscard]] bool SetMesh(const SubjectMesh &mesh, std::string &error);

  [[nodiscard]] bool SetPose(const SubjectPose &pose, std::string &error);

private:
  [[nodiscard]] bool HandVisibility(bool deferred, std::string &error);
  [[nodiscard]] bool HandStreams(const SubjectPose &pose, bool deferred, std::string &error);

  TriangleBvh Visibility_;

public:

  [[nodiscard]] bool SetLights(std::span<const SubjectLight> lights, std::string &error);

  void SetEnvironment(const SubjectEnvironment &environment) { Environment = environment; }

  void Encode(const FrameContext &ctx, const PassRecording &into);

  [[nodiscard]] bool ConfigureDepthOnly(const Gpu &gpu, std::string &error);
  void EncodeDepthOnly(const double lightFromWorld16[16], const double eye[3], int atlasPx,
                       const PassRecording &into);

  uint32_t VertexCount() const { return Resident_.NVerts; }
  long TriangleCount() const { return (long)Resident_.NIdx / 3; }

  uint32_t BatchCount() const { return (uint32_t)Batches.size(); }
  uint32_t DrawCount() const;

  uint32_t PipelineCount() const { return Built; }

  float ShadowNearM() const { return ShadowNearM_; }

private:
  static constexpr int kUniFloats = 56;

  static constexpr int kSurfaceScalars = 35;

  static constexpr int kUvMatrixFloats = 6;

  static constexpr int kUvSetFloats = 1;
  static constexpr int kSurfaceFloats =
      kSurfaceScalars + (kUvMatrixFloats + kUvSetFloats) * (int)kSubjectMaterialImages;

  static constexpr int kLightVec4s = 4;

  static constexpr int kLightFloats = 8 + 4 * kLightVec4s * (int)kMaxSubjectLights;

  struct SurfaceSlot {
    SubjectResidency::BoundImage Colour;
    SubjectResidency::BoundImage Normal;
    SubjectResidency::BoundImage MetalRough;
    SubjectResidency::BoundImage Emissive;

    SubjectResidency::BoundImage SpecularStrength;
    SubjectResidency::BoundImage SpecularTint;

    std::array<float, kSurfaceFloats> Row{};
    SurfaceKind Kind = SurfaceKind::Opaque;
    bool CullsBack = true;

    bool ReadsSecondUv = false;
  };

  SDL_GPUTexture *Behind = nullptr;
  SDL_GPUSampler *BehindSampler = nullptr;

  bool GlassDrawnElsewhere_ = false;

  void BindSurface(const SubjectMaterial &material);

public:
  void FlushCrossings(SDL_GPUCommandBuffer *commands) { Resident_.FlushCrossings(commands); }

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
  SubjectResidency Resident_;

  OwnedPipeline DepthOnly_;

  float ShadowNearM_ = 0.0f;
  std::vector<SubjectLight> Placed;
  SubjectEnvironment Environment;
  double Anchor[3] = {0, 0, 0};
  double PrevAnchor[3] = {0, 0, 0};
  double Model[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  std::vector<double> Placed_;

  bool WritesVelocity = false;
};

}
#endif
