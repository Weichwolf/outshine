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
#include "StreetField.h"
#include "WaterField.h"
#include "ClassField.h"
#include "ClusterDag.h"

namespace outshine { class WeatherProvider; }

namespace outshine::Data {
class SourceSet;
class Transport;
}  // namespace outshine::Data

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

  /* `viewMeters` = the view radius (FB_VIEW_KM * 1000). The registry and the transport are the
   * caller's; the world borrows them for as long as it streams. */
  [[nodiscard]] bool Open(Data::SourceSet &sources, Data::Transport &transport, double lat,
                          double lon, double viewMeters);

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

  /* Fraction of the geometry target cut that is SETTLED: on the GPU, or answered Absent by a stream
   * that will not answer again. The client holds the loading screen (and JSBSim) until this crosses
   * its threshold, so a leaf counted here must be one nothing is still waiting for. */
  float LoadProgress() const { return TargetTot > 0 ? (float)TargetRdy / (float)TargetTot : 0.0f; }
  int TargetTotal() const { return TargetTot; }
  int TargetSettledN() const { return TargetRdy; }
  /* WHAT THE CAMERA CAN SEE of the target cut. Residency is still the whole radius; this is the
   * number that says what a frustum-scoped one would have cost. */
  int TargetInViewN() const { return TargetView; }

  /* WHAT A LOAD IS STILL WAITING FOR, named. Residency is a conjunction, and a conjunction that
   * publishes only its value leaves every stall with as many candidate causes as it has terms. */
  enum class Await { Nothing, TargetCut, BuildingDag, BuildingBlocks, Vectors, Class };
  /* WHAT THE SIMULATION WAS WAITING FOR: the OSM block around the standpoint and the class grid.
   * Nothing about a device or a mesh is in it, so this is what the server target waits on. */
  [[nodiscard]] Await SimWaiting() const;
  [[nodiscard]] bool VectorsResident() const { return SimWaiting() == Await::Nothing; }
  /* NOTHING IS STILL ON ITS WAY. Streaming is asynchronous, so a fixed number of passes says nothing
   * about what has arrived; an oracle that wants a picture of the whole scene waits on this instead.
   * That is the geometry target cut and the cluster DAG of the tile the block last decoded, on top
   * of what the simulation waits for. */
  [[nodiscard]] Await Waiting() const;
  [[nodiscard]] bool Resident() const { return Waiting() == Await::Nothing; }
  int BuildingPendingTiles() const { return Vectors_.PendingTiles(); }
  /* THE OUTLINES AND WHAT THE CORE RESOLVED ON THEM: the decoded vectors, the footprints with the
   * ground under each of them, and the water bodies with their levels. This is where a generator's
   * region is cut from, and the reason it is handed out rather than answered here is that the
   * outline a generator reads and the outline the picture draws must be one line. */
  const OsmField &Vectors() const { return Vectors_; }
  const BuildingField &Footprints() const { return Buildings_; }
  /* WHAT A FOOTPRINT BECOMES, installed at bring-up. The server target installs nothing. */
  void StructureShapes(const StructureMesher *mesher) { Buildings_.Shapes(mesher); }
  const WaterField &WaterBodies() const { return Water_; }
  const StreetField &Ways() const { return Streets_; }

  /* WHERE THE EYE STANDS RELATIVE TO THE PROJECTION. Outside the Web Mercator band there is no root
   * tile and the picture pass draws nothing, which is a STATE the eye holds for as long as it stands
   * there — so it rides a telemetry column and only the crossing reaches the log. */
  [[nodiscard]] bool EyeInMercatorBand() const { return EyeInBand_; }

  /* THE RESIDENCY COUNTERS, for a moving measurement: a per-frame series that does not settle is the
   * defect, and none of these is visible in a picture. */
  int NodeCount() const { return (int)Nodes.size(); }
  int DrawnLeafCount() const { return (int)DrawnLeaves.size(); }
  long long BuiltCount() const { return Built; }
  long long EvictedCount() const { return Evicted; }

  /* WHAT THE PICTURE PASS ASKED THE STREAM FOR AND WHAT BECAME OF IT. Cumulative, like Built, and
   * for the same reason: a bench is a declared run and the per-window delta is the reader's
   * subtraction. Without it a pass that wanted nothing and a pass whose every want was refused
   * write the same row, and those are a settled world and a broken streamer.
   *
   * Wanted == Asked + Capped and Asked == Admitted + Waiting + Absent, both decidable from the CSV
   * alone: a pair that stops adding up is a counter that stopped being taken. */
  struct Admission {
    /* 64 bits, never `long`: the frame is wasm32, where a `long` counter wraps at 2^31 — and Wanted
     * rises once per unfinished leaf per pass, which is 2^31 inside a day of streaming. */
    long long Wanted = 0;     /* target leaves with no mesh yet */
    long long Asked = 0;      /* of those, the ones the pass had budget to ask about */
    long long Admitted = 0;   /* the pool had it: the mesh moved into the node */
    long long Waiting = 0;    /* the pool is still getting it */
    /* No mesh will come for this rung and the answer is final — the world has no ground here, or
     * nothing declares the rung. The two causes are kept apart one level down, in the pool's
     * FetchAbsent against FetchRefused, which is the only place the distinction is acted on. */
    long long Absent = 0;
    long long Capped = 0;     /* never asked: kMeshBuildsPerPass was spent */
  };
  const Admission &Admissions() const { return Adm_; }
  /* Meshes one pass may install. The cap is in ITEMS because an item costs a memcpy — fetch, mesh
   * and DAG all happen off this thread (world/TilePool.h). */
  static constexpr int kMeshBuildsPerPass = 2;
  /* What the world cost this frame, both halves of it. */
  double PassMs() const { return UpdateMs_ + RefineMs_; }
  double ClassMs() const { return ClassMs_; }
  double MeshMs() const { return MeshMs_; }
  double BuildingMs() const { return BuildingMs_ + BuildingDagMs_; }
  double BuildingDecodeMs() const { return BuildingDecodeMs_; }

  /* WHAT THE WORLD HOLDS ON THE HEAP, split by pool so a rise has an owner: the resident tile nodes
   * with their meshes and DAGs, the decoded OSM vectors, what is extruded and
   * tessellated from them, and the class. */
  /* THE POOLS THE WORLD HOLDS, and the sum is over every field: a measured pool outside the sum is
   * a gap the reader has to know to close, which is the same defect as not measuring it. */
  struct Pools {
    size_t TileNodes = 0, Vectors = 0, Buildings = 0, Water = 0, Streets = 0, Class = 0;
    size_t ByteCache = 0, DemCache = 0, Scheduler = 0;
    size_t Sum() const {
      return TileNodes + Vectors + Buildings + Water + Streets + Class + ByteCache + DemCache +
             Scheduler;
    }
  };
  Pools HeapPools() const;

  /* The field of view is an animation channel, so the ladder has to be able to follow it mid-run. */
  void SetPixelFocalLength(double px) { PixelFocal_ = px; }

