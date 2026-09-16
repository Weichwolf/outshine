#include "Lens.h"
#ifndef OUTSHINE_RENDER_SCENERENDERER_H
#define OUTSHINE_RENDER_SCENERENDERER_H

#include "math/Mat4.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include "Extent.h"
#include "Heap.h"
#include "scenario/Scenario.h"
#include <array>
#include <span>
#include <cstdint>
#include <memory>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuSubmission.h"
#include "GroundStorage.h"
#include "GpuOwned.h"
#include "Readback.h"
#include "Viewing.h"
#include "Compiled.h"
#include "stages/OverlayDraw.h"
#include "stages/PresentStage.h"
#include "stages/Resolve.h"
#include "stages/SubjectDraw.h"
#include "stages/AerialPerspectiveStage.h"
#include "stages/CompositeTransmissionStage.h"
#include "stages/MediumMultiScatterStage.h"
#include "stages/DepthPyramidStage.h"
#include "stages/IrradianceStage.h"
#include "stages/MediumRadianceStage.h"
#include "stages/LightVisibilityStage.h"
#include "stages/SubjectCullStage.h"
#include "stages/SkyStage.h"
#include "stages/MediumTransmittanceStage.h"
#include "stages/TonemapStage.h"

namespace outshine::Render {

struct KeptDraws {
  uint32_t Indices = 0;
  uint32_t Batches = 0;
};

struct PyramidDepths {
  float Nearest = 0.0f;
  float Farthest = 1.0f;
  float Mean = 0.0f;
};

class SceneRenderer {
public:
  [[nodiscard]] std::expected<void, std::string> Init(Extent frame,
                                                      std::shared_ptr<const Compiled> plan);

  [[nodiscard]] const Compiled &Plan() const { return *State_.Plan; }

  [[nodiscard]] bool DeviceUsable() const { return Ready_; }

  [[nodiscard]] const std::string &WhyNot() const { return WhyNot_; }

  struct Shown {
    int WidthPx = 0;
    int HeightPx = 0;
  };

  [[nodiscard]] std::expected<void, std::string>
  DrawsInto(int widthPx, int heightPx, SDL_Window *presents);
  [[nodiscard]] std::expected<std::optional<Shown>, std::string_view> Presented() const;
  void StopShowing();

  [[nodiscard]] SDL_GPUTextureFormat SurfaceFormat() const;

  void PresentInto(SDL_GPUTexture *surface) { State_.Frame.HostSurface = surface; }

  struct Region {
    double X = 0.0;
    double Y = 0.0;
    double Width = 0.0;
    double Height = 0.0;
    double Aspect = 0.0;
  };

  void SetPictureRegion(Region into) {
    State_.RegionX = into.X;
    State_.RegionY = into.Y;
    State_.RegionW = into.Width;
    State_.RegionH = into.Height;
    State_.RegionAspect = into.Aspect;
  }

  [[nodiscard]] std::expected<void, std::string> RenderFrame();

  [[nodiscard]] bool Drew() const { return State_.Submitted; }

  ~SceneRenderer() {
    WaitForGpu();
    StopShowing();
  }

  explicit SceneRenderer(GpuSubmission submission = {}) : Submission_(submission) {}

  SceneRenderer(const SceneRenderer &) = delete;
  SceneRenderer &operator=(const SceneRenderer &) = delete;

  void WaitForGpu();

  static constexpr int kFramesInFlight = 2;

  [[nodiscard]] int SettleFrames() const { return State_.Plan ? State_.Plan->SettleFrames() : 1; }

  [[nodiscard]] bool Queued() const { return Presenting_ == SDL_GPU_PRESENTMODE_VSYNC; }

  [[nodiscard]] bool Presents() const { return Showing_ != nullptr; }

  [[nodiscard]] bool Settle(std::string &error);

  [[nodiscard]] ReadState ReadPixels(std::vector<uint8_t> &rgba);

  [[nodiscard]] ReadState ReadDepth(std::vector<float> &depth);
  [[nodiscard]] static bool Executable(Stage stage);

  void CastsBelow(uint32_t slot) { State_.Frame.Shadow.CastsBelow(slot); }

