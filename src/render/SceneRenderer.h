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
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "FrameContext.h"
#include "Gpu.h"
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

struct Lens {
  double WidePx = 0;
  double HighPx = 0;
  float FovDeg = 0.0f;
  float OrthoM = 0.0f;
  float NearM = 0.0f;
  Vec2f Jitter = {{0.0f, 0.0f}};
};

struct PyramidDepths {
  float Nearest = 0.0f;
  float Farthest = 1.0f;
  float Mean = 0.0f;
};

class SceneRenderer {
public:
  void Init(Extent frame, std::shared_ptr<const Compiled> plan);

  [[nodiscard]] const Compiled &Plan() const { return *Plan_; }

  [[nodiscard]] bool DeviceUsable() const { return Ready_; }

  [[nodiscard]] const std::string &WhyNot() const { return WhyNot_; }

  struct Shown {
    int WidthPx = 0;
    int HeightPx = 0;
  };

  [[nodiscard]] std::expected<void, std::string_view>
  DrawsInto(int widthPx, int heightPx, SDL_Window *presents);
  [[nodiscard]] std::expected<std::optional<Shown>, std::string_view> Presented() const;
  void StopShowing();

  [[nodiscard]] SDL_GPUTextureFormat SurfaceFormat() const;

  void PresentInto(SDL_GPUTexture *surface) { HostSurface_ = surface; }

  struct Region {
    double X = 0.0;
    double Y = 0.0;
    double Width = 0.0;
    double Height = 0.0;
    double Aspect = 0.0;
  };

  void SetPictureRegion(Region into) {
    RegionX_ = into.X;
    RegionY_ = into.Y;
    RegionW_ = into.Width;
    RegionH_ = into.Height;
    RegionAspect_ = into.Aspect;
  }

  void RenderFrame();

  [[nodiscard]] bool Drew() const { return Submitted_; }

  ~SceneRenderer() { WaitForGpu(); }

  SceneRenderer() = default;
  SceneRenderer(const SceneRenderer &) = delete;
  SceneRenderer &operator=(const SceneRenderer &) = delete;

  void WaitForGpu();

  static constexpr int kFramesInFlight = 2;

  [[nodiscard]] int SettleFrames() const { return Plan_ ? Plan_->SettleFrames() : 1; }

  void WantsPixels();

  [[nodiscard]] bool Queued() const { return Presenting_ == SDL_GPU_PRESENTMODE_VSYNC; }

  [[nodiscard]] bool Presents() const { return Showing_ != nullptr; }

  void Settle() { SDL_WaitForGPUIdle(Device_.Get()); }

  [[nodiscard]] ReadState ReadPixels(std::vector<uint8_t> &rgba);

  [[nodiscard]] ReadState ReadDepth(std::vector<float> &depth);
  [[nodiscard]] static bool Executable(Stage stage);

  void CastsBelow(uint32_t slot) { Shadow_.CastsBelow(slot); }

  [[nodiscard]] ReadState ReadShadowAtlas(std::vector<float> &depth);
  static constexpr float kNearM = static_cast<float>(outshine::Scenario::Camera::kNearestM);

  [[nodiscard]] ReadState ReadSceneLinear(std::vector<float> &rgba);

  [[nodiscard]] ReadState ReadKeptIndices(KeptDraws &into);

  [[nodiscard]] PieceId PlacePiece(const PieceMesh &piece, std::string &error) {
    return Subjects_.PlacePiece(piece, error);
  }

  void ReleasePiece(PieceId which) { Subjects_.ReleasePiece(which); }

  [[nodiscard]] PageId PlaceHeightPage(std::span<const float> nodes, std::string &error) {
    return Subjects_.Ground().PlacePage(nodes, error);
  }

  void ReleaseHeightPage(PageId which) { Subjects_.Ground().ReleasePage(which); }

  [[nodiscard]] bool SetGroundGrid(std::span<const float> fractions, std::string &error) {
    return Subjects_.Ground().SetGrid(fractions, error);
  }

  [[nodiscard]] bool SetGroundLattice(std::span<const GroundTile> real,
                                      std::span<const GroundTile> virtual_,
                                      std::string &error) {
    return Subjects_.Ground().SetInstances(real, virtual_, error);
  }

  [[nodiscard]] uint32_t GroundLatticeTriangles() const { return Subjects_.Ground().Triangles(); }

  void WearPieces(std::span<const uint32_t> slotOfSurface) { Subjects_.WearPieces(slotOfSurface); }

  [[nodiscard]] uint32_t PiecesStanding() const { return Subjects_.PiecesStanding(); }

