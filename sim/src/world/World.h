/* Multi-LOD terrain streaming: a chunked-LOD quadtree that feeds Renderer a per-frame draw list of
 * the LOD cut. A node draws ITSELF or its four children, never both.
 *
 * THE CORRECTED walk.h SEMANTICS, because getting it wrong opens holes in the world: view distance may
 * only PREVENT a split — a child past the view radius makes the parent stay a drawn LEAF (detail
 * dropped, coverage NEVER), and children's viability is tested side-effect-free BEFORE the parent is
 * replaced. Refinement, Budgets und Konstanten: doc/world/terrain.md, Abschnitt 2. */
#ifndef WORLD_H
#define WORLD_H

#include <cstdint>
#include <vector>
#include "SpriteDraw.h"
#include "UnitDraw.h"
#include "BuildingField.h"
#include "WaterField.h"
#include "ClassField.h"
#include "ClusterDag.h"

namespace outshine::Render { class Renderer; }
namespace outshine::Units { class Unit; class UnitRegistry; struct UnitSignature; struct UnitPose; }
namespace outshine { class WeatherProvider; }

namespace outshine::World {

class VegetationTemplates;

/* THE STYLE of a laid smoke trail, because two of them exist and they differ in nothing but their
 * numbers: a solid motor lays white smoke in 250 m steps for 20 s, a holed airframe lays black smoke
 * in 60 m steps for 40 s. Same crumbs, same segments, same integration-free memory. */
struct TrailStyle {
  double StepM, LifeS;
  float R0M, RMaxM, Alpha0, Radiance;
};

class World {
public:
  World();
  /* Closes the tile stream, which on both platforms means retiring whatever is still fetching or
   * meshing off this thread. A pool outliving its process is a crash at static destruction. */
  ~World();

  /* BORROWED: the cast of the world is simulation state and lives in the core library, owned by the
   * client — fb-gym needs the same registry without linking any of world/. This is the drawing side. */
  void SetUnits(const Units::UnitRegistry *units) { Units_ = units; }
  const Units::UnitRegistry *Units() const { return Units_; }

  /* WHICH unit the camera is riding, by id: it is drawn from the inside otherwise, and an eye sitting
   * at the model origin sees the cockpit tub from within. -1 = draw every unit. */
  void SetEyeUnitId(int id) { EyeUnitId_ = id; }

  /* THE MISSION CLOCK, because an effect is an AGE: a flare's published bloom time, a chaff cloud's,
   * and the moment a motor was seen to light are all stamped in sim seconds, and `nowMs` beside them is
   * a wall clock. Until a client sets this, no age-dependent effect is drawn at all — a boundary
   * defence, not a default, because an invented age would be an invented picture. */
  void SetSimTimeS(double s) { SimTimeS_ = s; HaveSimTime_ = true; }

  /* BORROWED for the same reason: the atmosphere is simulation state (core/WeatherProvider), and the
   * drawing side only ever ASKS it — cover, cloud base and wind for the cloud rebuild. Null until a
   * client sets one, and nothing here draws weather today. */
  /* BORROWED: the client owns the table (it also hands the same rows to the renderer). Null = no
   * near-field ground field is built at all. */
  void SetVegetation(const VegetationTemplates *veg) { Veg_ = veg; Cls_.SetVegetation(veg); }

  /* THE CLASS, for every consumer that is not a fragment: -1 where OSM has no datum, which is a
   * state and not a default (doc/render/classification.md). */
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

  /* NOTHING IS STILL ON ITS WAY. Streaming is asynchronous, so a fixed number of passes says nothing
   * about what has arrived; an oracle that wants a picture of the whole scene waits on this instead.
   * Three streams have to be in: the geometry target cut, the OSM building block, and the cluster DAG
   * of the tile that block last decoded. */
  bool Resident() const {
    return TargetTot > 0 && TargetRdy == TargetTot && Vectors.PendingTiles() == 0 &&
           BuildingDagId == 0 && Cls_.Complete();
  }
  int BuildingPendingTiles() const { return Vectors.PendingTiles(); }
  /* Das Dach ueber einem Ort, ASL, oder -1e30 wo kein Grundriss steht. Ein Auge in einer Wand ist
   * kein Standpunkt, und nur diese beiden Felder zusammen koennen das sagen. */
  double RoofAslAt(double lat, double lon) const { return Buildings.RoofAslAt(Vectors, lat, lon); }