  [[nodiscard]] ReadState ReadShadowAtlas(std::vector<float> &depth);
  static constexpr float kNearM = static_cast<float>(outshine::Camera::kNearestM);

  [[nodiscard]] ReadState ReadSceneLinear(std::vector<float> &rgba);

  [[nodiscard]] ReadState ReadKeptIndices(KeptDraws &into);

  [[nodiscard]] PieceId PlacePiece(const PieceMesh &piece, std::string &error) {
    return State_.Content.Subjects.PlacePiece(piece, error);
  }

  void ReleasePiece(PieceId which) { State_.Content.Subjects.ReleasePiece(which); }

  [[nodiscard]] PageId PlaceHeightPage(std::span<const float> nodes, std::string &error) {
    return State_.Content.Subjects.Ground().PlacePage(nodes, error);
  }

  void ReleaseHeightPage(PageId which) { State_.Content.Subjects.Ground().ReleasePage(which); }

  [[nodiscard]] bool SetGroundGrid(std::span<const float> fractions, std::string &error) {
    return State_.Content.Subjects.Ground().SetGrid(fractions, error);
  }

  [[nodiscard]] bool SetGroundLattice(std::span<const GroundTile> real,
                                      std::span<const GroundTile> virtual_,
                                      std::string &error) {
    return State_.Content.Subjects.Ground().SetInstances(real, virtual_, error);
  }

  [[nodiscard]] uint32_t GroundLatticeTriangles() const {
    return State_.Content.Subjects.Ground().Triangles();
  }

  [[nodiscard]] bool
  SetPieceInstances(PieceId which, std::span<const Mat4> rows, std::string &error) {
    return State_.Content.Subjects.SetPieceInstances(which, rows, error);
  }

  void WearPieces(std::span<const uint32_t> slotOfSurface,
                  std::span<const uint32_t> registered = {}) {
    State_.Content.Subjects.WearPieces(slotOfSurface, registered);
  }

  [[nodiscard]] uint32_t PiecesStanding() const { return State_.Content.Subjects.PiecesStanding(); }

  [[nodiscard]] uint32_t PieceTriangles() const { return State_.Content.Subjects.PieceTriangles(); }

  [[nodiscard]] size_t TakeUploadAttempts() {
    return State_.Content.Subjects.Owned().TakeUploadAttempts();
  }

  [[nodiscard]] size_t TotalUploadAttempts() const {
    return State_.Content.Subjects.Resident().TotalUploadAttempts();
  }

  [[nodiscard]] size_t RecordedCrossings() const {
    return State_.Content.Subjects.Resident().RecordedCrossings();
  }

  [[nodiscard]] size_t TakeUploadBytes() {
    return State_.Content.Subjects.Owned().TakeUploadBytes();
  }

  [[nodiscard]] size_t TakeBufferAllocationAttempts() {
    return State_.Content.Subjects.Owned().TakeBufferAllocationAttempts();
  }

  [[nodiscard]] size_t TakeStagingAllocationAttempts() {
    return State_.Content.Subjects.Owned().TakeStagingAllocationAttempts();
  }

  [[nodiscard]] uint32_t PieceBytesHeld() const {
    return State_.Content.Subjects.Resident().HeldBytes();
  }

  [[nodiscard]] ReadState ReadSkyIrradiance(std::span<float, kIrradianceFloats> out);

  [[nodiscard]] ReadState ReadPyramid(PyramidDepths &into);

  [[nodiscard]] ReadState ReadShadingNormal(std::vector<float> &xyz);

  [[nodiscard]] ReadState ReadSurfaceIdentity(std::vector<float> &slot);

  [[nodiscard]] ReadState ReadSceneVelocity(std::vector<float> &xy);

  [[nodiscard]] bool ReplaceOverlay(std::span<const OverlayQuad> quads,
                                    const OverlayDraw::AtlasPixels *atlas,
                                    std::string &error) {
    return State_.Content.Overlay.Replace(
        State_.Frame.Handles, quads.data(), quads.size(), atlas, error);
  }

  [[nodiscard]] bool SetOverlay(const OverlayQuad *quads, size_t count, std::string &error) {
    return State_.Content.Overlay.SetQuads(State_.Frame.Handles, quads, count, error);
  }