  [[nodiscard]] uint32_t PieceTriangles() const { return Subjects_.PieceTriangles(); }

  [[nodiscard]] uint32_t PieceBytesHeld() const { return Subjects_.Resident().HeldBytes(); }

  [[nodiscard]] ReadState ReadSkyIrradiance(std::span<float, kIrradianceFloats> out);

  [[nodiscard]] ReadState ReadPyramid(PyramidDepths &into);

  [[nodiscard]] ReadState ReadShadingNormal(std::vector<float> &xyz);

  [[nodiscard]] ReadState ReadSurfaceIdentity(std::vector<float> &slot);

  [[nodiscard]] ReadState ReadSceneVelocity(std::vector<float> &xy);

  [[nodiscard]] bool SetOverlay(const OverlayQuad *quads, size_t count, std::string &error) {
    return Overlay_.SetQuads(Handles_, quads, count, error);
  }

  [[nodiscard]] bool
  SetOverlayAtlas(const uint8_t *rgba, int width, int height, std::string &error) {
    return Overlay_.SetAtlas(Handles_, rgba, width, height, error);
  }

  [[nodiscard]] bool SetSubjectMesh(const SubjectMesh &mesh, std::string &error) {
    const Heap::Tagged relaying("mesh-relay");
    return Subjects_.SetMesh(mesh, error) && (!DrawsGlass_ || Glass_.SetMesh(mesh, error));
  }

  [[nodiscard]] bool SubjectPlacementRows(size_t rows, std::string &error) {
    return Subjects_.PlacementRows(rows, error) &&
           (!DrawsGlass_ || Glass_.PlacementRows(rows, error));
  }

  void MoveSubjectPlacement(size_t slot, const Mat4 &model) {
    Subjects_.MovePlacement(slot, model);
    if (DrawsGlass_) { Glass_.MovePlacement(slot, model); }
  }

  [[nodiscard]] bool HandSubjectPlacements(std::string &error) {
    return Subjects_.HandPlacements(false, error) &&
           (!DrawsGlass_ || Glass_.HandPlacements(false, error));
  }

  [[nodiscard]] size_t SubjectPlacementsMoved() const { return Subjects_.PlacementsMoved(); }

  [[nodiscard]] uint32_t SubjectBytesStaged() const { return Subjects_.StagedBytes(); }

  void ForgetSubjectStaging() { Subjects_.ForgetStagedCount(); }

  [[nodiscard]] const Vec3 &ShadowStoodAtM() const { return Shadow_.StoodAtM(); }

  [[nodiscard]] bool SetSubjectPlacements(const double *models, size_t rows, std::string &error) {
    return Subjects_.SetPlacements(models, rows, error) &&
           (!DrawsGlass_ || Glass_.SetPlacements(models, rows, error));
  }

  [[nodiscard]] bool SetSubjectPose(const SubjectPose &pose, std::string &error) {
    return Subjects_.SetPose(pose, error) && (!DrawsGlass_ || Glass_.SetPose(pose, error));
  }

  [[nodiscard]] bool SetSubjectMaterials(std::span<const SubjectMaterial> materials,
                                         std::string &error) {
    return Subjects_.SetMaterials(materials, error) &&
           (!DrawsGlass_ || Glass_.SetMaterials(materials, error));
  }

  [[nodiscard]] bool SetSubjectLights(std::span<const SubjectLight> lights, std::string &error) {
    return Subjects_.SetLights(lights, error) && (!DrawsGlass_ || Glass_.SetLights(lights, error));
  }

  void SetMedium(const Medium &medium) {
    Medium_ = medium;
    MediumTransmittance_.Declare(medium);
    MultiScatter_.Declare(medium);
    Radiance_.Declare(medium, CosSunZenith_, EyeHeightM_);
    SkyIrradianceStage_.Declare(medium, CosSunZenith_);
  }

  void SetShadowFrame(const Vec3f &toSun, const Vec3f &up, double radiusM) {
    Shadow_.Declare({.ToSun = toSun, .Up = up}, radiusM);
  }

  void SetSky(const Vec3f &toSun, const Vec3f &up, float illuminanceLux, float eyeHeightM) {
    CosSunZenith_ = toSun[0] * up[0] + toSun[1] * up[1] + toSun[2] * up[2];
    EyeHeightM_ = eyeHeightM;
    Radiance_.Declare(Medium_, CosSunZenith_, EyeHeightM_);
    SkyIrradianceStage_.Declare(Medium_, CosSunZenith_);
    const SkyStanding stands = {
        .SunDir = toSun, .WorldUp = up, .IlluminanceLux = illuminanceLux, .EyeHeightM = eyeHeightM};
    Sky_.Declare(Medium_, stands);
    Aerial_.Declare(Medium_, stands);
  }

