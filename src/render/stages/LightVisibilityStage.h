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


  // THE ATLAS IS CACHED, AND THE DECISION IS MADE BEFORE THE PASS OPENS. Whether the pass CLEARS
  // its depth or LOADS what the last cast left is the difference between a cached shadow and an
  // erased one, and a render pass decides that at `Begin`. So the stage is asked first and answers
  // for the frame.
  //
  // Measured: Shibuya's shadow pass draws the subject's 9.43 M triangles into a 2048-pixel atlas
  // EVERY frame and costs 17.7 ms of a 31.4 ms frame -- more than the picture itself. Over the two
  // kilometres it covers, a texel is about a metre, so it is drawing 2.2 triangles per texel. And
  // the sun moves 0.25 degrees a minute, which slides a fifty-metre building's shadow edge by
  // 0.22 m -- a QUARTER TEXEL a minute. The atlas was being redrawn some ten thousand times more
  // often than it changes.
  //
  // Unreal caches static shadow depths and RAGE caches its static cascade, for this reason.
  void Prepare(const FrameContext &ctx);

  [[nodiscard]] bool Casting() const { return Casting_; }
  [[nodiscard]] bool Cached() const { return Held_ && !Casting_; }

  void Encode(const FrameContext &ctx, const PassRecording &into);

  [[nodiscard]] static std::string DepthOnlySource();
  [[nodiscard]] static std::string DepthOnlySource(std::string &error);

  void Build(const double preView[3]);

  [[nodiscard]] const double *LightFromWorld() const { return LightFromWorld_; }
  [[nodiscard]] size_t CastBatches() const { return CastBatches_; }
  [[nodiscard]] const double *StoodAtM() const { return StoodAtM_; }
  [[nodiscard]] bool Standing() const { return Declared_; }

public:
  void CastsBelow(uint32_t slot) { CastsBelow_ = slot; }

private:
  uint32_t CastsBelow_ = 0xffffffffu;
  [[nodiscard]] bool ConfigureDepthOnly(const Gpu &gpu, std::string &error);
  void Cast(const double lightFromWorld16[16], const double preView[3], int atlasPx,
            const PassRecording &into);

  size_t CastBatches_ = 0;
  double StoodAtM_[3] = {0.0, 0.0, 0.0};
  SubjectDraw *Subjects_ = nullptr;
  OwnedPipeline DepthOnly_;
  double ToSun_[3] = {0, 0, 1};
  double Up_[3] = {0, 1, 0};
  double RadiusM_ = 0.0;
  double LightFromWorld_[16] = {};

  // THE MATRIX WITHOUT THE CAMERA IN IT, which is what the atlas's CONTENT depends on. The
  // rendering is camera-relative, so the pre-view shift moves the light matrix and the geometry by
  // the same amount and the depths that land in the atlas do not change. Keying the cache on
  // `LightFromWorld_` would re-cast on every step the eye takes for no reason at all.
  double Static_[16] = {};
  double CastFrom_[16] = {};
  uint64_t CastAt_ = 0;
  bool Held_ = false;
  bool Casting_ = true;
  bool Declared_ = false;
};

}
#endif