  [[nodiscard]] bool
  SetOverlayAtlas(const uint8_t *rgba, int width, int height, std::string &error) {
    return State_.Content.Overlay.SetAtlas(State_.Frame.Handles, rgba, width, height, error);
  }

  [[nodiscard]] bool SetSubjectMesh(const SubjectMesh &mesh, std::string &error) {
    const Heap::Tagged relaying("mesh-relay");
    if (State_.Content.DrawsGlass && !State_.Content.Glass.ValidateMesh(mesh, error)) {
      return false;
    }
    return State_.Content.Subjects.SetMesh(mesh, error) &&
           (!State_.Content.DrawsGlass || State_.Content.Glass.SetMesh(mesh, error));
  }

  [[nodiscard]] bool SubjectPlacementRows(size_t rows, std::string &error) {
    return State_.Content.Subjects.PlacementRows(rows, error) &&
           (!State_.Content.DrawsGlass || State_.Content.Glass.PlacementRows(rows, error));
  }

  void MoveSubjectPlacement(size_t slot, const Mat4 &model) {
    State_.Content.Subjects.MovePlacement(slot, model);
    if (State_.Content.DrawsGlass) { State_.Content.Glass.MovePlacement(slot, model); }
  }

  [[nodiscard]] bool HandSubjectPlacements(std::string &error) {
    return State_.Content.Subjects.HandPlacements(false, error) &&
           (!State_.Content.DrawsGlass || State_.Content.Glass.HandPlacements(false, error));
  }

  [[nodiscard]] size_t SubjectPlacementsMoved() const {
    return State_.Content.Subjects.PlacementsMoved();
  }

  [[nodiscard]] uint32_t SubjectBytesStaged() const {
    return State_.Content.Subjects.StagedBytes();
  }

  void ForgetSubjectStaging() { State_.Content.Subjects.ForgetStagedCount(); }

  [[nodiscard]] const Vec3 &ShadowStoodAtM() const { return State_.Frame.Shadow.StoodAtM(); }

  [[nodiscard]] bool SetSubjectPlacements(const double *models, size_t rows, std::string &error) {
    return State_.Content.Subjects.SetPlacements(models, rows, error) &&
           (!State_.Content.DrawsGlass || State_.Content.Glass.SetPlacements(models, rows, error));
  }

  [[nodiscard]] bool SetSubjectPose(const SubjectPose &pose, std::string &error) {
    return State_.Content.Subjects.SetPose(pose, error) &&
           (!State_.Content.DrawsGlass || State_.Content.Glass.SetPose(pose, error));
  }

  [[nodiscard]] bool SetSubjectMaterials(std::span<const SubjectMaterial> materials,
                                         std::string &error) {
    return State_.Content.Subjects.SetMaterials(materials, error) &&
           (!State_.Content.DrawsGlass || State_.Content.Glass.SetMaterials(materials, error));
  }

  [[nodiscard]] bool AppendSubjectMaterials(std::span<const SubjectMaterial> materials,
                                            std::string &error) {
    if (!State_.Content.Subjects.ValidateMaterials(materials, error) ||
        (State_.Content.DrawsGlass && !State_.Content.Glass.ValidateMaterials(materials, error))) {
      return false;
    }
    return State_.Content.Subjects.AppendMaterials(materials, error) &&
           (!State_.Content.DrawsGlass || State_.Content.Glass.AppendMaterials(materials, error));
  }

  [[nodiscard]] bool SetSubjectLights(std::span<const SubjectLight> lights, std::string &error) {
    return State_.Content.Subjects.SetLights(lights, error) &&
           (!State_.Content.DrawsGlass || State_.Content.Glass.SetLights(lights, error));
  }

  void SetMedium(const Medium &medium) {
    State_.Medium = medium;
    State_.Frame.MediumTransmittance.Declare(medium);
    State_.Frame.MultiScatter.Declare(medium);
    State_.Frame.Radiance.Declare(medium, State_.CosSunZenith, State_.EyeHeightM);
    State_.Frame.SkyIrradianceStage.Declare(medium, State_.CosSunZenith);
  }

  void SetShadowFrame(const Vec3f &toSun, const Vec3f &up, double radiusM) {
    State_.Frame.Shadow.Declare({.ToSun = toSun, .Up = up}, radiusM);
  }