  /* THE RESIDENCY COUNTERS, for a moving measurement: a per-frame series that does not settle is the
   * defect, and none of these is visible in a picture. */
  int NodeCount() const { return (int)Nodes.size(); }
  int DrawnLeafCount() const { return (int)DrawnLeaves.size(); }
  long BuiltCount() const { return Built; }
  long EvictedCount() const { return Evicted; }
  double UpdateMs() const { return UpdateMs_; }
  double MeshMs() const { return MeshMs_; }
  double AlbedoMs() const { return AlbedoMs_; }
  double UploadMs() const { return UploadMs_; }
  double BuildingMs() const { return BuildingMs_; }
  double BuildingDecodeMs() const { return BuildingDecodeMs_; }

  /* Off = no fetch, no upload. The client gates this on the day/night fade. */
  void SetNightLights(bool on) { NightLights = on; }

  /* THE SUN'S ELEVATION over the eye, from the same block the client already gates the tile lights on
   * (State's EnvironmentBlock). It reaches the drawing side because a LAMP is the one effect whose
   * existence depends on the light around it — a jet does not switch on its position lights because the
   * renderer feels like it, and one picture may not have two nights in it. */
  void SetSunElevationDeg(float deg) { SunElDeg = deg; }

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
    std::vector<float> verts;              /* the DAG soup: every level's clusters, level 0 first */
    std::vector<Render::DagCluster> clusters;
    int nverts;
    double origin[3];      /* tile-centre ECEF (from the mesh, once built) */
    float err;             /* geometric error (m), valid once haveMesh */
    std::vector<uint8_t> albedo;
    int lightState;        /* night lights: 0 unfetched, 1 decoded (lightInst valid), -1 pending/none */
    std::vector<float> lightInst;   /* decoded sprites: count * 7 floats [posRelAnchor.xyz, radM, col.rgb] */
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
  void CountTargets(int z, long x, long y, const double eye[3], int &total, int &ready) const;  /* target-cut progress */
  void Emit(int idx);
  void AddWork(int idx, int z, long x, long y, const double eye[3], const double fwd[3]);
  void Center(int z, long x, long y, double out[3]) const;
  double SpanM(int z) const;
  void BuildLights(int idx);   /* fetch + decode /t/lights for node idx into its lightInst (rel Anchor) */

  void SurfaceAnchor(int z, long x, long y, double out[3]) const;

  /* The published cast turned into draw records, rebuilt every Update() and handed to the renderer.
   * Reused, so a steady cast allocates nothing after the first frame. */
  void PublishUnits();

  /* THE EFFECTS OF ONE UNIT, from what that unit PUBLISHES and from nothing else: the afterburner bit
   * off its signature, its countermeasure clouds off the same, and — for a round — the trail its own
   * motor laid, measured against the burn time the STORE CATALOGUE states for its type. */
  void AddUnitEffects(const Units::Unit &u, const Units::UnitSignature &sig, const double ecef[3],
                      const double fwd[3], const double right[3], const double up[3], bool isEye);

  void AddSmokeTrail(const Units::Unit &u, const double ecef[3], bool laying, const TrailStyle &st);

