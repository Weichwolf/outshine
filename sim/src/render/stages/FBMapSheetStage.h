/* FlightBox — FBMapSheetStage: the tactical map's GROUND, an OSM raster sheet.
 *
 * It draws the same /bake/osm pages the terrain streamer already pulls (fb_stream_pyramid, mode 0),
 * as screen-space quads on the Web-Mercator grid FBMapView projects onto — so a symbol sits on the
 * piece of sheet it was measured over by construction, not by keeping two projections in step.
 *
 * IT DOES NOT FETCH. The byte source is INJECTED by the client, because world/ tile streaming is the
 * client's business and render/ may not grow a second tile client of its own. The hook has the
 * loader's own contract: >0 resident, 0 pending, -1 a real hole.
 *
 * It records into the HUD pass FBRenderer already opens, first, under the symbology — no Begin*Pass
 * of its own, so the per-frame pass count is exactly the one it was. */
#ifndef FBMAPSHEETSTAGE_H
#define FBMAPSHEETSTAGE_H

#include "FBDrawStage.h"
#include "FBMapView.h"

#include <vector>

namespace FlightBox::Render {

/* What the map may honestly say about its own ground this frame. */
enum class FBMapSheetState { Off, Waiting, Partial, Complete, Unreachable };

/* The line a chart must carry when its ground is not there; empty when there is nothing to report. */
const char *FBMapSheetNote(FBMapSheetState s);

class FBMapSheetStage : public FBDrawStage {
public:
  /* The loader's contract, verbatim: writes fb_pyramid_bytes(ts) of packed sRGB mips into `dst`.
   * >0 resident, 0 pending (ask again), -1 a real hole. */
  using FBTileFetch = int (*)(int z, int x, int y, int ts, unsigned char *dst);

  /* [SET] 512 is the edge fb-tiles bakes for the terrain, so the map's pages are server-cache hits
   * rather than a second bake size. */
  static constexpr int kTs = 512;
  /* [SET] A 1280x720 frame at one texel per pixel spans at most 5x3 tiles once the zoom sits half a
   * level off; 24 leaves room for one pan-step of history before a re-fetch. 24 MiB of RGBA8+mips. */
  static constexpr int kLayers = 24;

  void Init(const FBGpu &gpu) override;
  void Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  void SetSource(FBTileFetch fetch) { Fetch = fetch; }
  /* Off = the stage records nothing and the cockpit frame is untouched. */
  void SetView(const FBMapView &v, bool on);
  /* Asks the source for whatever the current view still lacks. Separate from Encode because a client
   * may pump the sheet at its own rate — a blocking native fetch is not a per-frame cost. */
  void Pump();

  FBMapSheetState State() const;
  int TilesWanted() const { return Wanted; }
  int TilesResident() const { return Resident; }
  int TilesHole() const { return Holes; }
  int Zoom() const { return Z; }

private:
  struct FBSheetTile {
    int Z, X, Y;
    int Layer;        /* array layer, -1 until resident */
    int Ready;        /* 0 pending, 1 resident, -1 hole */
    unsigned Touch;
  };

  int Find(int z, int x, int y) const;
  int AllocLayer(void);
  void WriteLayer(int layer, const unsigned char *pyramid);
  void BuildQuads(void);

  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::Buffer Uni, Vtx;
  wgpu::Texture Sheet;
  wgpu::Sampler Samp;

  FBTileFetch Fetch = nullptr;
  FBMapView View{};
  bool On = false;
  int Z = 0;
  unsigned Frame = 0, Logged = 0;
  int Wanted = 0, Resident = 0, Holes = 0;
  std::vector<FBSheetTile> Tiles;
  std::vector<int> FreeLayers;
  int LayerUsed = 0;
  std::vector<float> Verts;      /* 6 verts/quad: posPx.xy, uv.xy, layer (negative = backdrop) */
  std::vector<unsigned char> Pyramid;
};

} // namespace FlightBox::Render
#endif