  void SetSky(const Vec3f &toSun, const Vec3f &up, float illuminanceLux, float eyeHeightM) {
    State_.CosSunZenith = toSun[0] * up[0] + toSun[1] * up[1] + toSun[2] * up[2];
    State_.EyeHeightM = eyeHeightM;
    State_.Frame.Radiance.Declare(State_.Medium, State_.CosSunZenith, State_.EyeHeightM);
    State_.Frame.SkyIrradianceStage.Declare(State_.Medium, State_.CosSunZenith);
    const SkyStanding stands = {
        .SunDir = toSun, .WorldUp = up, .IlluminanceLux = illuminanceLux, .EyeHeightM = eyeHeightM};
    State_.Frame.Sky.Declare(State_.Medium, stands);
    State_.Frame.Aerial.Declare(State_.Medium, stands);
  }

  void SetSkyEye(float eyeHeightM) {
    if (!State_.Frame.Sky.Stands()) { return; }
    State_.EyeHeightM = eyeHeightM;
    State_.Frame.Radiance.Declare(State_.Medium, State_.CosSunZenith, State_.EyeHeightM);
    State_.Frame.SkyIrradianceStage.Declare(State_.Medium, State_.CosSunZenith);
    State_.Frame.Sky.Eye(State_.Medium, eyeHeightM);
    State_.Frame.Aerial.Eye(State_.Medium, eyeHeightM);
  }

  [[nodiscard]] SDL_GPUTexture *SkyViewTable() const { return State_.Frame.SkyViewLut.Get(); }

  [[nodiscard]] SDL_GPUTexture *MultiScatterTable() const {
    return State_.Frame.MultiScatterLut.Get();
  }

  [[nodiscard]] SDL_GPUTexture *TransmittanceTable() const {
    return State_.Frame.TransmittanceLut.Get();
  }

  void SetSubjectEnvironment(const SubjectEnvironment &environment) {
    State_.Content.Subjects.SetEnvironment(environment);
    if (State_.Content.DrawsGlass) { State_.Content.Glass.SetEnvironment(environment); }
  }

  [[nodiscard]] uint32_t SubjectBatchCount() const { return State_.Content.Subjects.BatchCount(); }

  [[nodiscard]] uint32_t SubjectBatchesTaking(VertexLayout layout) const {
    uint32_t many = 0;
    for (const DrawBatch &batch : State_.Content.Subjects.Drawn()) {
      many += batch.Layout == layout ? 1u : 0u;
    }
    return many;
  }

  [[nodiscard]] size_t ShadowCastCount() const { return State_.Frame.Shadow.CastBatches(); }

  [[nodiscard]] size_t ShadowedFrames() const { return State_.Content.Subjects.ShadowedFrames(); }

  struct Effort {
    double TookMs = 0.0;
    uint32_t DeviceBytes = 0;
    uint32_t Draws = 0;
    uint32_t Triangles = 0;
    uint32_t Surfaces = 0;
    uint32_t Placements = 0;
    uint32_t Textured = 0;
    uint32_t Palettes = 0;
    uint32_t Distinct = 0;
    uint32_t Layouts = 0;
  };

  [[nodiscard]] const Effort &Spent(Stage stage) const {
    return Spent_[static_cast<size_t>(stage)];
  }

  [[nodiscard]] size_t SubjectUniformPushes() const {
    return State_.Content.Subjects.UniformPushes() + State_.Content.Glass.UniformPushes();
  }

  [[nodiscard]] float ExposureApplied() const {
    return State_.Plan ? State_.Plan->Exposure() : 0.0f;
  }

  [[nodiscard]] uint32_t SubjectDrawCount() const { return State_.Content.Subjects.DrawCount(); }

  [[nodiscard]] uint32_t SubjectPipelineCount() const {
    return State_.Content.Subjects.PipelineCount();
  }

  void SetCamera(const CameraBasis &basis, const Lens &lens) noexcept;

  [[nodiscard]] bool SetGroundClasses(std::span<const uint32_t> classes,
                                      std::span<const float> palette,
                                      std::string &error);

  [[nodiscard]] float NearMetres() const { return State_.NearM; }

  void BeginTemporalRun();

  [[nodiscard]] int SceneW() const { return State_.Frame.Width; }