  /* WHAT A HIT LOOKS LIKE, from the one thing the unit publishes about being hit: a RISE in its
   * monotone burst count is one detonation and is the only trigger there is. The ball and its column
   * outlive the unit's own frame, so they are kept here rather than in the unit's UnitFx. */
  void SpawnBlast(const double ecef[3], float radiusM, int unitId);
  void AddBlasts();
  /* ...and what is LEFT: a unit whose published register says it can no longer finish the sortie
   * burns, on the spot if it is down and behind itself if it is still flying. */
  void AddWreckFire(const Units::Unit &u, const Units::UnitSignature &sig,
                    const Units::UnitPose &p, const double ecef[3], const double up[3]);
  /* The lamps an airframe carries, placed off its PUBLISHED silhouette (span, length) and lit only
   * while the sun is down — see SetSunElevationDeg. */
  void AddNavLights(const Units::Unit &u, const Units::UnitSignature &sig, const double ecef[3],
                    const double fwd[3], const double right[3], const double up[3]);
  void AddLight(const double ecef[3], float radiusM, float r, float g, float b);
  bool Dark() const;

  /* One breadcrumb of a burning motor's path: WHERE the round was when this frame published it. It is
   * a memory of published poses, never a second integration — the renderer remembers what it was
   * shown, which is the only way a trail can follow a path the pose alone does not carry. */
  struct FxCrumb { double Ecef[3]; double T; };
  struct UnitFx {
    int Id = -1;
    unsigned Touch = 0;
    double FirstSeenS = 0.0;   /* the frame this unit first existed: a round is created at launch */
    uint16_t Hits = 0;         /* last published burst count — the edge, not the level, is the event */
    double BurnSinceS = -1.0;  /* first frame this unit was published as no longer combat-effective */
    std::vector<FxCrumb> Trail;
  };
  UnitFx &Fx(int id);

  /* One detonation, kept as its own record because it belongs to the PLACE and not to the unit: the
   * round that carried it is retired the same tick and the target may fly on out of the frame. */
  struct FxBlast {
    double Ecef[3];
    double T0;
    float RadiusM;
    float Seed;
  };
  std::vector<FxBlast> Blasts_;

  const Units::UnitRegistry *Units_ = nullptr;   /* borrowed, see SetUnits' banner */
  int EyeUnitId_ = -1;
  std::vector<Render::UnitDraw> UnitDraws_;
  std::vector<Render::SpriteDraw> SpriteDraws_;
  std::vector<UnitFx> Fx_;
  double SimTimeS_ = 0.0;
  bool HaveSimTime_ = false;
  float SunElDeg = 90.0f;   /* full day until a client says otherwise: no lamp burns by default */
  const WeatherProvider *Weather_ = nullptr;   /* borrowed, see SetWeather's banner */
  const VegetationTemplates *Veg_ = nullptr;  /* borrowed, see SetVegetation's banner */

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
  long MeshVram;

  bool NightLights;                  /* stream + emit night lights this run */
  double Anchor[3];                  /* light-position ECEF anchor (field origin); set in Open */
  std::vector<int> DrawnLeaves;      /* node indices emitted as drawn leaves this pass (light hosts) */
  std::vector<float> LightBuf;       /* concatenated visible sprite instances handed to the renderer */
  std::vector<uint8_t> LightBytes;   /* scratch for one tile's /t/lights payload */
  int LightsResident;                /* count emitted last pass (log) */

  OsmField Vectors{14, {"buildings", "water_polygons", "water_lines"}};
  ClassField Cls_;
  BuildingField Buildings;
  WaterField Water;
  std::vector<float> WaterVerts;
  uint32_t BuildingVerts = 0;
  std::vector<float> BuildingDagVerts;
  std::vector<Render::DagCluster> BuildingClusters;
  std::vector<float> BuildingSoup;   /* the one tile whose DAG is in flight; empty otherwise */
  int BuildingDagId = 0, BuildingDagSeq = 0;
  bool Opened = false;
  double UpdateMs_ = 0.0;
  double MeshMs_ = 0.0, AlbedoMs_ = 0.0, UploadMs_ = 0.0, BuildingMs_ = 0.0, BuildingDecodeMs_ = 0.0;
};

} // namespace outshine::World
#endif
