/* HOW MANY STOPS THE SCENE IS SLID BEFORE THE CURVE, placed from the ILLUMINATION and never from the
 * picture. Night to noon spans some twenty stops and a display carries far fewer, so a mapping has to
 * exist; what it may not depend on is where the camera points, because turning the head does not
 * change the physics of the scene. Its one input is IrradianceStage's horizontal irradiance, and its
 * one output is the scalar TaaStage multiplies scene radiance by before stages/Filmic.h's curve.
 *
 * One compute dispatch on eight floats, riding the sky-view compute pass right after the dispatch
 * that produces its input — no pass of its own, no readback, no frame of latency, and no state: the
 * anchors are a pure function of this frame's irradiance, so there is nothing to adapt. */
#ifndef EXPOSURESTAGE_H
#define EXPOSURESTAGE_H

#include "../ExposureParams.h"
#include "DrawStage.h"

namespace outshine::Render {

class ExposureStage : public DrawStage {
public:
  void Configure(const Gpu &gpu, wgpu::Buffer meterBuf, wgpu::Buffer irrBuf);
  void EncodeCompute(const FrameContext &ctx, wgpu::ComputePassEncoder &pass) override;

  void SetParams(const ExposureParams &p) { Params = p; }

  /* expScale, keyLog2, horizE, pad */
  static constexpr int kMeterFloats = 4;
  static constexpr uint64_t kMeterBytes = kMeterFloats * sizeof(float);

private:
  wgpu::Queue Queue;
  wgpu::ComputePipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::Buffer CfgBuf;
  ExposureParams Params;
};

} // namespace outshine::Render
#endif
