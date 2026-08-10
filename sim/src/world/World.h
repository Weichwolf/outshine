/* The world under the eye, in two halves that share one tile stream.
 *
 * `Update` is the simulation: OSM vectors, the class grid, water bodies and footprints. It has no
 * camera, needs no device, and is the whole of what the server target runs.
 *
 * `Refine` is the picture: a chunked-LOD quadtree over the terrain, cut against an eye. A node draws
 * ITSELF or its four children, never both. Its products — tile meshes, the draw list, the water and
 * building surfaces — are COLLECTED by whoever draws; nothing here calls a renderer.
 *
 * THE CORRECTED walk.h SEMANTICS, because getting it wrong opens holes in the world: view distance may
 * only PREVENT a split — a child past the view radius makes the parent stay a drawn LEAF (detail
 * dropped, coverage NEVER), and children's viability is tested side-effect-free BEFORE the parent is
 * replaced. */
#ifndef WORLD_H
#define WORLD_H

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "BuildingField.h"
#include "WaterField.h"
#include "ClassField.h"
#include "ClusterDag.h"
#include "ModuleMemory.h"

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
  bool Open(const char *tilesBase, double lat, double lon, double viewMeters, int albedoTS);

  /* THE SIMULATION PASS: vectors, class, water bodies, footprints. No camera and no device. */
  void Update(double camLat, double camLon);

  /* Where the picture looks from: the same standpoint in the two frames the cut needs — degrees for
   * the tile addresses, metres in ECEF for the distances. `FwdEcef` is a unit vector. */
  struct Eye {
    double LatDeg = 0.0, LonDeg = 0.0;
    const double *PosEcef = nullptr;
    const double *FwdEcef = nullptr;
  };
  /* THE PICTURE PASS, budgeted, and it runs AFTER Update in a frame that does both: the building
   * DAG it pumps is the job Update submitted. `nowMs` drives the 1 Hz counter log. */
  void Refine(const Eye &eye, double nowMs);

  /* A TILE MESH NOBODY HAS TAKEN YET, nearest and most in view first. The pointers are the node's
   * own storage and stay valid until Collect() names this id; they are the vertex layout of
   * core/ChunkVtx.h and the cluster ranges of core/ClusterDag.h. */
  struct TileMesh {
    uint64_t Id = 0;
    const float *Verts = nullptr;
    uint32_t VertCount = 0;
    const uint32_t *Idx = nullptr;
    uint32_t IdxCount = 0;
    const DagCluster *Clusters = nullptr;
    int ClusterCount = 0;
    double OriginEcef[3] = {};   /* tile centre, metres */
    double AnchorEcef[3] = {};   /* the z10 ancestor's centre, the procedural surface's frame */
  };
  const std::vector<TileMesh> &Uncollected() const { return Uncollected_; }
  /* `handle` is the collector's own name for this tile and the world only ever hands it back
   * through Drawn() and Retired(); the heap copy of the mesh is released here. */
  void Collect(uint64_t id, int handle);
  /* THE LOD CUT of the last Refine, in collector handles. */
  const std::vector<int> &Drawn() const { return DrawnHandles_; }
  /* Handles whose tile has been evicted since the last call. Emptied by the call. */
  std::vector<int> TakeRetired();

  /* THE WATER SURFACE, pos3 + nrm3 per vertex, ECEF offsets from AnchorEcef. `Seq` rises whenever
   * the tessellation changed and is 0 while nothing stands. */
  struct WaterSurface {
    const float *Verts = nullptr;
    uint32_t VertCount = 0;
    const double *AnchorEcef = nullptr;
    uint64_t Seq = 0;
  };
  WaterSurface Water() const;

  /* THE FOOTPRINTS AS A CLUSTER DAG, core/ChunkVtx.h layout, ECEF offsets from AnchorEcef. */
  struct BuildingSurface {
    const float *Verts = nullptr;
    uint32_t VertCount = 0;
    const uint32_t *Idx = nullptr;
    uint32_t IdxCount = 0;
    const DagCluster *Clusters = nullptr;
    int ClusterCount = 0;
    const double *AnchorEcef = nullptr;
    uint64_t Seq = 0;
  };
  BuildingSurface Buildings() const;

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

  /* WHAT THE SIMULATION WAS WAITING FOR: the OSM block around the standpoint and the class grid.
   * Nothing about a device or a mesh is in it, so this is what the server target waits on. */
  bool VectorsResident() const { return Vectors_.PendingTiles() == 0 && Cls_.Complete(); }
  /* NOTHING IS STILL ON ITS WAY. Streaming is asynchronous, so a fixed number of passes says nothing
   * about what has arrived; an oracle that wants a picture of the whole scene waits on this instead.
   * That is the geometry target cut and the cluster DAG of the tile the block last decoded, on top
   * of what the simulation waits for. */
  bool Resident() const {
    return TargetTot > 0 && TargetRdy == TargetTot && BuildingDagId == 0 &&
           DagDone_ == FootprintTileEnds_.size() && VectorsResident();
  }
  int BuildingPendingTiles() const { return Vectors_.PendingTiles(); }
  /* "NOTHING STANDS HERE" IS A STATE AND GETS A PREDICATE, not a literal at every call site: an
   * absent roof and an absent water surface answer with this, and a caller that subtracts it from a
   * ground height without asking gets a kilometre of nonsense. */
  static constexpr double kNoSurfaceAslM = -1.0e30;
  static bool SurfaceStands(double aslM) { return aslM > kNoSurfaceAslM * 0.5; }
  /* The roof over a place, ASL. An eye inside a wall is not a standpoint, and only this and the
   * ground together can say so. */
  double RoofAslAt(double lat, double lon) const { return Buildings_.RoofAslAt(Vectors_, lat, lon); }
  /* The water surface over a place, ASL. Depth is that less the ground, and both sides of that
   * subtraction are the core's (doc/architecture.md, "Water is not a transparency case"). */
  double WaterAslAt(double lat, double lon) const { return Water_.LevelAslAt(Vectors_, lat, lon); }

  /* THE RESIDENCY COUNTERS, for a moving measurement: a per-frame series that does not settle is the
   * defect, and none of these is visible in a picture. */
  int NodeCount() const { return (int)Nodes.size(); }
  int DrawnLeafCount() const { return (int)DrawnLeaves.size(); }
  long BuiltCount() const { return Built; }
  long EvictedCount() const { return Evicted; }
  /* What the world cost this frame, both halves of it. */
  double PassMs() const { return UpdateMs_ + RefineMs_; }
  double ClassMs() const { return ClassMs_; }
  double MeshMs() const { return MeshMs_; }
  double BuildingMs() const { return BuildingMs_ + BuildingDagMs_; }
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
    int handle;            /* the collector's name for this tile, -1 until collected */
    int haveMesh;
    unsigned readyPass;    /* pass the mesh was collected; drawable only in a LATER pass (2-phase) */
    unsigned emitPass;     /* last pass this tile was drawn — lets a mode switch keep it (old mode) vs re-coarsen */
    std::vector<float> verts;              /* ONE vertex per posting; every level indexes into it */
    std::vector<uint32_t> idx;             /* every level's clusters, level 0 first */
    std::vector<DagCluster> clusters;
    int nverts, nidx;
    double origin[3];      /* tile-centre ECEF (from the mesh, once built) */
    double anchor[3];      /* the z10 ancestor's centre, valid once haveMesh */
    float err;             /* geometric error (m), valid once haveMesh */
    std::vector<uint8_t> albedo;
  };
  struct Work { int idx; double prio; };

  int Ensure(int z, long x, long y);                              /* node index (creates on miss) */
  bool Taken(const Node &n) const { return n.haveMesh && n.handle >= 0; }
  /* Two-phase commit: drawable only ONE pass after the mesh was handed over, so a collector's upload
   * is submitted and visible before any draw references it. */
  bool Ready(const Node &n) const { return Taken(n) && Pass > n.readyPass; }
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
  int TS;
  double ViewM, Lat0, Lon0;
  std::vector<Node> Nodes;
  std::unordered_map<uint64_t, int> Index_;   /* packed (z,x,y) -> index into Nodes */
  std::vector<int> DrawnHandles_;
  std::vector<TileMesh> Uncollected_;
  std::vector<int> Retired_;
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

  OsmField Vectors_{14, {"buildings", "water_polygons", "water_lines"}};
  ClassField Cls_;
  BuildingField Buildings_;
  WaterField Water_;
  std::vector<float> WaterVerts;
  uint64_t WaterSeq_ = 0;
  bool WaterDirty_ = false;
  uint32_t BuildingVerts = 0;
  std::vector<float> BuildingDagVerts;
  std::vector<uint32_t> BuildingDagIdx;
  std::vector<DagCluster> BuildingClusters;
  std::vector<float> BuildingSoup;   /* the one tile whose DAG is in flight; empty otherwise */
  /* WHERE EACH EXTRUDED TILE ENDS in Buildings_.Verts(), as float indices. Extrusion is a statement
   * about place and runs at the simulation's pace; the cluster DAG is superlinear in soup size and
   * is therefore built ONE TILE AT A TIME, which is what this queue keeps possible without the
   * picture holding the simulation back. */
  std::vector<uint32_t> FootprintTileEnds_;
  size_t DagDone_ = 0;
  int BuildingDagId = 0, BuildingDagSeq = 0;
  uint64_t BuildingSeq_ = 0;
  bool Opened = false;
  double UpdateMs_ = 0.0, RefineMs_ = 0.0;
  double ClassMs_ = 0.0;
  double MeshMs_ = 0.0, BuildingMs_ = 0.0, BuildingDagMs_ = 0.0, BuildingDecodeMs_ = 0.0;
};

} // namespace outshine::World
#endif
