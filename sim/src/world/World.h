/* Multi-LOD terrain streaming: a chunked-LOD quadtree that feeds Renderer a per-frame draw list of
 * the LOD cut. A node draws ITSELF or its four children, never both.
 *
 * THE CORRECTED walk.h SEMANTICS, because getting it wrong opens holes in the world: view distance may
 * only PREVENT a split — a child past the view radius makes the parent stay a drawn LEAF (detail
 * dropped, coverage NEVER), and children's viability is tested side-effect-free BEFORE the parent is
 * replaced. */
#ifndef WORLD_H
#define WORLD_H

#include <cstdint>
#include <vector>
#include "BuildingField.h"
#include "WaterField.h"
#include "ClassField.h"
#include "ClusterDag.h"
#include "ModuleMemory.h"

namespace outshine::Render { class Renderer; }
namespace outshine { class WeatherProvider; }

namespace outshine::World {

class VegetationTemplates;

class World {
public:
  /* `pixelFocalLength` is core/PixelFocalLength.h's, in pixels of the DECLARED frame — the terrain
   * ladder and the vegetation ladder measure their screen-space error against the same number, and a
   * default here would be a second place saying how large the picture is. */
  explicit World(double pixelFocalLength);
  /* Closes the tile stream, which on both platforms means retiring whatever is still fetching or
   * meshing off this thread. A pool outliving its process is a crash at static destruction. */
  ~World();

  /* BORROWED for the same reason: the atmosphere is simulation state (core/WeatherProvider), and the
   * drawing side only ever ASKS it — cover, cloud base and wind for the cloud rebuild. Null until a
   * client sets one, and nothing here draws weather today. */
  /* BORROWED: the client owns the table (it also hands the same rows to the renderer). Null = no
   * near-field ground field is built at all. */
  void SetVegetation(const VegetationTemplates *veg) { Veg_ = veg; Cls_.SetVegetation(veg); }

  /* THE CLASS, for every consumer that is not a fragment: -1 where OSM has no datum, which is a
   * state and not a default. */
  const ClassField &Classes() const { return Cls_; }

  void SetWeather(const WeatherProvider *weather) { Weather_ = weather; }
  const WeatherProvider *Weather() const { return Weather_; }

  /* `viewMeters` = the view radius (FB_VIEW_KM * 1000). */
  bool Open(Render::Renderer *renderer, const char *tilesBase, double lat, double lon,
            double viewMeters, int albedoTS);

  /* One budgeted refinement pass; `nowMs` drives the 1 Hz counter log. */
  void Update(double camLat, double camLon, const double eyeEcef[3], const double fwdEcef[3],
              double nowMs);

  /* Currently VIEWED mode (0 = OSM, 1 = photo). Whichever is NOT the boot default is the lazy
   * OVERLAY: fetched only while viewed, for on-screen tiles, then cached. */
  /* The EAGER base albedo source, uploaded with every tile. */

  /* Fraction of the geometry target cut that is GPU-ready. The client holds the loading screen (and
   * JSBSim) until this crosses its threshold. */
  float LoadProgress() const { return TargetTot > 0 ? (float)TargetRdy / (float)TargetTot : 0.0f; }
  int TargetTotal() const { return TargetTot; }
  int TargetReadyN() const { return TargetRdy; }
  /* WHAT THE CAMERA CAN SEE of the target cut. Residency is still the whole radius; this is the
   * number that says what a frustum-scoped one would have cost. */
  int TargetInViewN() const { return TargetView; }

  /* NOTHING IS STILL ON ITS WAY. Streaming is asynchronous, so a fixed number of passes says nothing
   * about what has arrived; an oracle that wants a picture of the whole scene waits on this instead.
   * Three streams have to be in: the geometry target cut, the OSM building block, and the cluster DAG
   * of the tile that block last decoded. */
  bool Resident() const {
    return TargetTot > 0 && TargetRdy == TargetTot && Vectors.PendingTiles() == 0 &&
           BuildingDagId == 0 && Cls_.Complete();
  }
  int BuildingPendingTiles() const { return Vectors.PendingTiles(); }
  /* The roof over a place, ASL, or -1e30 where no footprint stands. An eye inside a wall is not a
   * standpoint, and only these two fields together can say so. */
  double RoofAslAt(double lat, double lon) const { return Buildings.RoofAslAt(Vectors, lat, lon); }

  /* THE RESIDENCY COUNTERS, for a moving measurement: a per-frame series that does not settle is the
   * defect, and none of these is visible in a picture. */
  int NodeCount() const { return (int)Nodes.size(); }
  int DrawnLeafCount() const { return (int)DrawnLeaves.size(); }
  long BuiltCount() const { return Built; }
  long EvictedCount() const { return Evicted; }
  double UpdateMs() const { return UpdateMs_; }
  double ClassMs() const { return ClassMs_; }
  double MeshMs() const { return MeshMs_; }
  double AlbedoMs() const { return AlbedoMs_; }
  double UploadMs() const { return UploadMs_; }
  double BuildingMs() const { return BuildingMs_; }
  double BuildingDecodeMs() const { return BuildingDecodeMs_; }

