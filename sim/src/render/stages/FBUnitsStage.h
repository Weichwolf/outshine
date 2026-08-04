/* The draw slot for units, wired into the scene pass right after the terrain — one indexed draw per
 * visible unit, no pass of its own and none added.
 *
 * COSTS NOTHING WHEN NOTHING IS THERE: with no draw list, no registered model or no device the whole
 * Encode() returns before it touches the queue, so an empty world is byte-identical to the picture
 * before this stage drew anything. The pass count is FBRenderer's and is untouched.
 * Vertrag + Messung: doc/render/units-visual.md. */
#ifndef FBUNITSSTAGE_H
#define FBUNITSSTAGE_H

#include <memory>
#include <vector>
#include "FBDrawStage.h"
#include "FBCloudDensity.h"
#include "FBUnitDraw.h"
#include "FBUnitModel.h"

namespace FlightBox::Render {

class FBUnitsStage : public FBDrawStage {
public:
  /* Must run AFTER FBRenderer::CreateAtmosphere — it pins the sky-view LUT the haze reads, exactly as
   * FBTilesStage does, so a jet fades into the same air as the mountain behind it. */
  void Configure(const FBGpu &gpu, wgpu::Sampler samp, wgpu::Sampler lutSamp,
                 wgpu::TextureView skyLutView, wgpu::Buffer atmoBuf);

  /* CPU-side load; the GPU upload happens at Configure(). Call before FBRenderer::Init. */
  bool AddModel(const char *typeName, const char *dir);

  /* Borrowed for exactly one frame — FBWorld rewrites its vector every Update(). */
  void SetDraws(const FBUnitDraw *draws, int count) { Draws = draws; Count = draws ? count : 0; }

  void SetSky(const FBCloudSky &sky) { Sky = sky; }

  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  int LastDraws() const { return LastDraws_; }

private:
  static constexpr int kUniFloats = 36;   /* mat4 + sun + haze + three decks; FBTilesStage's `U` verbatim */
  static constexpr int kMaxUnits = 64;    /* storage-buffer slots; draws past this are dropped */
  static constexpr int kPartRows = kMaxUnitParts * 3;   /* one 3x4 part matrix = three vec4 rows */
  static constexpr unsigned kUnitLogEvery = 300;        /* [unit_draw] cadence; a screenshot run is short */

  struct GpuLod {
    wgpu::Buffer Vtx, Idx, Mats;
    wgpu::Texture Tex;
    wgpu::BindGroup Bind;
    uint32_t IndexCount = 0;
  };
  struct GpuModel {
    std::unique_ptr<FBUnitModel> Cpu;
    std::vector<GpuLod> Lods;
  };
  struct Item { const GpuLod *Lod; int Slot; };

  void UploadModel(GpuModel &m);
  const GpuModel *Find(const char *type) const;
  void LogUnit(const FBFrameContext &ctx, const FBUnitDraw &d, const FBUnitModel::Lod &lod,
               const double rel[3], double range, int li, int tris) const;
  void LogCast(const FBFrameContext &ctx) const;

  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::TextureFormat HdrFormat = wgpu::TextureFormat::RGBA16Float;
  wgpu::Sampler Samp, LutSamp;
  wgpu::TextureView SkyLutView;
  wgpu::Buffer AtmoBuf, Uni, PartBuf;
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Frame;
  std::vector<GpuModel> Models;
  std::vector<float> PartScratch;   /* kMaxUnits * kPartRows * 4, reused: no per-frame heap */
  std::vector<Item> Items;          /* likewise reused; bounded by kMaxUnits */

  const FBUnitDraw *Draws = nullptr;
  int Count = 0;
  int LastDraws_ = 0;
  bool Ready = false;
  FBCloudSky Sky;
};

} // namespace FlightBox::Render
#endif
