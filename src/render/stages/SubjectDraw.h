#ifndef OUTSHINE_RENDER_STAGES_SUBJECTDRAW_H
#define OUTSHINE_RENDER_STAGES_SUBJECTDRAW_H

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

  void ShadowedBy(SDL_GPUTexture *atlas, SDL_GPUSampler *exact, const double *lightFromWorld16) {
    Atlas_ = atlas;
    AtlasSampler_ = exact;
    Shadowed_ = atlas != nullptr && exact != nullptr && lightFromWorld16 != nullptr;
    if (!Shadowed_) { return; }
    for (int at = 0; at < 16; ++at) { LightFromWorld_[at] = lightFromWorld16[at]; }
  }

  void CastsNoShadow() { ShadowedBy(nullptr, nullptr, nullptr); }

  [[nodiscard]] bool PlacementRows(size_t rows, std::string &error) {
    for (const DrawBatch &batch : Batches) {
      if ((size_t)batch.ModelSlot < rows) { continue; }
      error = "a draw names placement slot " + std::to_string(batch.ModelSlot) +
              " over a table of " + std::to_string(rows) + " placements";
      Placed_.clear();
      return false;
    }
    Placed_.resize(rows * 16u, 0.0);
    return true;
  }

  void MovePlacement(size_t slot, const double model16[16]) {
    if ((slot + 1) * 16u > Placed_.size()) { return; }
    for (size_t at = 0; at < 16u; ++at) { Placed_[slot * 16u + at] = model16[at]; }
    ++Moved_;
  }

  [[nodiscard]] size_t PlacementsMoved() const { return Moved_; }

  [[nodiscard]] bool SetPlacements(const double *models, size_t rows, std::string &error) {
    if (models == nullptr && rows > 0) {
      Placed_.clear();
      error = "a placement table of " + std::to_string(rows) +
              " rows arrives with no rows to read -- a count without a table is a declaration "
              "that names placements it does not hand over";
      return false;
    }
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


  [[nodiscard]] const SubjectResidency &Resident() const { return Resident_; }
  [[nodiscard]] const std::vector<DrawBatch> &Drawn() const { return Batches; }
  [[nodiscard]] const std::vector<double> &Placements() const { return Placed_; }
  [[nodiscard]] const double *AnchorM() const { return Anchor; }
  [[nodiscard]] const double *ModelM() const { return Model; }

  uint32_t VertexCount() const { return Resident_.NVerts; }
  long TriangleCount() const { return (long)Resident_.NIdx / 3; }

  uint32_t BatchCount() const { return (uint32_t)Batches.size(); }
  uint32_t DrawCount() const;

  uint32_t PipelineCount() const { return Built; }

  float ShadowNearM() const { return ShadowNearM_; }

private:
  static constexpr int kUniFloats = 72;

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

  SDL_GPUTexture *Atlas_ = nullptr;
  SDL_GPUSampler *AtlasSampler_ = nullptr;
  double LightFromWorld_[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  bool Shadowed_ = false;
  size_t ShadowedFrames_ = 0;

public:
  [[nodiscard]] size_t ShadowedFrames() const { return ShadowedFrames_; }

private:
  OwnedPipeline DepthOnly_;

  float ShadowNearM_ = 0.0f;
  std::vector<SubjectLight> Placed;
  SubjectEnvironment Environment;
  double Anchor[3] = {0, 0, 0};
  double PrevAnchor[3] = {0, 0, 0};
  double Model[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  std::vector<double> Placed_;
  size_t Moved_ = 0;

  bool WritesVelocity = false;
};

}
#endif
