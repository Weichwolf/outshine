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
#include "stages/TonemapStage.h"

namespace outshine::Render {

class Renderer {
public:

  void Init(int width, int height, std::shared_ptr<const RenderPlan> plan);
  [[nodiscard]] const RenderPlan &Plan(void) const { return *Plan_; }
  [[nodiscard]] bool DeviceUsable(void) const { return Ready; }

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
  void EncodePass(SDL_GPUCommandBuffer *commands, size_t pass);
  [[nodiscard]] SDL_GPUTexture *Target(Resource resource) const;
  [[nodiscard]] DisplayOptions Display(void) const;

  [[nodiscard]] SDL_GPUTexture *LinearSource(void) const;

  OwnedDevice Device_;

  SDL_GPUTexture *HostSurface_ = nullptr;
  std::shared_ptr<const RenderPlan> Plan_;
  Gpu Handles;
  OwnedTexture HdrTex, VelTex, DepthTex, FrameTex;

  OwnedTexture TransmissiveTex, CompositedTex;

  OwnedTexture ShadingNormalTex;

  OwnedTexture SurfaceIdentityTex;
  OwnedSampler Samp;
  SubjectDraw Subjects_;

  SubjectDraw Glass_;

  bool DrawsGlass_ = false;
  CompositeTransmissionStage CompositeTransmission_;
  TonemapStage Tonemap_;
  OverlayDraw Overlay_;
  PresentStage Present_;

  bool Ready = false;
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