  /* WHAT THE WORLD HOLDS ON THE HEAP, split by pool so a rise has an owner: the resident tile nodes
   * with their meshes, DAGs, albedo and lamps, the decoded OSM vectors, what is extruded and
   * tessellated from them, and the class. */
  struct Pools {
    size_t TileNodes = 0, Vectors = 0, Buildings = 0, Water = 0, Class = 0;
    size_t Sum() const { return TileNodes + Vectors + Buildings + Water + Class; }
  };
  Pools HeapPools() const;
  /* The tile byte caches THIS module can see. Natively that is all of them. In the browser it is only
   * the main thread's own fetch caches — the pool's decoded bytes sit in the tile workers' separate
   * wasm modules, which no call from here can reach. */
  size_t ByteCacheBytes() const;
  /* THE OTHER LINEAR MEMORIES the client runs on: one row per tile-worker module, each measured
   * inside the module it belongs to. Empty where the pool is threads of this module. */
  std::vector<ModuleMemory> WorkerMemory() const;

  /* The field of view is an animation channel, so the ladder has to be able to follow it mid-run. */
  void SetPixelFocalLength(double px) { PixelFocal_ = px; }

private:
  struct Node {
    int z;
    long x, y;
    unsigned touch;
    int stale;
    int slot;              /* Renderer table slot, -1 until uploaded */
    int haveMesh;
    unsigned readyPass;    /* pass the GPU upload was issued; drawable only in a LATER pass (2-phase) */
    unsigned emitPass;     /* last pass this tile was drawn — lets a mode switch keep it (old mode) vs re-coarsen */
    std::vector<float> verts;              /* ONE vertex per posting; every level indexes into it */
    std::vector<uint32_t> idx;             /* every level's clusters, level 0 first */
    std::vector<DagCluster> clusters;
    int nverts, nidx;
    double origin[3];      /* tile-centre ECEF (from the mesh, once built) */
    float err;             /* geometric error (m), valid once haveMesh */
    std::vector<uint8_t> albedo;
  };
  struct Work { int idx; double prio; };

  int Ensure(int z, long x, long y);                              /* node index (creates on miss) */
  bool Uploaded(const Node &n) const { return n.haveMesh && n.slot >= 0; }
  /* Two-phase commit: drawable only ONE pass after the upload was issued, so the WriteTexture is
   * submitted and visible before any draw references the layer. */
  bool Ready(const Node &n) const { return Uploaded(n) && Pass > n.readyPass; }
  bool Viable(int z, long x, long y, const double eye[3]) const;  /* map bounds + view (pure) */
  bool WantSplit(int z, long x, long y, const double eye[3]) const;   /* geometry-only refine test */
  int  Find(int z, long x, long y) const;                            /* node idx or -1 (no create) */
  bool CanCover(int z, long x, long y, const double eye[3]) const;    /* subtree fully ready? (pure) */
  void RequestSubtree(int z, long x, long y, const double eye[3], const double fwd[3]);  /* cascade request to targets */
  int  Descend(int z, long x, long y, const double eye[3], const double fwd[3]);  /* draw traversal; 1 = covered */
  void CountTargets(int z, long x, long y, const double eye[3], const double fwd[3], int &total,
                    int &ready, int &inView) const;   /* target-cut progress + the share the camera sees */
  void Emit(int idx);
  void AddWork(int idx, int z, long x, long y, const double eye[3], const double fwd[3]);
  void Center(int z, long x, long y, double out[3]) const;
  double SpanM(int z) const;
  void SurfaceAnchor(int z, long x, long y, double out[3]) const;

  const WeatherProvider *Weather_ = nullptr;   /* borrowed, see SetWeather's banner */
  const VegetationTemplates *Veg_ = nullptr;  /* borrowed, see SetVegetation's banner */

  double PixelFocal_;
  Render::Renderer *R;
  int TS;
  double ViewM, Lat0, Lon0;
  std::vector<Node> Nodes;
  std::vector<int> DrawSlots;
  std::vector<Work> WorkList;
  unsigned Pass;
  long Evicted;
  long Built = 0;                    /* cumulative tile uploads (build completions) — thrash diagnosis */
  long PrevBuilt = 0, PrevEvicted = 0;   /* deltas for the builds/min + evictions/min rate on [fbworld] */
  double LastLog;
  int Leaves, DrawnReady, Pending;   /* per-pass counters */
  int TargetTot, TargetRdy;          /* geometry target-cut: total leaves / GPU-ready (LoadProgress) */
  int TargetView = 0;                /* of those, the share inside the camera cone — published only */
  long MeshVram;

  std::vector<int> DrawnLeaves;      /* node indices emitted as drawn leaves this pass; the only list
                                        that says which mesh the near-field ground field may rasterise */

  OsmField Vectors{14, {"buildings", "water_polygons", "water_lines"}};
  ClassField Cls_;
  BuildingField Buildings;
  WaterField Water;
  std::vector<float> WaterVerts;
  uint32_t BuildingVerts = 0;
  std::vector<float> BuildingDagVerts;
  std::vector<uint32_t> BuildingDagIdx;
  std::vector<DagCluster> BuildingClusters;
  std::vector<float> BuildingSoup;   /* the one tile whose DAG is in flight; empty otherwise */
  int BuildingDagId = 0, BuildingDagSeq = 0;
  bool Opened = false;
  double UpdateMs_ = 0.0;
  double ClassMs_ = 0.0;
  uint64_t UploadedClassVersion_ = 0;
  double MeshMs_ = 0.0, AlbedoMs_ = 0.0, UploadMs_ = 0.0, BuildingMs_ = 0.0, BuildingDecodeMs_ = 0.0;
};

} // namespace outshine::World
#endif
