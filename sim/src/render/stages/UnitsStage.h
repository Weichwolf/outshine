/* The draw slot for units, wired into the scene pass right after the terrain — one indexed draw per
 * visible unit, no pass of its own and none added.
 *
 * COSTS NOTHING WHEN NOTHING IS THERE: with no draw list, no registered model or no device the whole
 * Encode() returns before it touches the queue, so an empty world is byte-identical to the picture
 * before this stage drew anything. The pass count is Renderer's and is untouched.
 * Vertrag + Messung: doc/render/units-visual.md. */
#ifndef UNITSSTAGE_H
#define UNITSSTAGE_H

#include <memory>
#include <vector>
#include "DrawStage.h"
#include "CloudDensity.h"
#include "UnitDraw.h"
#include "UnitModel.h"

namespace outshine::Render {

class UnitsStage : public DrawStage {
public:
  /* Must run AFTER Renderer::CreateAtmosphere — it pins the sky-view LUT the haze reads, exactly as
   * TilesStage does, so a jet fades into the same air as the mountain behind it. */
  void Configure(const Gpu &gpu, wgpu::Sampler samp, wgpu::Sampler lutSamp,
                 wgpu::TextureView skyLutView, wgpu::Buffer atmoBuf);

  /* CPU-side load; the GPU upload happens at Configure(). Call before Renderer::Init. */
  bool AddModel(const char *typeName, const char *dir);

  /* Borrowed for exactly one frame — World rewrites its vector every Update(). */
  void SetDraws(const UnitDraw *draws, int count) { Draws = draws; Count = draws ? count : 0; }

  void SetSky(const CloudSky &sky) { Sky = sky; }

  /* WHERE THIS TYPE'S EXHAUST LEAVES, in model space, off the loaded mesh (UnitModel). False = no
   * such node in the asset, and then nothing draws a plume for it. The flame is built by World,
   * where the pose is, so this is the one number that has to travel back out of the model. */
  bool Nozzle(const char *type, float off[3], float &radiusM) const;

  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  int LastDraws() const { return LastDraws_; }

private:
  static constexpr int kUniFloats = 36;   /* mat4 + sun + haze + three decks; TilesStage's `U` verbatim */
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
    std::unique_ptr<UnitModel> Cpu;
    std::vector<GpuLod> Lods;
  };
  struct Item { const GpuLod *Lod; int Slot; };

  void UploadModel(GpuModel &m);
  const GpuModel *Find(const char *type) const;
  void LogUnit(const FrameContext &ctx, const UnitDraw &d, const UnitModel::Lod &lod,
               const double rel[3], double range, int li, int tris) const;
  void LogCast(const FrameContext &ctx) const;

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

  const UnitDraw *Draws = nullptr;
  int Count = 0;
  int LastDraws_ = 0;
  bool Ready = false;
  CloudSky Sky;
};

} // namespace outshine::Render
#endif