  void SetSkyEye(float eyeHeightM) {
    if (!Sky_.Stands()) { return; }
    EyeHeightM_ = eyeHeightM;
    Radiance_.Declare(Medium_, CosSunZenith_, EyeHeightM_);
    SkyIrradianceStage_.Declare(Medium_, CosSunZenith_);
    Sky_.Eye(Medium_, eyeHeightM);
    Aerial_.Eye(Medium_, eyeHeightM);
  }

  [[nodiscard]] SDL_GPUTexture *SkyViewTable() const { return SkyViewLut_.Get(); }

  [[nodiscard]] SDL_GPUTexture *MultiScatterTable() const { return MultiScatterLut_.Get(); }

  [[nodiscard]] SDL_GPUTexture *TransmittanceTable() const { return TransmittanceLut_.Get(); }

  void SetSubjectEnvironment(const SubjectEnvironment &environment) {
    Subjects_.SetEnvironment(environment);
    if (DrawsGlass_) { Glass_.SetEnvironment(environment); }
  }

  [[nodiscard]] uint32_t SubjectBatchCount() const { return Subjects_.BatchCount(); }

  [[nodiscard]] uint32_t SubjectBatchesTaking(VertexLayout layout) const {
    uint32_t many = 0;
    for (const DrawBatch &batch : Subjects_.Drawn()) { many += batch.Layout == layout ? 1u : 0u; }
    return many;
  }

  [[nodiscard]] size_t ShadowCastCount() const { return Shadow_.CastBatches(); }

  [[nodiscard]] size_t ShadowedFrames() const { return Subjects_.ShadowedFrames(); }

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
    return Subjects_.UniformPushes() + Glass_.UniformPushes();
  }

  [[nodiscard]] float ExposureApplied() const { return Plan_ ? Plan_->Exposure() : 0.0f; }

  [[nodiscard]] uint32_t SubjectDrawCount() const { return Subjects_.DrawCount(); }

  [[nodiscard]] uint32_t SubjectPipelineCount() const { return Subjects_.PipelineCount(); }

  void SetCameraBasis(const CameraBasis &stands);

  void SetFovDeg(double deg) { FovDeg_ = deg > 0.0 ? static_cast<float>(deg) : FovDeg_; }

  [[nodiscard]] bool SetGroundClasses(const uint32_t *words,
                                      size_t wordCount,
                                      const float *palette,
                                      size_t paletteFloats,
                                      std::string &error);

  void SetOrthoM(double m) { OrthoM_ = static_cast<float>(m); }

  void SetNearM(double m) { NearM_ = m > 0.0 ? static_cast<float>(m) : NearM_; }

  [[nodiscard]] float NearMetres() const { return NearM_; }

  void BeginTemporalRun();

  [[nodiscard]] int SceneW() const { return Width_; }

  [[nodiscard]] int SceneH() const { return Height_; }

  [[nodiscard]] double PictureW() const;
  [[nodiscard]] double PictureH() const;

  [[nodiscard]] double SceneAspect() const {
    return PictureH() > 0 ? PictureW() / PictureH() : 0.0;
  }

