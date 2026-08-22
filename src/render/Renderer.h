#ifndef RENDERER_H
#define RENDERER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "Readback.h"
#include "RenderPlan.h"
#include "stages/OverlayDraw.h"
#include "stages/PresentStage.h"
#include "stages/Resolve.h"
#include "stages/SubjectDraw.h"
#include "stages/CompositeTransmissionStage.h"
#include "stages/MediumMultiScatterStage.h"
#include "stages/MediumRadianceStage.h"
#include "stages/LightVisibilityStage.h"
#include "stages/SkyStage.h"
#include "stages/MediumTransmittanceStage.h"
#include "stages/TonemapStage.h"

namespace outshine::Render {

class Renderer {
public:

  void Init(int width, int height, std::shared_ptr<const RenderPlan> plan);
  [[nodiscard]] const RenderPlan &Plan(void) const { return *Plan_; }
  [[nodiscard]] bool DeviceUsable(void) const { return Ready; }

  [[nodiscard]] const std::string &WhyNot(void) const { return WhyNot_; }

  [[nodiscard]] SDL_GPUDevice *Device(void) const { return Device_.Get(); }

  [[nodiscard]] SDL_GPUTextureFormat SurfaceFormat(void) const;

  void PresentInto(SDL_GPUTexture *surface) { HostSurface_ = surface; }

  void SetPictureRegion(double x, double y, double width, double height, double aspect = 0.0) {
    RegionX_ = x;
    RegionY_ = y;
    RegionW_ = width;
    RegionH_ = height;
    RegionAspect_ = aspect;
  }

  void RenderFrame(void);

  ~Renderer(void) { WaitForGpu(); }
  Renderer(void) = default;
  Renderer(const Renderer &) = delete;
  Renderer &operator=(const Renderer &) = delete;

  void WaitForGpu(void);

  static constexpr int kFramesInFlight = 2;

  [[nodiscard]] int SettleFrames(void) const { return Plan_ ? Plan_->SettleFrames() : 1; }

  [[nodiscard]] ReadState ReadPixels(std::vector<uint8_t> &rgba);

  [[nodiscard]] ReadState ReadDepth(std::vector<float> &depth);
  [[nodiscard]] ReadState ReadShadowAtlas(std::vector<float> &depth);
  static constexpr float kNearM = 0.05f;

  [[nodiscard]] ReadState ReadSceneLinear(std::vector<float> &rgba);

  [[nodiscard]] ReadState ReadShadingNormal(std::vector<float> &xyz);

  [[nodiscard]] ReadState ReadSurfaceIdentity(std::vector<float> &slot);

  [[nodiscard]] ReadState ReadSceneVelocity(std::vector<float> &xy);

  [[nodiscard]] bool SetOverlay(const OverlayQuad *quads, size_t count, std::string &error) {
    return Overlay_.SetQuads(Handles, quads, count, error);
  }

  [[nodiscard]] bool SetOverlayAtlas(const uint8_t *rgba, int width, int height, std::string &error) {
    return Overlay_.SetAtlas(Handles, rgba, width, height, error);
  }

  [[nodiscard]] bool SetSubjectMesh(const SubjectMesh &mesh, std::string &error) {
    return Subjects_.SetMesh(mesh, error) && (!DrawsGlass_ || Glass_.SetMesh(mesh, error));
  }

  [[nodiscard]] bool SetSubjectPlacements(const double *models, size_t rows, std::string &error) {
    return Subjects_.SetPlacements(models, rows, error) &&
           (!DrawsGlass_ || Glass_.SetPlacements(models, rows, error));
  }

  [[nodiscard]] bool SetSubjectPose(const SubjectPose &pose, std::string &error) {
    return Subjects_.SetPose(pose, error) && (!DrawsGlass_ || Glass_.SetPose(pose, error));
  }

  [[nodiscard]] bool SetSubjectMaterials(const std::vector<SubjectMaterial> &materials,
                                         std::string &error) {
    return Subjects_.SetMaterials(materials, error) && (!DrawsGlass_ || Glass_.SetMaterials(materials, error));
  }

