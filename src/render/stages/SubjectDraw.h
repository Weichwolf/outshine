#ifndef OUTSHINE_RENDER_STAGES_SUBJECTDRAW_H
#define OUTSHINE_RENDER_STAGES_SUBJECTDRAW_H

#include <span>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "math/Vec3.h"
#include "scene/SurfaceState.h"

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
  [[nodiscard]] static const char *
  FragmentEntry(SurfaceDomain domain, SurfaceKind kind, VertexLayout layout);
  static constexpr size_t kSubjectFragmentStorage = 3;

  static constexpr DrawShape ShaderShape{.VertexUniformBuffers = 1,
                                         .VertexStorageBuffers = 1,
                                         .FragmentSamplers = kSubjectImages,
                                         .FragmentUniformBuffers = kSubjectFragmentUniforms,
                                         .FragmentStorageBuffers = kSubjectFragmentStorage};
  static constexpr DrawShape DepthOnlyShape{.VertexUniformBuffers = 1, .VertexStorageBuffers = 1};

  [[nodiscard]] bool Configure(const Gpu &gpu, std::string &error);

  void SeeThroughTo(SDL_GPUTexture *behind, SDL_GPUSampler *exact) {
    Behind = behind;
    BehindSampler = exact;
  }

  void GlassIsDrawnElsewhere() { GlassDrawnElsewhere_ = true; }

  void ShadowedBy(SDL_GPUTexture *atlas, SDL_GPUSampler *exact, const double *lightFromWorld) {
    Atlas_ = atlas;
    AtlasSampler_ = exact;
    Shadowed_ = atlas != nullptr && exact != nullptr && lightFromWorld != nullptr;
    if (!Shadowed_) { return; }
    for (int at = 0; at < 16; ++at) { LightFromWorld_[at] = lightFromWorld[at]; }
  }

  void CastsNoShadow() { ShadowedBy(nullptr, nullptr, nullptr); }

  [[nodiscard]] bool HandPlacements(bool deferred, std::string &error);

  [[nodiscard]] bool HandDrawArguments(bool deferred, std::string &error);

  [[nodiscard]] uint32_t ClusterJobs() const { return Jobs_; }

  [[nodiscard]] uint32_t ClusterBatchRows() const {
    return Args_.empty() ? 0u : static_cast<uint32_t>(Batches.size());
  }

  [[nodiscard]] bool PlacementRows(size_t rows, std::string &error) {
    if (rows == 0) {
      Placed_.clear();
      Before_.clear();
      Stamped_.clear();
      return true;
    }
    for (const DrawBatch &batch : Batches) {
      if (static_cast<size_t>(batch.ModelSlot) + static_cast<size_t>(batch.Instances) <= rows) {
        continue;
      }
      error = "a draw names placement slot " + std::to_string(batch.ModelSlot) + " and " +
              std::to_string(batch.Instances) + " instance(s) over a table of " +
              std::to_string(rows) + " placements";
      Placed_.clear();
      return false;
    }
    if (Placed_.size() == rows * 16u) { return true; }
    Placed_.resize(rows * 16u, 0.0);
    Before_.assign(rows * 16u, 0.0);
    Stamped_.assign(rows, 0u);
    return true;
  }

  void MovePlacement(size_t slot, const double model[16]) {
    if ((slot + 1) * 16u > Placed_.size()) { return; }
    Before_.resize(Placed_.size(), 0.0);
    Stamped_.resize(Placed_.size() / 16u, 0u);
    if (Stamped_[slot] != Frame_) {
      const double *const carry = Stamped_[slot] == 0u ? model : Placed_.data() + slot * 16u;
      for (size_t at = 0; at < 16u; ++at) { Before_[slot * 16u + at] = carry[at]; }
      Stamped_[slot] = Frame_;
    }
    for (size_t at = 0; at < 16u; ++at) { Placed_[slot * 16u + at] = model[at]; }
    ++Moved_;
    RowsStale_ = true;
  }

  void CarryFrame() { ++Frame_; }

  [[nodiscard]] size_t PlacementsMoved() const { return Moved_; }

  [[nodiscard]] uint64_t Generation() const { return Moved_ + Reshaped_; }

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
      Before_.clear();
      Stamped_.clear();
      return true;
    }
    Placed_.assign(models, models + rows * 16u);
    Before_ = Placed_;
    Stamped_.assign(rows, Frame_);
    for (const DrawBatch &batch : Batches) {
      if (static_cast<size_t>(batch.ModelSlot) + static_cast<size_t>(batch.Instances) <= rows) {
        continue;
      }
      error = "a draw names placement slot " + std::to_string(batch.ModelSlot) + " and " +
              std::to_string(batch.Instances) + " instance(s) over a table of " +
              std::to_string(rows) + " placements";
      Placed_.clear();
      return false;
    }
    return true;
  }

  [[nodiscard]] bool SetMaterials(std::span<const SubjectMaterial> materials, std::string &error);

  [[nodiscard]] bool SetMesh(const SubjectMesh &mesh, std::string &error);

  [[nodiscard]] bool SetPose(const SubjectPose &pose, std::string &error);

