#ifndef OUTSHINE_RENDER_STAGES_LIGHTVISIBILITYSTAGE_H
#define OUTSHINE_RENDER_STAGES_LIGHTVISIBILITYSTAGE_H

#include <string>

#include "math/Vec3.h"
#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"

namespace outshine::Render {

class SubjectDraw;

inline constexpr int kShadowAtlasPx = 2048;

class LightVisibilityStage {
public:
  [[nodiscard]] bool Configure(SubjectDraw &subjects, const Gpu &gpu, std::string &error);

  void Declare(const Vec3f &toSun, const Vec3f &up, double radiusM);

  void Prepare(const FrameContext &ctx);

  [[nodiscard]] bool Casting() const { return Casting_; }

  [[nodiscard]] bool Cached() const { return Held_ && !Casting_; }

  void Encode(const FrameContext &ctx, const PassRecording &into);

  [[nodiscard]] static std::string DepthOnlySource();
  [[nodiscard]] static std::string DepthOnlySource(std::string &error);

  void Build(const Vec3 &preView);

  [[nodiscard]] const double *LightFromWorld() const { return LightFromWorld_; }

  [[nodiscard]] size_t CastBatches() const { return CastBatches_; }

  [[nodiscard]] const Vec3 &StoodAtM() const { return StoodAtM_; }

  [[nodiscard]] bool Standing() const { return Declared_; }

public:
  void CastsBelow(uint32_t slot) { CastsBelow_ = slot; }

private:
  uint32_t CastsBelow_ = 0xffffffffu;
  [[nodiscard]] bool ConfigureDepthOnly(const Gpu &gpu, std::string &error);
  void Cast(const double lightFromWorld[16],
            const Vec3 &preView,
            int atlasPx,
            const PassRecording &into);

  size_t CastBatches_ = 0;
  Vec3 StoodAtM_;
  SubjectDraw *Subjects_ = nullptr;
  OwnedPipeline DepthOnly_;
  Vec3 ToSun_ = {{0, 0, 1}};
  Vec3 Up_ = {{0, 1, 0}};
  double RadiusM_ = 0.0;
  double LightFromWorld_[16] = {};

  double Static_[16] = {};
  double CastFrom_[16] = {};
  uint64_t CastAt_ = 0;
  bool Held_ = false;
  bool Casting_ = true;
  bool Declared_ = false;
};

} // namespace outshine::Render
#endif
