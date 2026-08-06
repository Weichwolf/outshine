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
  /* THE TACTICAL MAP, and it REPLACES the cockpit symbology rather than being added to it: two
   * pictures at once would hand the map's knowledge to the seat and the seat's to the map, which is
   * the one thing doc/player-layer.md §9.7 says a careless build deletes. Borrowed, never owned;
   * nullptr = the cockpit. The PASS COUNT is unaffected either way — the same pass, other strokes. */
  void SetOverlay(const Systems::FBHudGeometry *g) { Overlay = g; }
  void SetAgl(float agl) { Agl = agl; }   /* feeds FBHudEnv, for the horizon-dip calc + AGL readout */
  /* What a WATCHED run's deck reads instead of a cockpit's blocks. Borrowed, nullptr in a cockpit. */
  void SetWatch(const Systems::FBHudWatch *w) { Watch = w; }

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
  const Systems::FBHudGeometry *Overlay = nullptr;
  const Systems::FBHudWatch *Watch = nullptr;
  std::vector<float> LoadingGlyphs;
};

} // namespace FlightBox::Render
#endif