private:
  std::array<Effort, kStageCount> Spent_ = {{}};

  void Create(Resource resource);
  [[nodiscard]] bool Configure(Stage stage, std::string &error);
  void EncodeStage(Stage stage, const PassRecording &into);

  struct Executor {
    Stage Named;
    bool (SceneRenderer::*Configure)(std::string &error);
    void (SceneRenderer::*Encode)(const FrameContext &ctx, const PassRecording &into);
  };

  static constexpr size_t kExecutorCount = 18;
  static const std::array<Executor, kExecutorCount> kExecutors;
  [[nodiscard]] static const Executor *ExecutorOf(Stage stage);
  void Picture(bool picture, const PassRecording &into);
  [[nodiscard]] bool ConfigureSubjects(std::string &error);
  [[nodiscard]] bool ConfigureGlass(std::string &error);
  [[nodiscard]] bool ConfigureCompositeTransmission(std::string &error);
  [[nodiscard]] bool ConfigureOverlay(std::string &error);
  [[nodiscard]] bool ConfigurePresent(std::string &error);
  [[nodiscard]] bool ConfigureTonemap(std::string &error);
  [[nodiscard]] bool ConfigureMediumTransmittance(std::string &error);
  [[nodiscard]] bool ConfigureMediumMultiScatter(std::string &error);
  [[nodiscard]] bool ConfigureMediumRadiance(std::string &error);

  [[nodiscard]] bool ConfigureIrradiance(std::string &error);

  [[nodiscard]] bool ConfigureDepthPyramid(std::string &error);
  [[nodiscard]] bool ConfigureSky(std::string &error);
  [[nodiscard]] bool ConfigureAerialPerspective(std::string &error);
  [[nodiscard]] bool ConfigureLightVisibility(std::string &error);
  [[nodiscard]] bool ConfigureSubjectCull(std::string &error);
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
  void EncodePass(SDL_GPUCommandBuffer *commands, size_t pass);
  [[nodiscard]] FrameContext Framing() const;
  void SettleShadow();
  std::array<bool, kResourceCount> Touched_ = {{}};
  [[nodiscard]] SDL_GPUTexture *Target(Resource resource) const;

  [[nodiscard]] SDL_GPUBuffer *BufferFor(Resource resource) const;
  [[nodiscard]] DisplayOptions Display() const;

  [[nodiscard]] SDL_GPUTexture *LinearSource() const;

  OwnedDevice Device_;

  SDL_GPUTexture *HostSurface_ = nullptr;
  bool Stands();
  std::expected<void, std::string_view> StandsOffscreen();

  SDL_GPUPresentMode Presenting_ = SDL_GPU_PRESENTMODE_VSYNC;
  SDL_Window *Showing_ = nullptr;
  Shown Shown_;
  bool Wanted_ = false;
  std::vector<uint8_t> Taken_;
  SDL_GPUTexture *Offscreen_ = nullptr;
  std::shared_ptr<const Compiled> Plan_;
  Gpu Handles_;
  OwnedTexture HdrTex_, VelTex_, DepthTex_, FrameTex_;
  OwnedTexture TransmittanceLut_, MultiScatterLut_, SkyViewLut_;
  OwnedTexture ShadowAtlas_;

  OwnedTexture TransmissiveTex_, CompositedTex_, AerialTex_;

  OwnedTexture ShadingNormalTex_;

  OwnedTexture SurfaceIdentityTex_;
  OwnedSampler Samp_, LutSamp_;
  SubjectDraw Subjects_;

  SubjectDraw Glass_;

  bool DrawsGlass_ = false;
  CompositeTransmissionStage CompositeTransmission_;
  AerialPerspectiveStage Aerial_;
  TonemapStage Tonemap_;
  MediumTransmittanceStage MediumTransmittance_;
  MediumMultiScatterStage MultiScatter_;
  MediumRadianceStage Radiance_;
  IrradianceStage SkyIrradianceStage_;
  DepthPyramidStage PyramidStage_;
  OwnedBuffer GroundClasses_;
  OwnedBuffer GroundPalette_;
  uint32_t GroundClassBytes_ = 0;
  uint32_t GroundPaletteBytes_ = 0;
  OwnedBuffer IrradianceBuffer_;
  OwnedBuffer Pyramid_;
  Readback PyramidRead_;
  SkyStage Sky_;
  LightVisibilityStage Shadow_;
  SubjectCullStage Cull_;
  Medium Medium_ = kEarthAir;
  float CosSunZenith_ = 1.0f;
  float EyeHeightM_ = 0.0f;
  OverlayDraw Overlay_;
  PresentStage Present_;

  bool Ready_ = false;
  std::string WhyNot_;
  int Width_ = 0, Height_ = 0;

  std::array<OwnedTexture, 2> LinearTex_{};
  int LinearAt_ = 0;
  bool HistoryHeld_ = false;

  bool HistoryStarted_ = false;

  static constexpr int kJitterPeriod = 8;
  int JitterAt_ = 0;
  Vec2f Jitter_ = {{0.0f, 0.0f}};
  Vec2f PrevJitter_ = {{0.0f, 0.0f}};
  bool CameraFull_ = false;

  double RegionX_ = 0, RegionY_ = 0, RegionW_ = 0, RegionH_ = 0, RegionAspect_ = 0;

  struct Placed {
    double LeftPx = 0, TopPx = 0, WidthPx = 0, HeightPx = 0;
  };

  [[nodiscard]] Placed PictureRect() const;
  [[nodiscard]] Lens Through() const;
  CameraBasis Camera_;
  float FovDeg_ = 0.0f;
  float OrthoM_ = 0.0f;
  float NearM_ = kNearM;

  bool Submitted_ = false;

  std::array<SDL_GPUFence *, kFramesInFlight> Landed_ = {};
  int LandedAt_ = 0;
  Vec3 PrevEye_ = {{0, 0, 0}};
  Mat4f PrevMvp_ = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
};

} // namespace outshine::Render
#endif
