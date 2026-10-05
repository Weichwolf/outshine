#ifndef OUTSHINE_RENDER_STAGES_LIGHTVISIBILITYSTAGE_H
#define OUTSHINE_RENDER_STAGES_LIGHTVISIBILITYSTAGE_H

#include <array>
#include <string>

#include "math/Mat4.h"
#include "math/Vec3.h"
#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "StageSubmission.h"
#include "ShadowRegionUniform.h"

namespace outshine::Render {

constexpr uint32_t kNoBatch = 0xffffffffu;

class SubjectDraw;

class LightVisibilityStage {
public:
  [[nodiscard]] bool Configure(SubjectDraw &subjects, const Gpu &gpu, std::string &error);

  void Binds(SubjectDraw &subjects) noexcept { Subjects_ = &subjects; }

  void Invalidate() noexcept { Cache_.Invalidate(); }

  struct Overhead {
    Vec3f ToSun;
    Vec3f Up;
  };

  void Declare(Overhead sky, double radiusM, bool cameraCentred = false);

  void Prepare(const FrameContext &ctx);

  [[nodiscard]] bool Casting() const { return Casting_; }

  [[nodiscard]] bool Cached() const noexcept { return Cache_.Submitted() && !Casting_; }

  [[nodiscard]] bool HasSubmittedData() const noexcept { return Cache_.Submitted(); }

  void Encode(const FrameContext &ctx, const PassRecording &into);

  void Build(const Vec3 &preView);

  [[nodiscard]] const Mat4 &LightFromWorld() const { return LightFromWorld_; }

  [[nodiscard]] const std::array<ShadowRegionUniform, kSunShadowRegions> &Regions() const {
    return RegionUniforms_;
  }

  [[nodiscard]] size_t RegionCount() const { return CameraCentred_ ? kSunShadowRegions : 1; }

  [[nodiscard]] const Mat4 &RegionProjection(size_t region) const { return Projections_[region]; }

  [[nodiscard]] size_t CastBatches() const { return CastBatches_; }

  [[nodiscard]] const Vec3 &StoodAtM() const { return StoodAtM_; }

  [[nodiscard]] bool Standing() const { return Declared_; }

  void CastsBelow(uint32_t slot) noexcept {
    if (CastsBelow_ == slot) { return; }
    CastsBelow_ = slot;
    Cache_.Invalidate();
  }

private:
  [[nodiscard]] Vec3 CasterCentre() const;
  uint32_t CastsBelow_ = kNoBatch;
  [[nodiscard]] bool ConfigureDepthOnly(const Gpu &gpu, std::string &error);

  struct LightBasis {
    Vec3 Right;
    Vec3 Upward;
    Vec3 Forward;
  };

  void
  BuildRegions(const LightBasis &basis, const Vec3 &centre, double radiusM, const Vec3 &preView);
  void
  Cast(const Mat4 &lightFromWorld, const Vec3 &preView, size_t region, const PassRecording &into);

  size_t CastBatches_ = 0;
  Vec3 StoodAtM_;
  SubjectDraw *Subjects_ = nullptr;
  OwnedPipeline DepthOnly_;
  Vec3 ToSun_ = {{0, 0, 1}};
  Vec3 Up_ = {{0, 1, 0}};
  double RadiusM_ = 0.0;
  Mat4 LightFromWorld_ = {{}};

  std::array<Mat4, kSunShadowRegions> Projections_{};
  std::array<Mat4, kSunShadowRegions> Static_{};
  std::array<Mat4, kSunShadowRegions> PreparedTransform_{};
  std::array<ShadowRegionUniform, kSunShadowRegions> RegionUniforms_{};
  bool CameraCentred_ = false;
  uint64_t PreparedGeneration_ = 0;
  uint64_t PreparedGroundGeneration_ = 0;
  StageCache Cache_;
  bool Casting_ = true;
  bool Declared_ = false;
};

}
#endif
