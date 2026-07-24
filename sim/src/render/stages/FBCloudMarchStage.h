/* FlightBox — FBCloudMarchStage: the volumetric raymarch (kCloudWGSL) into the quarter-res CloudLowTex.
 * Owns the per-frame CloudUni update (weather deck -> shell radii/material params) and the optional GPU
 * timestamp query bracketing the march pass (native oracle [cloud-perf] telemetry). Reads the base/
 * detail/cell noise volumes + depth/atmosphere LUTs as views injected at Configure() (produced by their
 * own bake stages / FBRenderer) — never regenerates them. */
#ifndef FBCLOUDMARCHSTAGE_H
#define FBCLOUDMARCHSTAGE_H

#include <webgpu/webgpu_cpp.h>
#include "FBDrawStage.h"

namespace FlightBox {

class FBCloudMarchStage : public FBDrawStage {
public:
  void Configure(const FBGpu &gpu, wgpu::Buffer atmoBuf, wgpu::Sampler lutSamp,
                 wgpu::TextureView skyLUTView, wgpu::TextureView transLUTView, wgpu::TextureView depthView,
                 wgpu::TextureView baseView, wgpu::TextureView detailView, wgpu::TextureView cellView,
                 bool hasTimestamp);

  void SetQuality(double q) { CloudQuality = q; }
  double GetQuality(void) const { return CloudQuality; }
  void SetCloudLab(float cover, float density, float extinct, float sunI, float detail) {
    CloudLab = true; LabCover = cover; LabDensity = density; LabExtinct = extinct; LabSunI = sunI; LabDetail = detail;
  }
  double GetCloudMidR(void) const { return CloudMidR; }
  wgpu::TextureView GetLowView(void) const { return CloudLowTex.CreateView(); }
  bool WantsTimestamp(void) const { return HasTimestamp; }
  wgpu::QuerySet GetQuerySet(void) const { return TsQuery; }

  /* Refreshes CloudUni from the current frame's weather/camera state — call before Encode(). */
  void Update(const FBFrameContext &ctx);
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  /* GPU-timing bracket for the native [cloud-perf] log: Resolve before Finish(), Poll after Submit(). */
  void ResolveTimestamps(wgpu::CommandEncoder &enc);
  void PollTimestamps(void);

private:
  wgpu::Device Device;
  wgpu::Queue Queue;

  wgpu::Texture CloudLowTex;   /* QUARTER-RES march target (Width/4 x Height/4) rgba16float */
  wgpu::RenderPipeline CloudPipe;
  wgpu::BindGroup CloudBind;
  wgpu::Buffer CloudUni;
  wgpu::Sampler CloudSamp;     /* 3D linear, repeat */
  int CloudW = 0, CloudH = 0;  /* quarter-res dims */
  double CloudQuality = 1.0;
  double CloudMidR = 6.362;    /* cloud mid-shell radius Mm (Resolve's reprojection depth) */

  bool CloudLab = false;       /* cloud-lab param override active */
  float LabCover = 0, LabDensity = 0, LabExtinct = 0, LabSunI = 0, LabDetail = 0;

  wgpu::QuerySet TsQuery;
  wgpu::Buffer TsResolveBuf, TsReadBuf;
  bool HasTimestamp = false, TsMapPending = false;
  double TsAccumMs = 0.0;
  int TsCount = 0;
};

} // namespace FlightBox
#endif
