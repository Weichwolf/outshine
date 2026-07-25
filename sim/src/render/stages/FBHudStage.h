/* FlightBox — FBHudStage: the MIL-STD-1787 HUD's WebGPU backend. Pure backend now: it owns pipelines/
 * buffers/atlas and, once per Encode(), asks the borrowed FBDisplaySystem to fill an FBHudGeometry —
 * the symbology LOGIC lives there (systems/FBDisplaySystem), not here. Two pipelines share one
 * pixel->NDC map: Stroke (every straight segment -- rails/ticks/carets/boxes/the conformal horizon --
 * as an analytic-coverage AA quad, replacing the old hard LineList + separate AA-triangle pipeline),
 * Text (bitmap-font atlas glyph quads, coverage-AA). Also owns the boot/teleport loading-screen text
 * draw (a second, unrelated use of the Text pipeline).
 *
 * FBRenderer keeps the actual telemetry pose (FBState) itself — sun/moon/cloud drive its OWN lighting
 * math too, not just the HUD — and pushes a fresh copy into SetState() right before each Encode(); the
 * HUD pass itself is only entered by FBRenderer when HudEnabled (an empty pass is still a pass — the
 * PASS-COUNT invariant lives at that outer `if`, not in here). */
#ifndef FBHUDSTAGE_H
#define FBHUDSTAGE_H

#include "FBDisplaySystem.h"
#include "FBDrawStage.h"
#include "FBHudGeometry.h"
#include "FBState.h"
#include <vector>

namespace FlightBox {

class FBHudStage : public FBDrawStage {
public:
  void Init(const FBGpu &gpu) override;

  void SetState(const FBState &s, bool have) { State = s; Have = have; }
  /* The Displays system slot that owns the symbology (see the class banner) — borrowed, never owned;
   * the App wires it from the active module (e.g. FBF16Module::Displays()). nullptr -> empty HUD. */
  void SetDisplaySystem(const FBDisplaySystem *disp) { Disp = disp; }
  void SetAgl(float agl) { Agl = agl; }   /* feeds FBHudEnv, for the horizon-dip calc + AGL readout */

  /* Regenerates the symbology geometry for THIS frame's State and draws all three pipelines. */
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  /* Boot/teleport loading screen: "LOADING TERRAIN x%" + tile counter, MAX7456 glyphs, Text pipeline
   * only. A distinct draw from Encode() — FBRenderer calls this instead, while LoadingScreen is on. */
  void EncodeLoadingText(wgpu::RenderPassEncoder &pass, int width, int height, float pct, int ready, int total);

private:
  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::RenderPipeline StrokePipe, TextPipe;
  wgpu::BindGroup StrokeBind, TextBind;
  wgpu::Buffer Uni, StrokeVtx, TextVtx;
  wgpu::Texture Atlas;
  wgpu::Sampler Samp;
  FBState State{};
  bool Have = false;
  float Agl = 0.0f;
  const FBDisplaySystem *Disp = nullptr;
  FBHudGeometry Geometry;
  std::vector<float> LoadingGlyphs;
};

} // namespace FlightBox
#endif
