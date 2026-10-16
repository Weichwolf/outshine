#ifndef OUTSHINE_RENDER_STAGES_SUBJECTDRAW_H
#define OUTSHINE_RENDER_STAGES_SUBJECTDRAW_H

#include <span>
#include <optional>
#include <array>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include "FragmentArms.h"
#include "SurfaceOutputs.h"
#include "math/Mat4.h"
#include "math/Vec3.h"
#include "SurfaceState.h"

#include "FrameContext.h"
#include "Gpu.h"
#include <memory>
#include <cstddef>

#include "KernelShape.h"
#include "VertexArms.h"
#include "GroundLattice.h"
#include "SubjectResidency.h"
#include "GpuPlacement.h"
#include "SubjectMaterialPacking.h"
#include "SubjectTypes.h"
#include "SubjectLightingUniform.h"

namespace outshine::Render {

class SubjectPipelineBinding {
public:
  SubjectPipelineBinding() = default;
  SubjectPipelineBinding(const SubjectPipelineBinding &) = delete;
  SubjectPipelineBinding &operator=(const SubjectPipelineBinding &) = delete;
  SubjectPipelineBinding(SubjectPipelineBinding &&) noexcept = default;
  SubjectPipelineBinding &operator=(SubjectPipelineBinding &&) noexcept = default;

private:
  friend class SubjectDraw;
  static constexpr size_t kPipelineCount =
      kSurfaceDomains * kVertexLayouts.size() * 2 * kSurfaceKinds;
  Native::Device *Device = nullptr;
  std::array<std::unique_ptr<Native::GraphicsPipeline>, kPipelineCount> Pipelines;
  uint32_t Built = 0;
  std::vector<Resource> Colours;
  GroundPipelineBinding Ground;
  Native::Texture *Behind = nullptr;
  Native::Sampler *BehindSampler = nullptr;
  bool GlassDrawnElsewhere = false;
  bool WritesVelocity = false;
  bool CapturesMaterial = false;
};

struct SubjectMeshUploadMetrics {
  double AdmissionMs = 0.0;
  double IndexMs = 0.0;
  double StreamsMs = 0.0;
  double TablesMs = 0.0;
};

class SubjectDraw {
public:
  struct MeshTicket {
    uint64_t Generation = 0;
    bool NeedsFinish = false;
  };

  using SourceOptions = SurfaceOutputs;

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

  [[nodiscard]] Native::Result<void> Configure(const Gpu &gpu);
  [[nodiscard]] Native::Result<void> Configure(SubjectPipelineBinding &binding,
                                               const Gpu &gpu,
                                               Native::Texture *behind,
                                               Native::Sampler *exact,
                                               bool separateTransmission);
  [[nodiscard]] Native::Result<void> AttachPipelines(SubjectPipelineBinding &binding,
                                                     const Gpu &gpu);
  [[nodiscard]] Native::Result<void> PrepareGroundStorage(const Gpu &gpu);

  void UsePipelines(SubjectPipelineBinding &binding) noexcept {
    Binding_ = &binding;
    Ground_.UsePipelines(binding.Ground);
  }

  void SeeThroughTo(Native::Texture *behind, Native::Sampler *exact) {
    Binding().Behind = behind;
    Binding().BehindSampler = exact;
  }

  void SetSeparateTransmission(bool enabled) { Binding().GlassDrawnElsewhere = enabled; }

  void ShadowedBy(Native::Texture *atlas,
                  Native::Sampler *exact,
                  size_t lightIndex,
                  const Mat4 &lightFromWorld,
                  const std::array<ShadowRegionUniform, kSunShadowRegions> &regions,
                  size_t count) {
    Atlas_ = atlas;
    AtlasSampler_ = exact;
    Shadowed_ = atlas != nullptr && exact != nullptr && count > 0;
    ShadowRegionCount_ = Shadowed_ ? count : 0;
    ShadowLightIndex_ = lightIndex;
    if (!Shadowed_) { return; }
    LightFromWorld_ = lightFromWorld;
    ShadowRegions_ = regions;
  }

  void CastsNoShadow() { ShadowedBy(nullptr, nullptr, 0, Mat4{}, {}, 0); }

  [[nodiscard]] bool HandPlacements(bool deferred, std::string &error);

  [[nodiscard]] bool HandDrawArguments(bool deferred, std::string &error);

  [[nodiscard]] GroundLattice &Ground() { return Ground_; }

  [[nodiscard]] const GroundLattice &Ground() const { return Ground_; }

  void SetPiecesVisible(bool visible);

  [[nodiscard]] PieceId PlacePiece(const PieceMesh &piece, std::string &error);
  void ReleasePiece(PieceId which);
  [[nodiscard]] bool
  SetPieceInstances(PieceId which, std::span<const Mat4> rows, std::string &error);
  void SetNativePieceSurfaces(std::span<const uint32_t> slots);
  void SetRegisteredPieceSurfaces(std::span<const uint32_t> slots);
  [[nodiscard]] bool HandTables(std::string &error);