private:
  /* WHAT A TILE'S GEOMETRY IS, and Vacant is what the stream's two final answers become
   * (world/TilePool.h Reply::Absent and Reply::Undeclared): there is no ground at this rung and
   * there never will be. It is TERMINAL — the split above it is retracted (Splits) and the coarser
   * rung carries the area, so a leaf that will never arrive stops being something the load waits
   * for. */
  enum class MeshState { Wanted, Held, Vacant };

  struct Node {
    int z;
    long x, y;
    unsigned touch;
    int stale;
    int handle;            /* the collector's name for this tile, -1 until collected */
    MeshState Mesh = MeshState::Wanted;
    /* A CHILD ANSWERED Absent: this node is the finest rung that can cover its own area, and the
     * split below it stays retracted while it is in the cut. The refusal is held HERE and not read
     * off the children, because a child carries no mesh once it is Vacant and is evicted for being
     * untouched — which would re-open the split, re-ask, and leave the ladder flickering. */
    bool SplitRefused = false;
    unsigned readyPass;    /* pass the mesh was collected; drawable only in a LATER pass (2-phase) */
    unsigned emitPass;     /* last pass this tile was drawn — lets a mode switch keep it (old mode) vs re-coarsen */
    std::vector<float> verts;              /* ONE vertex per posting; every level indexes into it */
    std::vector<uint32_t> idx;             /* every level's clusters, level 0 first */
    std::vector<DagCluster> clusters;
    int nverts, nidx;
    double origin[3];      /* tile-centre ECEF (from the mesh, once built) */
    double anchor[3];      /* the z10 ancestor's centre, valid once Held */
    float err;             /* geometric error (m), valid once Held */
  };
  struct Work { int idx; double prio; };

  int Ensure(int z, long x, long y);                              /* node index (creates on miss) */
  [[nodiscard]] bool Taken(const Node &n) const { return n.Mesh == MeshState::Held && n.handle >= 0; }
  /* Two-phase commit: drawable only ONE pass after the mesh was handed over, so a collector's upload
   * is submitted and visible before any draw references it. */
  [[nodiscard]] bool Ready(const Node &n) const { return Taken(n) && Pass > n.readyPass; }
  /* NOTHING MORE WILL HAPPEN TO THIS NODE — drawable, or ground that does not exist. The load waits
   * on the unsettled ones; the picture draws only the ready ones. */
  [[nodiscard]] bool Settled(const Node &n) const { return Ready(n) || n.Mesh == MeshState::Vacant; }
  [[nodiscard]] bool Wants(const Node &n) const { return n.Mesh != MeshState::Vacant && !Taken(n); }
  [[nodiscard]] bool Viable(int z, long x, long y, const double eye[3]) const;  /* map bounds + view (pure) */
  [[nodiscard]] bool WantSplit(int z, long x, long y, const double eye[3]) const;   /* geometry-only refine test */
  /* The refine test the TRAVERSAL uses: geometry, minus any child that is Vacant. */
  [[nodiscard]] bool Splits(int z, long x, long y, const double eye[3]) const;
  int  Find(int z, long x, long y) const;                            /* node idx or -1 (no create) */
  [[nodiscard]] bool CanCover(int z, long x, long y, const double eye[3]) const;    /* subtree fully ready? (pure) */
  void RequestSubtree(int z, long x, long y, const double eye[3], const double fwd[3]);  /* cascade request to targets */
  void RootRing(const Eye &eye, uint32_t rx, uint32_t ry);           /* the pass's top-level tiles */
  void NoteBand(bool inBand, double latDeg, double lonDeg);          /* the crossing, once per crossing */
  int  Descend(int z, long x, long y, const double eye[3], const double fwd[3]);  /* draw traversal; 1 = covered */
  void DrawChildren(int z, long x, long y, const double eye[3], const double fwd[3]);
  void CountTargets(int z, long x, long y, const double eye[3], const double fwd[3], int &total,
                    int &ready, int &inView) const;   /* target-cut progress + the share the camera sees */
  void Emit(int idx);
  /* One tile's mesh out of the stream, if this pass still has budget for it. `budget` falls only on
   * an arrival: a pass that asked twice and got nothing has spent nothing. */
  void AdmitMesh(Node &nd, int &budget);
  void AddWork(int idx, int z, long x, long y, const double eye[3], const double fwd[3]);
  void Center(int z, long x, long y, double out[3]) const;
  double SpanM(int z) const;
  void SurfaceAnchor(int z, long x, long y, double out[3]) const;

  const WeatherProvider *Weather_ = nullptr;   /* borrowed, see SetWeather's banner */
  const VegetationTemplates *Veg_ = nullptr;  /* borrowed, see SetVegetation's banner */

  double PixelFocal_;
  double ViewM, Lat0, Lon0;
  std::vector<Node> Nodes;
  std::unordered_map<uint64_t, int> Index_;   /* packed (z,x,y) -> index into Nodes */
  std::vector<int> DrawnHandles_;
  std::vector<TileMesh> Uncollected_;
  std::vector<int> Retired_;
  std::vector<Work> WorkList;
  unsigned Pass;
  long long Evicted;
  long long Built = 0;                    /* cumulative tile uploads (build completions) — thrash diagnosis */
  Admission Adm_;
  bool EyeInBand_ = true;                 /* a declaration is refused outside it, so a world opens inside */
  long long PrevBuilt = 0, PrevEvicted = 0;   /* deltas for the builds/min + evictions/min rate on [fbworld] */
  double LastLog;
  int Leaves, DrawnReady, Pending;   /* per-pass counters */
  int TargetTot, TargetRdy;          /* geometry target-cut: total leaves / settled (LoadProgress) */
  int TargetView = 0;                /* of those, the share inside the camera cone — published only */
  long MeshVram;

  std::vector<int> DrawnLeaves;      /* node indices emitted as drawn leaves this pass; the only list
                                        that says which mesh the near-field ground field may rasterise */

  OsmField Vectors_{14, OsmLayerNames({OsmLayer::Buildings, OsmLayer::WaterPolygons,
                                       OsmLayer::WaterLines, OsmLayer::Streets,
                                       OsmLayer::StreetPolygons})};
  ClassField Cls_;
  BuildingField Buildings_;
  WaterField Water_;
  void CutKerbs();

  StreetField Streets_;
  /* THE MADE SURFACES AS VALUES, rebuilt each pass out of Streets_ so the field that raises a
   * footprint can find the street in front of it without naming the peer that ingested it. */
  std::vector<WayLine> Kerbs_;
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

const char *AwaitName(World::Await await);

} // namespace outshine::World
#endif
