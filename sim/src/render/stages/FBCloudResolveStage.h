/* FlightBox — FBCloudResolveStage: temporal upsample of FBCloudMarchStage's quarter-res output into a
 * full-res ping-pong history (kCloudResolveWGSL), reprojected by the camera's motion at the cloud
 * mid-shell. FBRenderer still opens/closes this pass and reads WriteIndex()/GetHistView()/GetWSumView()
 * to build its RenderPassDescriptor (the two-target write attachment) BEFORE calling Encode(); Advance()
 * (called once per frame, after the tonemap composite has read this frame's result) flips the ping-pong
 * index and snapshots this frame's view-proj/eye as "previous" for next frame's reprojection — mirrors
 * the original single-function RenderFrame ordering exactly. */
#ifndef FBCLOUDRESOLVESTAGE_H
#define FBCLOUDRESOLVESTAGE_H

#include <webgpu/webgpu_cpp.h>
#include "FBFrameContext.h"
#include "FBGpu.h"

namespace FlightBox {

class FBCloudResolveStage {
public:
  void Configure(const FBGpu &gpu, wgpu::Buffer atmoBuf, wgpu::Sampler samp, wgpu::TextureView cloudLowView);

  int WriteIndex(void) const { return HistCur; }
  int ReadIndex(void) const { return 1 - HistCur; }
  wgpu::TextureView GetHistView(int k) const { return CloudHist[k].CreateView(); }
  wgpu::TextureView GetWSumView(int k) const { return CloudWSum[k].CreateView(); }

  void SetAccumMode(bool on) { AccumMode = on; }
  void ResetHistory(void) { HistValid = false; AccumN = 0; }

  /* Records the resolve draw into an ALREADY-open pass targeting GetHistView(WriteIndex())/
   * GetWSumView(WriteIndex()) — the caller builds that pass descriptor first. `cloudMidR` is
   * FBCloudMarchStage's shell mid-radius (Mm), the reprojection depth. */
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass, double cloudMidR);
  /* Flips the ping-pong index + snapshots this frame's view-proj/eye as "previous" — call once, after
   * the tonemap composite has consumed WriteIndex() for this frame. */
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

} // namespace FlightBox
#endif
