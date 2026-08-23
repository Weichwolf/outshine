#ifndef OUTSHINE_RENDER_STAGES_LIGHTVISIBILITYSTAGE_H
#define OUTSHINE_RENDER_STAGES_LIGHTVISIBILITYSTAGE_H

#include <string>

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"

namespace outshine::Render {

class SubjectDraw;

inline constexpr int kShadowAtlasPx = 2048;

class LightVisibilityStage {
public:
  [[nodiscard]] bool Configure(SubjectDraw &subjects, const Gpu &gpu, std::string &error);

  void Declare(const float toSun[3], const float up[3], double radiusM);

  void Frame(const double centreM[3]);

  void Encode(const FrameContext &ctx, const PassRecording &into);

  void Build(const double eye[3]);

  [[nodiscard]] const double *LightFromWorld() const { return LightFromWorld_; }
  [[nodiscard]] bool Standing() const { return Declared_; }

private:
  SubjectDraw *Subjects_ = nullptr;
  double ToSun_[3] = {0, 0, 1};
  double Up_[3] = {0, 1, 0};
  double RadiusM_ = 0.0;
  double CentreM_[3] = {0, 0, 0};
  double LightFromWorld_[16] = {};
  bool Declared_ = false;
};

}
#endif
