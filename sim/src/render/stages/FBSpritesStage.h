/* The draw slot for effect billboards — engine flame, rocket plume, flares, chaff, detonations, fire,
 * lamps — wired into the scene pass right before the HUD. ONE instanced draw for the whole frame's
 * effects, no pass of its own and none added.
 *
 * Screen-space stretched billboards, six analytic fragment profiles, no texture at all. Turbulence is
 * value noise over a hash (Dave Hoskins, "Hash without Sine", 2014, shadertoy XdGfRR / jcgt.org).
 * Deviation: three octaves, not four — doc/render/visual-target.md §1 buys ALU, not bandwidth, and the
 * fourth octave is below a pixel at every range a detonation is seen from.
 *
 * COSTS NOTHING WHEN NOTHING IS THERE: with no sprite list or no device the whole Encode() returns
 * before it touches the queue, so a world without effects is byte-identical to the picture before this
 * stage drew anything. The pass count is FBRenderer's and is untouched.
 * Vertrag + Messung: doc/render/units-visual.md. */
#ifndef FBSPRITESSTAGE_H
#define FBSPRITESSTAGE_H

#include <vector>
#include "FBDrawStage.h"
#include "FBCloudDensity.h"
#include "FBSpriteDraw.h"

namespace FlightBox::Render {

class FBSpritesStage : public FBDrawStage {
public:
  /* Must run AFTER FBRenderer::CreateAtmosphere — smoke reads the same sky-view LUT the terrain and the
   * airframes read, so a distant trail fades into the same air as the mountain behind it. */
  void Configure(const FBGpu &gpu, wgpu::Sampler lutSamp, wgpu::TextureView skyLutView,
                 wgpu::Buffer atmoBuf);

  /* Borrowed for exactly one frame — FBWorld rewrites its vector every Update(). */
  void SetSprites(const FBSpriteDraw *s, int count) { Sprites = s; Count = s ? count : 0; }

  void SetSky(const FBCloudSky &sky) { Sky = sky; }

  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  int LastDraws() const { return LastDraws_; }

private:
  static constexpr int kUniFloats = 36;    /* mat4 + viewport + haze + three decks — FBUnitsStage's `U` shape */
  static constexpr int kMaxSprites = 512;  /* instance-buffer slots; sprites past this are dropped */
  static constexpr int kInstFloats = 16;
  static constexpr int kSpriteKinds = 6;   /* FBSpriteKind::Count would be the seventh name for six */
  static constexpr unsigned kSpriteLogEvery = 300;

  /* Logs on the periodic cadence AND whenever the CAST CHANGES — an effect appearing or going out is
   * exactly the event a frame proof wants to be able to point at, and a fixed cadence misses it. */
  void LogCast(const FBFrameContext &ctx);

  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::Sampler LutSamp;
  wgpu::TextureView SkyLutView;
  wgpu::Buffer AtmoBuf, Uni, Inst;
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Frame;
  std::vector<float> InstScratch;   /* kMaxSprites * kInstFloats, reused: no per-frame heap */
  std::vector<int> Order;           /* far-to-near indices; likewise reused */

  const FBSpriteDraw *Sprites = nullptr;
  int Count = 0;
  int LastDraws_ = 0;
  int LastKindCount_[kSpriteKinds] = {-1, -1, -1, -1, -1, -1};
  bool Ready = false;
  FBCloudSky Sky;
};

} // namespace FlightBox::Render
#endif
