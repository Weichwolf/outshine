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


  void Encode(const FrameContext &ctx, const PassRecording &into);

  [[nodiscard]] static std::string DepthOnlySource();
  [[nodiscard]] static std::string DepthOnlySource(std::string &error);

  void Build(const double eye[3]);

  [[nodiscard]] const double *LightFromWorld() const { return LightFromWorld_; }
  [[nodiscard]] size_t CastBatches() const { return CastBatches_; }
  [[nodiscard]] bool Standing() const { return Declared_; }

public:
  void CastsBelow(uint32_t slot) { CastsBelow_ = slot; }

private:
  uint32_t CastsBelow_ = 0xffffffffu;
  [[nodiscard]] bool ConfigureDepthOnly(const Gpu &gpu, std::string &error);
  void Cast(const double lightFromWorld16[16], const double eye[3], int atlasPx,
            const PassRecording &into);

  size_t CastBatches_ = 0;
  SubjectDraw *Subjects_ = nullptr;
  OwnedPipeline DepthOnly_;
  double ToSun_[3] = {0, 0, 1};
  double Up_[3] = {0, 1, 0};
  double RadiusM_ = 0.0;
  double LightFromWorld_[16] = {};
  bool Declared_ = false;
};

}
#endif
