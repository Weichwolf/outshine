/* Temporal upsample of the quarter-res march into a full-res ping-pong history, reprojected by the
 * camera's motion at the cloud mid-shell. FBRenderer reads WriteIndex()/GetHistView()/GetWSumView() to
 * build the TWO-target pass descriptor BEFORE calling Encode() — which is exactly why this stage
 * cannot open its own pass. doc/flightbox/rendering.md, Abschnitt 5. */
#ifndef FBCLOUDRESOLVESTAGE_H
#define FBCLOUDRESOLVESTAGE_H

#include <webgpu/webgpu_cpp.h>
#include "FBFrameContext.h"
#include "FBGpu.h"

namespace FlightBox::Render {

class FBCloudResolveStage {
public:
  void Configure(const FBGpu &gpu, wgpu::Buffer atmoBuf, wgpu::Sampler samp, wgpu::TextureView cloudLowView);

  int WriteIndex(void) const { return HistCur; }
  int ReadIndex(void) const { return 1 - HistCur; }
  wgpu::TextureView GetHistView(int k) const { return CloudHist[k].CreateView(); }
  wgpu::TextureView GetWSumView(int k) const { return CloudWSum[k].CreateView(); }

  void SetAccumMode(bool on) { AccumMode = on; }
  void ResetHistory(void) { HistValid = false; AccumN = 0; }

  /* `cloudMidR` is the march's shell mid-radius (Mm) — the reprojection depth. */
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass, double cloudMidR);
  /* Call once per frame, AFTER the tonemap composite has consumed WriteIndex(). */
  void Advance(const FBFrameContext &ctx);

private:
  wgpu::Device Device;
  wgpu::Queue Queue;

  wgpu::Texture CloudHist[2];        /* rgba16float ping-pong (accumulated cloud) */
  wgpu::Texture CloudWSum[2];        /* accumulated splat weight per full-res pixel (r32float) */
  wgpu::RenderPipeline CloudResolvePipe;
  wgpu::BindGroup CloudResolveBind[2];   /* [k] binds CloudHist[k]/CloudWSum[k] as the PREV history */
  wgpu::Buffer ResolveUni;

  float PrevVP[16] = {0};            /* previous frame's MvpCamRel (reprojection) */
  double PrevEye[3] = {0, 0, 0};      /* previous frame's ECEF eye (metres) */
  int HistCur = 0;
  bool HistValid = false;
  bool AccumMode = false;             /* lab proof mode: weighted-splat running average */
  int AccumN = 0;                    /* frames since reset */
};

} // namespace FlightBox::Render
#endif