  [[nodiscard]] int SceneH() const { return State_.Frame.Height; }

  [[nodiscard]] double PictureW() const;
  [[nodiscard]] double PictureH() const;

  [[nodiscard]] double SceneAspect() const {
    return PictureH() > 0 ? PictureW() / PictureH() : 0.0;
  }

private:
  struct FrameResources;

  [[nodiscard]] std::expected<void, std::string> PrepareFrame();
  GpuSubmission Submission_;
  std::array<Effort, kStageCount> Spent_ = {{}};

  void Create(FrameResources &frame, const Compiled &plan, Resource resource);
  [[nodiscard]] static bool Created(const FrameResources &frame, Resource resource);
  [[nodiscard]] bool Configure(Stage stage,
                               FrameResources &frame,
                               const Compiled &plan,
                               bool drawsGlass,
                               std::string &error);
  [[nodiscard]] bool
  ConfigurePlanStages(FrameResources &frame, const Compiled &plan, bool drawsGlass);
  [[nodiscard]] static AttachmentSet
  ColoursForStage(const FrameResources &frame, const Compiled &plan, Stage wanted);
  void EncodeStage(Stage stage, const PassRecording &into);

  struct Executor {
    Stage Named;
    bool (*Configure)(SceneRenderer &renderer,
                      FrameResources &frame,
                      const Compiled &plan,
                      bool drawsGlass,
                      std::string &error);
    void (SceneRenderer::*Encode)(const FrameContext &ctx, const PassRecording &into);
  };

  static constexpr size_t kExecutorCount = 18;
  static const std::array<Executor, kExecutorCount> kExecutors;
  [[nodiscard]] static const Executor *ExecutorOf(Stage stage);
  void Picture(bool picture, const PassRecording &into);
  [[nodiscard]] static bool ConfigureSubjects(SceneRenderer &renderer,
                                              FrameResources &frame,
                                              const Compiled &plan,
                                              bool drawsGlass,
                                              std::string &error);
  [[nodiscard]] static bool ConfigureGlass(SceneRenderer &renderer,
                                           FrameResources &frame,
                                           const Compiled &plan,
                                           bool drawsGlass,
                                           std::string &error);
  [[nodiscard]] static bool ConfigureCompositeTransmission(SceneRenderer &renderer,
                                                           FrameResources &frame,
                                                           const Compiled &plan,
                                                           bool drawsGlass,
                                                           std::string &error);
  [[nodiscard]] static bool ConfigureOverlay(SceneRenderer &renderer,
                                             FrameResources &frame,
                                             const Compiled &plan,
                                             bool drawsGlass,
                                             std::string &error);
  [[nodiscard]] static bool ConfigurePresent(SceneRenderer &renderer,
                                             FrameResources &frame,
                                             const Compiled &plan,
                                             bool drawsGlass,
                                             std::string &error);
  [[nodiscard]] static bool ConfigureTonemap(SceneRenderer &renderer,
                                             FrameResources &frame,
                                             const Compiled &plan,
                                             bool drawsGlass,
                                             std::string &error);
  [[nodiscard]] static bool ConfigureMediumTransmittance(SceneRenderer &renderer,
                                                         FrameResources &frame,
                                                         const Compiled &plan,
                                                         bool drawsGlass,
                                                         std::string &error);
  [[nodiscard]] static bool ConfigureMediumMultiScatter(SceneRenderer &renderer,
                                                        FrameResources &frame,
                                                        const Compiled &plan,
                                                        bool drawsGlass,
                                                        std::string &error);
  [[nodiscard]] static bool ConfigureMediumRadiance(SceneRenderer &renderer,
                                                    FrameResources &frame,
                                                    const Compiled &plan,
                                                    bool drawsGlass,
                                                    std::string &error);
  [[nodiscard]] static bool ConfigureIrradiance(SceneRenderer &renderer,
                                                FrameResources &frame,
                                                const Compiled &plan,
                                                bool drawsGlass,
                                                std::string &error);
  [[nodiscard]] static bool ConfigureDepthPyramid(SceneRenderer &renderer,
                                                  FrameResources &frame,
                                                  const Compiled &plan,
                                                  bool drawsGlass,
                                                  std::string &error);
  [[nodiscard]] static bool ConfigureSky(SceneRenderer &renderer,
                                         FrameResources &frame,
                                         const Compiled &plan,
                                         bool drawsGlass,
                                         std::string &error);
  [[nodiscard]] static bool ConfigureAerialPerspective(SceneRenderer &renderer,
                                                       FrameResources &frame,
                                                       const Compiled &plan,
                                                       bool drawsGlass,
                                                       std::string &error);
  [[nodiscard]] static bool ConfigureLightVisibility(SceneRenderer &renderer,
                                                     FrameResources &frame,
                                                     const Compiled &plan,
                                                     bool drawsGlass,
                                                     std::string &error);
  [[nodiscard]] static bool ConfigureSubjectCull(SceneRenderer &renderer,
                                                 FrameResources &frame,
                                                 const Compiled &plan,
                                                 bool drawsGlass,
                                                 std::string &error);
  void EncodeSubjects(const FrameContext &ctx, const PassRecording &into);
  void EncodeGlass(const FrameContext &ctx, const PassRecording &into);
  void EncodeCompositeTransmission(const FrameContext &ctx, const PassRecording &into);
  void EncodeOverlay(const FrameContext &ctx, const PassRecording &into);
  void EncodePresent(const FrameContext &ctx, const PassRecording &into);
  void EncodeTonemap(const FrameContext &ctx, const PassRecording &into);
  void EncodeMediumTransmittance(const FrameContext &ctx, const PassRecording &into);
  void EncodeMediumMultiScatter(const FrameContext &ctx, const PassRecording &into);
  void EncodeMediumRadiance(const FrameContext &ctx, const PassRecording &into);

