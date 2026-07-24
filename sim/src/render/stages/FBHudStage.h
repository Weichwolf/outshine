/* FlightBox — FBHudStage: the MIL-STD-1787 HUD's WebGPU backend. THE stage that reuses the WebGL
 * symbology (FBHud.h -> w3_build_hud) — the only translation unit that includes FBHud.h (its globals
 * are file-static; a second TU including it would silently get its OWN empty copy). Three pipelines
 * share one pixel->NDC map: Solid (AA triangles, the conformal horizon), Line (rails/ticks/carets —
 * the SAME shader as Solid, a second pipeline over the same bind group, just a different primitive
 * topology), Text (MAX7456 atlas glyph quads, alpha-tested). Also owns the boot/teleport loading-
 * screen text draw (a second, unrelated use of the Text pipeline).
 *
 * FBRenderer keeps the actual telemetry pose (FBState) itself — sun/moon/cloud drive its OWN lighting
 * math too, not just the HUD — and pushes a fresh copy into SetState() right before each Encode(); the
 * HUD pass itself is only entered by FBRenderer when HudEnabled (an empty pass is still a pass — the
 * PASS-COUNT invariant lives at that outer `if`, not in here). */
#ifndef FBHUDSTAGE_H
#define FBHUDSTAGE_H

#include "FBDrawStage.h"
#include "FBState.h"

namespace FlightBox {

class FBHudStage : public FBDrawStage {
public:
  void Init(const FBGpu &gpu) override;

  void SetState(const FBState &s, bool have) { State = s; Have = have; }
  void SetGroundMode(int photo);   /* SVS/EVS annunciator (w3_ground, an FBHud.h global) */
  void SetAgl(float agl);          /* w3_agl (an FBHud.h global), for the horizon-dip calc */

  /* Regenerates the symbology geometry for THIS frame's State and draws all three pipelines. */
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  /* Boot/teleport loading screen: "LOADING TERRAIN x%" + tile counter, MAX7456 glyphs, Text pipeline
   * only. A distinct draw from Encode() — FBRenderer calls this instead, while LoadingScreen is on. */
  void EncodeLoadingText(wgpu::RenderPassEncoder &pass, int width, int height, float pct, int ready, int total);

private:
  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::RenderPipeline SolidPipe, LinePipe, TextPipe;
  wgpu::BindGroup SolidBind, TextBind;
  wgpu::Buffer Uni, TriVtx, LineVtx, TextVtx;
  wgpu::Texture Atlas;
  wgpu::Sampler Samp;
  FBState State{};
  bool Have = false;
};

} // namespace FlightBox
#endif
