#ifndef OUTSHINE_RENDER_SCENERENDERER_H
#define OUTSHINE_RENDER_SCENERENDERER_H

#include "Heap.h"
#include "Scenario.h"
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
#include "Compiled.h"
#include "stages/OverlayDraw.h"
#include "stages/PresentStage.h"
#include "stages/Resolve.h"
#include "stages/SubjectDraw.h"
#include "stages/AerialPerspectiveStage.h"
#include "stages/CompositeTransmissionStage.h"
#include "stages/MediumMultiScatterStage.h"
#include "stages/MediumRadianceStage.h"
#include "stages/LightVisibilityStage.h"
#include "stages/SubjectCullStage.h"
#include "stages/SkyStage.h"
#include "stages/MediumTransmittanceStage.h"
#include "stages/TonemapStage.h"

namespace outshine::Render {

class SceneRenderer {
public:

  void Init(int width, int height, std::shared_ptr<const Compiled> plan);
  [[nodiscard]] const Compiled &Plan() const { return *Plan_; }
  [[nodiscard]] bool DeviceUsable() const { return Ready_; }

  [[nodiscard]] const std::string &WhyNot() const { return WhyNot_; }

  struct Shown {
    int WidthPx = 0;
    int HeightPx = 0;
  };

  [[nodiscard]] std::expected<void, std::string_view> DrawsInto(int widthPx, int heightPx,
                                                                SDL_Window *presents);
  [[nodiscard]] std::expected<std::optional<Shown>, std::string_view> Presented() const;
  void StopShowing();

  [[nodiscard]] SDL_GPUTextureFormat SurfaceFormat() const;

  void PresentInto(SDL_GPUTexture *surface) { HostSurface_ = surface; }