private:
  [[nodiscard]] bool HandStreams(const SubjectPose &pose, bool deferred, std::string &error);
  [[nodiscard]] bool HandClusters(const SubjectMesh &mesh, std::string &error);

  [[nodiscard]] bool Room(OwnedBuffer &into,
                          SubjectResidency::Stream held,
                          SDL_GPUBufferUsageFlags usage,
                          uint32_t bytes) {
    if (into && Bound().Held[static_cast<size_t>(held)] >= bytes) { return true; }
    SDL_GPUBufferCreateInfo wanted{};
    wanted.usage = usage;
    wanted.size = Bound().Held[static_cast<size_t>(held)] > 0
                      ? Bound().Held[static_cast<size_t>(held)]
                      : bytes;
    while (wanted.size < bytes) { wanted.size *= 2u; }
    into = OwnedBuffer(Device, SDL_CreateGPUBuffer(Device, &wanted));
    Bound().Held[static_cast<size_t>(held)] = into ? wanted.size : 0u;
    return static_cast<bool>(into);
  }

public:
  [[nodiscard]] bool SetLights(std::span<const SubjectLight> lights, std::string &error);

  void SetEnvironment(const SubjectEnvironment &environment) { IndirectLight = environment; }

  void SkyFrom(SDL_GPUBuffer *irradiance) { SkyIrradiance_ = irradiance; }

  void GroundFrom(SDL_GPUBuffer *classes, SDL_GPUBuffer *palette) {
    GroundClasses_ = classes;
    GroundPalette_ = palette;
  }

  void Encode(const FrameContext &ctx, const PassRecording &into);

  [[nodiscard]] const SubjectResidency &Resident() const { return Bound(); }

  void Shares(SubjectResidency &other) { At_ = &other; }

  [[nodiscard]] SubjectResidency &Owned() { return Own_; }

  [[nodiscard]] bool Borrows() const { return At_ != nullptr; }

  [[nodiscard]] const std::vector<DrawBatch> &Drawn() const { return Batches; }

  [[nodiscard]] uint32_t ColourImages() const;
  [[nodiscard]] uint32_t DistinctPlacements() const;
  [[nodiscard]] uint32_t Layouts() const;
  [[nodiscard]] uint32_t Textured() const;

  [[nodiscard]] const std::vector<double> &Placements() const { return Placed_; }

  [[nodiscard]] const Vec3 &AnchorM() const { return Anchor; }

  [[nodiscard]] const double *ModelM() const { return Model; }

  [[nodiscard]] uint32_t HeldBytes() const { return At_ != nullptr ? 0u : Own_.HeldBytes(); }

  [[nodiscard]] uint32_t StagedBytes() const { return Bound().StagedBytes(); }

  void ForgetStagedCount() { Bound().ForgetStagedCount(); }

  [[nodiscard]] uint32_t VertexCount() const { return Bound().NVerts; }

  [[nodiscard]] long TriangleCount() const { return static_cast<long>(Bound().NIdx) / 3; }

  [[nodiscard]] uint32_t BatchCount() const { return static_cast<uint32_t>(Batches.size()); }

  [[nodiscard]] uint32_t DrawCount() const;

  [[nodiscard]] uint32_t PipelineCount() const { return Built; }