  [[nodiscard]] uint32_t PiecesStanding() const { return PiecesLive_; }

  [[nodiscard]] uint32_t PieceTriangles() const { return PieceTriangles_; }

  [[nodiscard]] uint32_t SubjectRows() const { return SubjectRows_; }

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
    for (const DrawBatch &batch : SubjectBatches_) {
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

  void MovePlacement(size_t slot, const Mat4 &model) {
    if ((slot + 1) * 16u > Placed_.size()) { return; }
    Before_.resize(Placed_.size(), 0.0);
    Stamped_.resize(Placed_.size() / 16u, 0u);
    if (Stamped_[slot] != Frame_) {
      const double *const carry = Stamped_[slot] == 0u ? model.data() : Placed_.data() + slot * 16u;
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
    for (const DrawBatch &batch : SubjectBatches_) {
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
  [[nodiscard]] bool ValidateMaterials(std::span<const SubjectMaterial> materials,
                                       std::string &error) const;
  [[nodiscard]] bool AppendMaterials(std::span<const SubjectMaterial> materials,
                                     std::string &error);

  [[nodiscard]] size_t MaterialSlots() const noexcept { return Slots.size(); }

  void TruncateMaterials(size_t count) noexcept;

  [[nodiscard]] bool ValidateMesh(const SubjectMesh &mesh, std::string &error) const;
  [[nodiscard]] bool SetMesh(const SubjectMesh &mesh, std::string &error);
  [[nodiscard]] std::expected<MeshTicket, std::string> BeginMesh(const SubjectMesh &mesh);
  [[nodiscard]] bool FinishMesh(MeshTicket ticket, const SubjectMesh &mesh, std::string &error);

  [[nodiscard]] bool MeshPending() const noexcept { return PendingMesh_.NeedsFinish; }

  [[nodiscard]] SubjectMeshUploadMetrics LastMeshUpload() const noexcept { return LastMeshUpload_; }

  [[nodiscard]] bool SetPose(const SubjectPose &pose, std::string &error);

private:
  class PipelineShaders;
  static constexpr size_t kVertexLayoutCount = kVertexLayouts.size();
  static constexpr size_t kPipelines = kSurfaceDomains * kVertexLayoutCount * 2 * kSurfaceKinds;

  static constexpr int kUniFloats = 56;

  struct FrameUniforms {
    std::array<float, kUniFloats> View{};
    std::array<float, sizeof(SubjectLightingUniform) / sizeof(float)> Light{};
  };

  struct DrawResources {
    static constexpr size_t kMostBindings = kSubjectImages + 3 + kSubjectFragmentUniforms + 1;
    std::array<Native::Binding, kMostBindings> Bindings{};
    std::array<Native::VertexBuffer, kMostVertexRuns> Vertices{};
    uint32_t BindingCount = 0;
    uint32_t VertexCount = 0;
  };

  [[nodiscard]] FrameUniforms FrameUniformsOf(const FrameContext &ctx);
  void BindVertexStreams(DrawResources &into, VertexLayout layout, VertexMotion motion) const;
  [[nodiscard]] static long ColourAttachment(std::span<const Resource> colours, Resource which);
  [[nodiscard]] static Native::Result<void>
  ConfigureKind(const Gpu &gpu,
                const SourceOptions &options,
                std::span<const Resource> colours,
                SurfaceKind kind,
                PipelineShaders &shaders,
                std::array<std::unique_ptr<Native::GraphicsPipeline>, kPipelines> &pipelines,
                uint32_t &built);
  [[nodiscard]] static Native::Result<void>
  ConfigureVariant(SurfaceKind kind,
                   SurfaceDomain domain,
                   VertexLayout layout,
                   const SourceOptions &options,
                   std::span<const Native::ColourTarget> targets,
                   Native::Device &device,
                   PipelineShaders &shaders,
                   std::array<std::unique_ptr<Native::GraphicsPipeline>, kPipelines> &pipelines,
                   uint32_t &built);

  [[nodiscard]] bool
  ValidateBatch(const SubjectMesh &mesh, const DrawBatch &batch, std::string &error) const;
  [[nodiscard]] bool ValidatePiece(const PieceMesh &piece, std::string &error) const;

  [[nodiscard]] bool HandStreams(const SubjectPose &pose, bool deferred, std::string &error);
  void BindSlot(DrawResources &into,
                size_t slot,
                VertexLayout layout,
                const FrameUniforms &uniforms) const;
  [[nodiscard]] Native::Result<void> EncodeGround(const PassRecording &into,
                                                  const FrameUniforms &uniforms) const;
  [[nodiscard]] Native::Result<void> DrawBatchRange(const PassRecording &into,
                                                    size_t first,
                                                    bool culled,
                                                    size_t end,
                                                    const DrawResources &resources);

public:
  [[nodiscard]] bool SetLights(std::span<const SubjectLight> lights, std::string &error);

  void SetEnvironment(const SubjectEnvironment &environment) { IndirectLight = environment; }

  [[nodiscard]] const SubjectEnvironment &Environment() const { return IndirectLight; }

  struct Reflections {
    Native::Texture *Atlas = nullptr;
    Native::Sampler *Sampler = nullptr;
    Vec3f Up = {{0, 1, 0}};
    Vec3f ToSun = {{0, 1, 0}};
  };

  void ReflectionsFrom(Reflections from) { Reflections_ = from; }

  struct GroundBuffers {
    Native::Buffer *Classes = nullptr;
    Native::Buffer *Palette = nullptr;
  };

  void GroundFrom(GroundBuffers from) {
    GroundClasses_ = from.Classes;
    GroundPalette_ = from.Palette;
  }

  [[nodiscard]] Native::Result<void> Encode(const FrameContext &ctx, const PassRecording &into);

  [[nodiscard]] const SubjectResidency &Resident() const { return Bound(); }

  void Shares(SubjectResidency &other) { At_ = &other; }

  [[nodiscard]] SubjectResidency &Owned() { return Own_; }

  [[nodiscard]] bool Borrows() const { return At_ != nullptr; }

  [[nodiscard]] const std::vector<DrawBatch> &Drawn() const { return Batches; }

  [[nodiscard]] Native::Result<void> EncodeShadow(const PassRecording &into,
                                                  const Native::GraphicsPipeline &pipeline,
                                                  const DrawBatch &batch,
                                                  uint32_t instances,
                                                  std::span<const std::byte> view) const;

  [[nodiscard]] uint32_t ColourImages() const;
  [[nodiscard]] uint32_t DistinctPlacements() const;
  [[nodiscard]] uint32_t Layouts() const;
  [[nodiscard]] uint32_t Textured() const;

  [[nodiscard]] const std::vector<double> &Placements() const { return Placed_; }

  [[nodiscard]] const Vec3 &AnchorM() const { return Anchor; }

  [[nodiscard]] const Mat4 &ModelM() const { return Model; }

  [[nodiscard]] uint64_t HeldBytes() const { return At_ != nullptr ? 0u : Own_.HeldBytes(); }

  [[nodiscard]] uint32_t StagedBytes() const { return Bound().StagedBytes(); }

  void ForgetStagedCount() { Bound().ForgetStagedCount(); }

  [[nodiscard]] uint32_t VertexCount() const { return Bound().Shape().Vertices; }

  [[nodiscard]] long TriangleCount() const {
    return static_cast<long>(Bound().Shape().Indices) / 3;
  }

  [[nodiscard]] uint32_t BatchCount() const { return static_cast<uint32_t>(Batches.size()); }

  [[nodiscard]] uint32_t DrawCount() const;

  [[nodiscard]] uint32_t EncodedDrawCalls() const noexcept { return EncodedDrawCalls_; }

  [[nodiscard]] uint32_t PipelineCount() const { return Binding().Built; }

private:
  static constexpr size_t kLightFloats = sizeof(SubjectLightingUniform) / sizeof(float);

  struct SurfaceSlot {
    SubjectResidency::BoundImage Colour;
    SubjectResidency::BoundImage Normal;
    SubjectResidency::BoundImage MetalRough;
    SubjectResidency::BoundImage Emissive;

    SubjectResidency::BoundImage SpecularStrength;
    SubjectResidency::BoundImage SpecularTint;
    SubjectResidency::BoundImage DiffuseReflection;
    SubjectResidency::BoundImage DiffuseTransmission;

    PackedSubjectMaterial Row{};
    SurfaceKind Kind = SurfaceKind::Opaque;
    bool CullsBack = true;
    std::optional<std::array<float, 3>> Unlit;

    SurfaceDomain Domain = SurfaceDomain::Subject;
    bool ReadsSecondUv = false;
  };

  GroundLattice Ground_;

  [[nodiscard]] bool BindSurface(const SubjectMaterial &material, std::string &error);

public:
  [[nodiscard]] Native::Result<void> FlushCrossings(Native::Commands *commands) {
    return Bound().FlushCrossings(commands);
  }

  void CommitCrossings() { Bound().CommitCrossings(); }

private:
  [[nodiscard]] std::array<float, kLightFloats> PackedLights(const FrameContext &ctx) const;

  [[nodiscard]] static size_t
  PipelineAt(SurfaceDomain domain, VertexLayout layout, SurfaceKind kind, bool cullsBack);

  std::vector<SurfaceSlot> Slots;
  std::vector<DrawBatch> Batches;

  std::vector<VertexLayout> BatchLayout;

  SubjectResidency Own_;
  SubjectResidency *At_ = nullptr;
  Native::Buffer *GroundClasses_ = nullptr;
  Native::Buffer *GroundPalette_ = nullptr;

  [[nodiscard]] SubjectResidency &Bound() { return At_ != nullptr ? *At_ : Own_; }

  [[nodiscard]] const SubjectResidency &Bound() const { return At_ != nullptr ? *At_ : Own_; }

  Native::Texture *Atlas_ = nullptr;
  Native::Sampler *AtlasSampler_ = nullptr;
  std::array<ShadowRegionUniform, kSunShadowRegions> ShadowRegions_{};
  size_t ShadowRegionCount_ = 0;
  size_t ShadowLightIndex_ = 0;
  Mat4 LightFromWorld_ = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  size_t ShadowedFrames_ = 0;
  size_t UniformPushes_ = 0;

public:
  [[nodiscard]] size_t ShadowedFrames() const { return ShadowedFrames_; }

  [[nodiscard]] size_t UniformPushes() const { return UniformPushes_; }

private:
  std::vector<SubjectLight> Placed;
  SubjectEnvironment IndirectLight;
  Reflections Reflections_;
  Vec3 Anchor;
  Mat4 Model = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  std::vector<double> Placed_;
  std::vector<double> Before_;
  std::vector<uint64_t> Stamped_;
  Mat4 ModelBefore_ = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  uint64_t ModelStamp_ = 0;
  uint64_t Frame_ = 1;
  std::vector<GpuPlacement> Rows_;

  std::vector<uint32_t> Args_;
  uint32_t Jobs_ = 0;
  uint32_t EncodedDrawCalls_ = 0;

  struct Piece {
    SubjectResidency::Range V;
    SubjectResidency::Range I;
    SubjectResidency::Range C;
    SubjectResidency::Range T;
    uint32_t IndexCount = 0;
    PieceSurface Surface;
    VertexLayout Layout = VertexLayout::PositionNormal;
    std::optional<std::array<float, 3>> Emitted;
    std::vector<Mat4> Rows;
    uint32_t FirstRow = 0;
    uint32_t MaxInstances = 0;
    std::vector<DagCluster> Clusters;
    bool Live = false;
  };

  std::vector<DrawBatch> SubjectBatches_;
  std::vector<uint32_t> SubjectJobs_;
  std::vector<float> SubjectSpheres_;
  std::vector<uint32_t> TableJobs_, TableOrder_, TableRows_;
  std::vector<float> TableSpheres_;
  uint32_t SubjectRows_ = 0;
  std::vector<Piece> Pieces_;
  std::vector<uint32_t> Spare_;
  std::vector<uint32_t> SlotOf_, RegisteredSlotOf_;
  uint32_t PiecesLive_ = 0;
  uint32_t PieceTriangles_ = 0;

  [[nodiscard]] bool RoomForStreams(std::string &error);

  struct OptionalStreamEnds {
    uint32_t VertexEnd;
    uint32_t ColourEnd;
    uint32_t TangentEnd;
  };

  [[nodiscard]] bool
  RoomForOptionalStreams(OptionalStreamEnds ends, VertexRunsCarried carried, std::string &error);
  void BindPlacementStorage(DrawResources &into, VertexLayout layout) const;
  [[nodiscard]] size_t IndirectRunEnd(size_t first) const noexcept;
  [[nodiscard]] bool Retable(std::string &error);
  [[nodiscard]] bool BuildSubjectBatches(std::string &error);
  void PrepareSubjectTables();
  [[nodiscard]] uint32_t MaterialSlotFor(const Piece &piece) const;
  void OrderPieces();
  [[nodiscard]] bool AppendPieceBatches(Piece &one, std::string &error);
  [[nodiscard]] bool UploadTables(std::string &error);
  size_t Moved_ = 0;
  uint64_t Reshaped_ = 0;
  SubjectMeshUploadMetrics LastMeshUpload_;
  MeshTicket PendingMesh_;
  const DrawList *PendingDraws_ = nullptr;
  const uint32_t *PendingIndices_ = nullptr;
  uint32_t PendingIndexCount_ = 0;

  SubjectPipelineBinding OwnedBinding_;
  SubjectPipelineBinding *Binding_ = &OwnedBinding_;
  bool Shadowed_ = false;
  bool RowsStale_ = false;
  bool TablesStale_ = false;
  bool PiecesVisible_ = true;

  [[nodiscard]] SubjectPipelineBinding &Binding() { return *Binding_; }

  [[nodiscard]] const SubjectPipelineBinding &Binding() const { return *Binding_; }
};

}
#endif