  [[nodiscard]] bool SetSubjectLights(const std::vector<SubjectLight> &lights, std::string &error) {
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

  void ShadowCentre(const double centreM[3]) { Shadow_.Frame(centreM); }

  void SetSky(const float toSun[3], const float up[3], float illuminanceLux, float eyeHeightM) {
    CosSunZenith_ = toSun[0] * up[0] + toSun[1] * up[1] + toSun[2] * up[2];
    EyeHeightM_ = eyeHeightM;
    Radiance_.Declare(Medium_, CosSunZenith_, EyeHeightM_);
    Sky_.Declare(Medium_, toSun, up, illuminanceLux, eyeHeightM);
  }

  [[nodiscard]] SDL_GPUTexture *SkyViewTable(void) const { return SkyViewLut_.Get(); }

  [[nodiscard]] SDL_GPUTexture *MultiScatterTable(void) const { return MultiScatterLut_.Get(); }

  [[nodiscard]] SDL_GPUTexture *TransmittanceTable(void) const { return TransmittanceLut_.Get(); }

  void SetSubjectEnvironment(const SubjectEnvironment &environment) {
    Subjects_.SetEnvironment(environment);
    if (DrawsGlass_) { Glass_.SetEnvironment(environment); }
  }
  [[nodiscard]] uint32_t SubjectBatchCount(void) const { return Subjects_.BatchCount(); }
  [[nodiscard]] uint32_t SubjectDrawCount(void) const { return Subjects_.DrawCount(); }

  [[nodiscard]] uint32_t SubjectPipelineCount(void) const { return Subjects_.PipelineCount(); }

  [[nodiscard]] float ShadowRayNearM(void) const { return Subjects_.ShadowNearM(); }

  void SetCameraBasis(const double eye[3], const double fwd[3], const double right[3],
                      const double up[3]);

  void SetFovDeg(double deg) { FovDeg = deg > 0.0 ? (float)deg : FovDeg; }
  void SetOrthoM(double m) { OrthoM = (float)m; }

  void SetNearM(double m) { NearM = m > 0.0 ? (float)m : NearM; }
  [[nodiscard]] float NearMetres(void) const { return NearM; }

  void BeginTemporalRun(void);

  [[nodiscard]] int SceneW(void) const { return Width; }
  [[nodiscard]] int SceneH(void) const { return Height; }
  [[nodiscard]] double PictureW(void) const;
  [[nodiscard]] double PictureH(void) const;

  [[nodiscard]] double SceneAspect(void) const {
    return PictureH() > 0 ? (double)PictureW() / (double)PictureH() : 0.0;
  }

private:

  [[nodiscard]] static bool Executable(Stage stage);
  void Create(Resource resource);
  [[nodiscard]] bool Configure(Stage stage, std::string &error);
  void EncodeStage(Stage stage, const PassRecording &into);

  struct Executor {
    Stage Named;
    bool (Renderer::*Configure)(std::string &error);
    void (Renderer::*Encode)(const FrameContext &ctx, const PassRecording &into);
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
  [[nodiscard]] bool ConfigureLightVisibility(std::string &error);
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
  void EncodeLightVisibility(const FrameContext &ctx, const PassRecording &into);
  void EncodePass(SDL_GPUCommandBuffer *commands, size_t pass);
  bool Touched_[kResourceCount] = {};
  [[nodiscard]] SDL_GPUTexture *Target(Resource resource) const;
  [[nodiscard]] DisplayOptions Display(void) const;

  [[nodiscard]] SDL_GPUTexture *LinearSource(void) const;

  OwnedDevice Device_;

  SDL_GPUTexture *HostSurface_ = nullptr;
  std::shared_ptr<const RenderPlan> Plan_;
  Gpu Handles;
  OwnedTexture HdrTex, VelTex, DepthTex, FrameTex;
  OwnedTexture TransmittanceLut_, MultiScatterLut_, SkyViewLut_;
  OwnedTexture ShadowAtlas_;

  OwnedTexture TransmissiveTex, CompositedTex;

  OwnedTexture ShadingNormalTex;

  OwnedTexture SurfaceIdentityTex;
  OwnedSampler Samp, LutSamp;
  SubjectDraw Subjects_;

  SubjectDraw Glass_;

  bool DrawsGlass_ = false;
  CompositeTransmissionStage CompositeTransmission_;
  TonemapStage Tonemap_;
  MediumTransmittanceStage MediumTransmittance_;
  MediumMultiScatterStage MultiScatter_;
  MediumRadianceStage Radiance_;
  SkyStage Sky_;
  LightVisibilityStage Shadow_;
  Medium Medium_;
  float CosSunZenith_ = 1.0f;
  float EyeHeightM_ = 0.0f;
  OverlayDraw Overlay_;
  PresentStage Present_;

  bool Ready = false;
  std::string WhyNot_;
  int Width = 0, Height = 0;

  OwnedTexture LinearTex_[2];
  int LinearAt_ = 0;
  bool HistoryHeld_ = false;

  bool HistoryStarted_ = false;

  static constexpr int kJitterPeriod = 8;
  int JitterAt_ = 0;
  float Jitter_[2] = {0.0f, 0.0f};
  float PrevJitter_[2] = {0.0f, 0.0f};
  bool CameraFull = false;

  double RegionX_ = 0, RegionY_ = 0, RegionW_ = 0, RegionH_ = 0, RegionAspect_ = 0;

  struct Placed {
    double LeftPx = 0, TopPx = 0, WidthPx = 0, HeightPx = 0;
  };
  [[nodiscard]] Placed PictureRect(void) const;
  double Eye[3] = {0, 0, 0};
  double Fwd[3] = {0, 0, 0}, Right[3] = {0, 0, 0}, Up[3] = {0, 0, 0};
  float FovDeg = 60.0f;
  float OrthoM = 0.0f;
  float NearM = kNearM;

  bool Submitted = false;

  SDL_GPUFence *Landed_[kFramesInFlight] = {};
  int LandedAt_ = 0;
  double PrevEye[3] = {0, 0, 0};
  float PrevMvp16[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

}
#endif