private:
  static constexpr int kUniFloats = 56;

  static constexpr int kSurfaceScalars = 35;

  static constexpr int kUvMatrixFloats = 6;

  static constexpr int kUvSetFloats = 1;
  static constexpr int kSurfaceFloats =
      kSurfaceScalars + (kUvMatrixFloats + kUvSetFloats) * static_cast<int>(kSubjectMaterialImages);

  static constexpr int kLightVec4s = 4;

  static constexpr int kLightFloats = 16 + 4 * kLightVec4s * static_cast<int>(kMaxSubjectLights);

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

    SurfaceDomain Domain = SurfaceDomain::Subject;
    bool ReadsSecondUv = false;
  };

  SDL_GPUTexture *Behind = nullptr;
  SDL_GPUSampler *BehindSampler = nullptr;

  bool GlassDrawnElsewhere_ = false;

  void BindSurface(const SubjectMaterial &material);

public:
  void FlushCrossings(SDL_GPUCommandBuffer *commands) { Bound().FlushCrossings(commands); }

private:
  [[nodiscard]] std::array<float, kLightFloats> PackedLights(const FrameContext &ctx) const;

  [[nodiscard]] static size_t
  PipelineAt(SurfaceDomain domain, VertexLayout layout, SurfaceKind kind, bool cullsBack);

  static constexpr size_t kSurfaceKinds = 5;

  static constexpr size_t kVertexLayoutCount = kVertexLayouts.size();
  static constexpr size_t kPipelines = kSurfaceDomains * kVertexLayoutCount * 2 * kSurfaceKinds;

  SDL_GPUDevice *Device = nullptr;
  std::array<OwnedPipeline, kPipelines> Pipelines;
  uint32_t Built = 0;

  std::vector<SurfaceSlot> Slots;
  std::vector<DrawBatch> Batches;

  std::vector<VertexLayout> BatchLayout;

  std::vector<Resource> Colours;
  SubjectResidency Own_;
  SubjectResidency *At_ = nullptr;
  SDL_GPUBuffer *SkyIrradiance_ = nullptr;
  SDL_GPUBuffer *GroundClasses_ = nullptr;
  SDL_GPUBuffer *GroundPalette_ = nullptr;

  [[nodiscard]] SubjectResidency &Bound() { return At_ != nullptr ? *At_ : Own_; }

  [[nodiscard]] const SubjectResidency &Bound() const { return At_ != nullptr ? *At_ : Own_; }

  SDL_GPUTexture *Atlas_ = nullptr;
  SDL_GPUSampler *AtlasSampler_ = nullptr;
  double LightFromWorld_[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  bool Shadowed_ = false;
  size_t ShadowedFrames_ = 0;
  size_t UniformPushes_ = 0;

public:
  [[nodiscard]] size_t ShadowedFrames() const { return ShadowedFrames_; }

  [[nodiscard]] size_t UniformPushes() const { return UniformPushes_; }

private:
  OwnedPipeline DepthOnly_;

  std::vector<SubjectLight> Placed;
  SubjectEnvironment IndirectLight;
  Vec3 Anchor;
  double Model[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  std::vector<double> Placed_;
  std::vector<double> Before_;
  std::vector<uint64_t> Stamped_;
  double ModelBefore_[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  uint64_t ModelStamp_ = 0;
  uint64_t Frame_ = 1;
  std::vector<float> Rows_;

  std::vector<uint32_t> Args_;
  uint32_t Jobs_ = 0;
  bool RowsStale_ = false;
  size_t Moved_ = 0;
  uint64_t Reshaped_ = 0;

  bool WritesVelocity = false;
};

} // namespace outshine::Render
#endif