  void SetPictureRegion(double x, double y, double width, double height, double aspect = 0.0) {
    RegionX_ = x;
    RegionY_ = y;
    RegionW_ = width;
    RegionH_ = height;
    RegionAspect_ = aspect;
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

  // WHETHER THIS DEVICE HAS A WINDOW TO SHOW A FRAME IN. Headless is the fast path rather than a
  // degraded one, so the answer is a fact about the target and never a complaint about it.
  [[nodiscard]] bool Presents() const { return Showing_ != nullptr; }

  void Settle() { SDL_WaitForGPUIdle(Device_.Get()); }
  [[nodiscard]] ReadState ReadPixels(std::vector<uint8_t> &rgba);

  [[nodiscard]] ReadState ReadDepth(std::vector<float> &depth);
  [[nodiscard]] static bool Executable(Stage stage);
  void CastsBelow(uint32_t slot) {
    Shadow_.CastsBelow(slot);
    Subjects_.TracesBelow(slot);
    Glass_.TracesBelow(slot);
  }
  [[nodiscard]] ReadState ReadShadowAtlas(std::vector<float> &depth);
  static constexpr float kNearM = (float)outshine::Camera::kNearestM;

  [[nodiscard]] ReadState ReadSceneLinear(std::vector<float> &rgba);

  [[nodiscard]] ReadState ReadShadingNormal(std::vector<float> &xyz);

  [[nodiscard]] ReadState ReadSurfaceIdentity(std::vector<float> &slot);

  [[nodiscard]] ReadState ReadSceneVelocity(std::vector<float> &xy);

  [[nodiscard]] bool SetOverlay(const OverlayQuad *quads, size_t count, std::string &error) {
    return Overlay_.SetQuads(Handles_, quads, count, error);
  }

  [[nodiscard]] bool SetOverlayAtlas(const uint8_t *rgba, int width, int height, std::string &error) {
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

  void MoveSubjectPlacement(size_t slot, const double model16[16]) {
    Subjects_.MovePlacement(slot, model16);
    if (DrawsGlass_) { Glass_.MovePlacement(slot, model16); }
  }

  [[nodiscard]] bool HandSubjectPlacements(std::string &error) {
    return Subjects_.HandPlacements(false, error) && (!DrawsGlass_ || Glass_.HandPlacements(false, error));
  }

  [[nodiscard]] size_t SubjectPlacementsMoved() const { return Subjects_.PlacementsMoved(); }

  // BYTES THE CPU HANDED THE GPU. RAGE and Unreal both drive this toward zero on a steady frame:
  // geometry is resident and only what CHANGED crosses the bus. A number that stays high while
  // nothing moves is the finding, not the frame time it produces.
  [[nodiscard]] uint32_t SubjectBytesStaged() const { return Subjects_.StagedBytes(); }
  void ForgetSubjectStaging() { Subjects_.ForgetStagedCount(); }
  [[nodiscard]] const double *ShadowStoodAtM() const { return Shadow_.StoodAtM(); }

  [[nodiscard]] bool SetSubjectPlacements(const double *models, size_t rows, std::string &error) {
    return Subjects_.SetPlacements(models, rows, error) &&
           (!DrawsGlass_ || Glass_.SetPlacements(models, rows, error));
  }

  [[nodiscard]] bool SetSubjectPose(const SubjectPose &pose, std::string &error) {
    return Subjects_.SetPose(pose, error) && (!DrawsGlass_ || Glass_.SetPose(pose, error));
  }

  [[nodiscard]] bool SetSubjectMaterials(std::span<const SubjectMaterial> materials,
                                         std::string &error) {
    return Subjects_.SetMaterials(materials, error) && (!DrawsGlass_ || Glass_.SetMaterials(materials, error));
  }

  [[nodiscard]] bool SetSubjectLights(std::span<const SubjectLight> lights, std::string &error) {
    return Subjects_.SetLights(lights, error) && (!DrawsGlass_ || Glass_.SetLights(lights, error));
  }

  void SetMedium(const Medium &medium) {
    Medium_ = medium;
    MediumTransmittance_.Declare(medium);
    MultiScatter_.Declare(medium);
    Radiance_.Declare(medium, CosSunZenith_, EyeHeightM_);
  }

  void SetShadowFrame(const float toSun[3], const float up[3], double radiusM) {
    Shadow_.Declare(toSun, up, radiusM);
  }

  void SetSky(const float toSun[3], const float up[3], float illuminanceLux, float eyeHeightM) {
    CosSunZenith_ = toSun[0] * up[0] + toSun[1] * up[1] + toSun[2] * up[2];
    EyeHeightM_ = eyeHeightM;
    Radiance_.Declare(Medium_, CosSunZenith_, EyeHeightM_);
    Sky_.Declare(Medium_, toSun, up, illuminanceLux, eyeHeightM);
    Aerial_.Declare(Medium_, toSun, up, illuminanceLux, eyeHeightM);
  }

  void SetSkyEye(float eyeHeightM) {
    if (!Sky_.Stands()) { return; }
    EyeHeightM_ = eyeHeightM;
    Radiance_.Declare(Medium_, CosSunZenith_, EyeHeightM_);
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

  // WHICH VERTEX LAYOUT EACH BATCH TOOK, which decides which SHADER VARIANT drew it. Nothing could
  // read this, so a picture that disagreed with a reference could be argued about for a session
  // without anyone checking whether the draw took the variant the code says it takes -- which is
  // exactly what happened. A number that decides a picture and cannot be read is a number that
  // gets reasoned about instead of measured.
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

  [[nodiscard]] const Effort &Spent(Stage stage) const { return Spent_[(size_t)stage]; }

  [[nodiscard]] size_t SubjectUniformPushes() const {
    return Subjects_.UniformPushes() + Glass_.UniformPushes();
  }
  [[nodiscard]] float ExposureApplied() const { return Plan_ ? Plan_->Exposure() : 0.0f; }

  [[nodiscard]] uint32_t SubjectDrawCount() const { return Subjects_.DrawCount(); }

  [[nodiscard]] uint32_t SubjectPipelineCount() const { return Subjects_.PipelineCount(); }

  [[nodiscard]] float ShadowRayNearM() const { return Subjects_.ShadowNearM(); }

  void SetCameraBasis(const double eye[3], const double fwd[3], const double right[3],
                      const double up[3]);

  void SetFovDeg(double deg) { FovDeg_ = deg > 0.0 ? (float)deg : FovDeg_; }
  void SetOrthoM(double m) { OrthoM_ = (float)m; }

  void SetNearM(double m) { NearM_ = m > 0.0 ? (float)m : NearM_; }
  [[nodiscard]] float NearMetres() const { return NearM_; }

  void BeginTemporalRun();

  [[nodiscard]] int SceneW() const { return Width_; }
  [[nodiscard]] int SceneH() const { return Height_; }
  [[nodiscard]] double PictureW() const;
  [[nodiscard]] double PictureH() const;

  [[nodiscard]] double SceneAspect() const {
    return PictureH() > 0 ? (double)PictureW() / (double)PictureH() : 0.0;
  }


private:
  Effort Spent_[kStageCount] = {};


  void Create(Resource resource);
  [[nodiscard]] bool Configure(Stage stage, std::string &error);
  void EncodeStage(Stage stage, const PassRecording &into);

  struct Executor {
    Stage Named;
    bool (SceneRenderer::*Configure)(std::string &error);
    void (SceneRenderer::*Encode)(const FrameContext &ctx, const PassRecording &into);
  };
  static const Executor kExecutors[];
  static const size_t kExecutorCount;
  [[nodiscard]] static const Executor *ExecutorOf(Stage stage);
  void Picture(bool picture, const PassRecording &into);
  [[nodiscard]] bool ConfigureSubjects(std::string &error);
  [[nodiscard]] bool ConfigureGlass(std::string &error);
  [[nodiscard]] bool ConfigureCompositeTransmission(std::string &error);
  [[nodiscard]] bool ConfigureTemporalResolve(std::string &error);
  [[nodiscard]] bool ConfigureOverlay(std::string &error);
  [[nodiscard]] bool ConfigurePresent(std::string &error);
  [[nodiscard]] bool ConfigureTonemap(std::string &error);
  [[nodiscard]] bool ConfigureMediumTransmittance(std::string &error);
  [[nodiscard]] bool ConfigureMediumMultiScatter(std::string &error);
  [[nodiscard]] bool ConfigureMediumRadiance(std::string &error);
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
  void EncodeSky(const FrameContext &ctx, const PassRecording &into);
  void EncodeAerialPerspective(const FrameContext &ctx, const PassRecording &into);
  void EncodeLightVisibility(const FrameContext &ctx, const PassRecording &into);
  void EncodeSubjectCull(const FrameContext &ctx, const PassRecording &into);
  void EncodePass(SDL_GPUCommandBuffer *commands, size_t pass);
  [[nodiscard]] FrameContext Framing() const;
  void SettleShadow();
  bool Touched_[kResourceCount] = {};
  [[nodiscard]] SDL_GPUTexture *Target(Resource resource) const;

  // A TABLE IS RESOLVED THE WAY A PICTURE IS, and by a separate answer because the device
  // binds the two by separate calls. A resource answers exactly one of the two.
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
  SkyStage Sky_;
  LightVisibilityStage Shadow_;
  SubjectCullStage Cull_;
  Medium Medium_;
  float CosSunZenith_ = 1.0f;
  float EyeHeightM_ = 0.0f;
  OverlayDraw Overlay_;
  PresentStage Present_;

  bool Ready_ = false;
  std::string WhyNot_;
  int Width_ = 0, Height_ = 0;

  OwnedTexture LinearTex_[2];
  int LinearAt_ = 0;
  bool HistoryHeld_ = false;

  bool HistoryStarted_ = false;

  static constexpr int kJitterPeriod = 8;
  int JitterAt_ = 0;
  float Jitter_[2] = {0.0f, 0.0f};
  float PrevJitter_[2] = {0.0f, 0.0f};
  bool CameraFull_ = false;

  double RegionX_ = 0, RegionY_ = 0, RegionW_ = 0, RegionH_ = 0, RegionAspect_ = 0;

  struct Placed {
    double LeftPx = 0, TopPx = 0, WidthPx = 0, HeightPx = 0;
  };
  [[nodiscard]] Placed PictureRect() const;
  double Eye_[3] = {0, 0, 0};
  double Fwd_[3] = {0, 0, 0}, Right_[3] = {0, 0, 0}, Up_[3] = {0, 0, 0};
  float FovDeg_ = 60.0f;
  float OrthoM_ = 0.0f;
  float NearM_ = kNearM;

  bool Submitted_ = false;

  SDL_GPUFence *Landed_[kFramesInFlight] = {};
  int LandedAt_ = 0;
  double PrevEye_[3] = {0, 0, 0};
  float PrevMvp16_[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

}
#endif