  void EncodeIrradiance(const FrameContext &ctx, const PassRecording &into);
  void EncodeDepthPyramid(const FrameContext &ctx, const PassRecording &into);
  [[nodiscard]] EyeBasis Eye() const;
  void EncodeSky(const FrameContext &ctx, const PassRecording &into);
  void EncodeAerialPerspective(const FrameContext &ctx, const PassRecording &into);
  void EncodeLightVisibility(const FrameContext &ctx, const PassRecording &into);
  void EncodeSubjectCull(const FrameContext &ctx, const PassRecording &into);
  void EncodeSubjectScan(const FrameContext &ctx, const PassRecording &into);
  void EncodeSubjectCompact(const FrameContext &ctx, const PassRecording &into);
  void EncodePass(SDL_GPUCommandBuffer *commands, size_t pass, StageSubmission &submission);
  void EncodeComputePass(SDL_GPUCommandBuffer *commands,
                         const Compiled::Pass &declared,
                         StageSubmission &submission);
  void EncodeGraphicsPass(SDL_GPUCommandBuffer *commands,
                          const Compiled::Pass &declared,
                          StageSubmission &submission);
  [[nodiscard]] FrameContext Framing() const;
  void SettleShadow();
  std::array<bool, kResourceCount> Touched_ = {{}};
  [[nodiscard]] SDL_GPUTexture *Target(Resource resource) const;
  [[nodiscard]] static SDL_GPUTexture *Target(const FrameResources &frame, Resource resource);
  void BindFrameResources();

  [[nodiscard]] SDL_GPUBuffer *BufferFor(Resource resource) const;
  [[nodiscard]] DisplayOptions Display() const;

  [[nodiscard]] SDL_GPUTexture *LinearSource() const;
  [[nodiscard]] SDL_GPUTexture *DisplaySource() const;

  OwnedDevice Device_;

  struct WorldContent {
    SubjectDraw Subjects;
    SubjectDraw Glass;
    OverlayDraw Overlay;
    bool DrawsGlass = false;

    WorldContent() = default;
    WorldContent(const WorldContent &) = delete;
    WorldContent &operator=(const WorldContent &) = delete;
    WorldContent(WorldContent &&) noexcept = default;
    WorldContent &operator=(WorldContent &&) noexcept = default;
  };

