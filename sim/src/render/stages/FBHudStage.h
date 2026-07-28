/* The HUD's WebGPU backend and nothing more: pipelines, buffers, atlas. Once per Encode() it asks the
 * BORROWED FBDisplaySystem to fill an FBHudGeometry — the symbology LOGIC lives there.
 * The HUD pass is entered by FBRenderer only when HudEnabled: an empty pass is still a pass, so the
 * PASS-COUNT invariant lives at that outer `if` and not in here.
 * Pipelines, Blend und Deckungsformel: doc/render/renderer.md, Abschnitt 7.2. */
#ifndef FBHUDSTAGE_H
#define FBHUDSTAGE_H

#include "FBDisplaySystem.h"
#include "FBDrawStage.h"
#include "FBHudGeometry.h"
#include "FBState.h"
#include <vector>

namespace FlightBox::Render {

class FBHudStage : public FBDrawStage {
public:
  void Init(const FBGpu &gpu) override;

  void SetState(const FBState &s, bool have) { State = s; Have = have; }
  /* Borrowed, never owned; the client wires it from the active module. nullptr -> empty HUD. */
  void SetDisplaySystem(const Systems::FBDisplaySystem *disp) { Disp = disp; }
  void SetAgl(float agl) { Agl = agl; }   /* feeds FBHudEnv, for the horizon-dip calc + AGL readout */

  /* Regenerates the symbology for THIS frame's State, then draws. */
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  /* The Text pipeline's second, unrelated use. FBRenderer calls this INSTEAD of Encode(). */
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
  const Systems::FBDisplaySystem *Disp = nullptr;
  Systems::FBHudGeometry Geometry;
  std::vector<float> LoadingGlyphs;
};

} // namespace FlightBox::Render
#endif