  struct FrameResources {
    SDL_GPUTexture *HostSurface = nullptr;
    Shown Shown;
    OwnedTexture Offscreen;
    Gpu Handles;
    OwnedTexture HdrTex, VelTex, DepthTex, FrameTex;
    OwnedTexture TransmittanceLut, MultiScatterLut, SkyViewLut;
    OwnedTexture ShadowAtlas;
    OwnedTexture TransmissiveTex, CompositedTex, AerialTex;
    OwnedTexture ShadingNormalTex;
    OwnedTexture SurfaceIdentityTex;
    OwnedSampler Samp, LutSamp;
    OwnedBuffer IrradianceBuffer;
    OwnedBuffer Pyramid;
    Readback PyramidRead;
    int Width = 0, Height = 0;
    std::array<OwnedTexture, 2> LinearTex{};
    int LinearAt = 0;
    bool HistoryHeld = false;
    CompositeTransmissionStage CompositeTransmission;
    AerialPerspectiveStage Aerial;
    TonemapStage Tonemap;
    MediumTransmittanceStage MediumTransmittance;
    MediumMultiScatterStage MultiScatter;
    MediumRadianceStage Radiance;
    IrradianceStage SkyIrradianceStage;
    DepthPyramidStage PyramidStage;
    SkyStage Sky;
    LightVisibilityStage Shadow;
    SubjectCullStage Cull;
    OverlayPipeline OverlayPipe;
    PresentStage Present;
    SubjectPipelineBinding SubjectPipelines;
    SubjectPipelineBinding GlassPipelines;
  };

  struct SceneState {
    FrameResources Frame;
    std::shared_ptr<const Compiled> Plan;
    WorldContent Content;
    Medium Medium = kEarthAir;
    float CosSunZenith = 1.0f;
    float EyeHeightM = 0.0f;
    bool HistoryStarted = false;
    int JitterAt = 0;
    Vec2f Jitter = {{0.0f, 0.0f}};
    Vec2f PrevJitter = {{0.0f, 0.0f}};
    bool CameraFull = false;
    double RegionX = 0.0;
    double RegionY = 0.0;
    double RegionW = 0.0;
    double RegionH = 0.0;
    double RegionAspect = 0.0;
    CameraBasis Camera;
    float FovDeg = 0.0f;
    float OrthoWidthM = 0.0f;
    float OrthoM = 0.0f;
    float NearM = kNearM;
    float FarM = 0.0f;
    bool Submitted = false;
    Vec3 PrevEye = {{0, 0, 0}};
    Mat4f PrevMvp = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};

    SceneState() = default;
    SceneState(const SceneState &) = delete;
    SceneState &operator=(const SceneState &) = delete;
    SceneState(SceneState &&) noexcept = default;
    SceneState &operator=(SceneState &&) noexcept = default;
  };

  static_assert(std::is_nothrow_move_constructible_v<FrameResources>);
  static_assert(std::is_nothrow_move_assignable_v<FrameResources>);
  static_assert(std::is_nothrow_move_constructible_v<WorldContent>);
  static_assert(std::is_nothrow_move_assignable_v<WorldContent>);
  static_assert(std::is_nothrow_move_constructible_v<SceneState>);
  static_assert(std::is_nothrow_move_assignable_v<SceneState>);

  bool Stands();
  [[nodiscard]] std::expected<void, std::string>
  InitForTarget(Extent frame, std::shared_ptr<const Compiled> plan, bool presents);
  [[nodiscard]] std::expected<void, std::string>
  StandsOffscreen(FrameResources &frame, const Compiled *plan, bool presents);
  [[nodiscard]] std::expected<OwnedTexture, std::string> MakeOffscreen(const Compiled *plan,
                                                                       Extent frame);
  [[nodiscard]] std::expected<SDL_GPUPresentMode, std::string> ClaimWindow(SDL_Window *window);

  SDL_GPUPresentMode Presenting_ = SDL_GPU_PRESENTMODE_VSYNC;
  SDL_Window *Showing_ = nullptr;
  GroundStorage GroundStorage_;
  bool Ready_ = false;
  std::string WhyNot_;

  static constexpr int kJitterPeriod = 8;

  struct Placed {
    double LeftPx = 0, TopPx = 0, WidthPx = 0, HeightPx = 0;
  };

  [[nodiscard]] Placed PictureRect() const;
  [[nodiscard]] Lens Through() const;
  std::array<SDL_GPUFence *, kFramesInFlight> Landed_ = {};
  int LandedAt_ = 0;
  SceneState State_;
};

}
#endif
