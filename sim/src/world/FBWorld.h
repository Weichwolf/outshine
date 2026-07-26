/* FlightBox — FBWorld: multi-LOD terrain streaming for the WebGPU renderer. Owns a chunked-LOD
 * QUADTREE (Cesium-style) and feeds FBRenderer a per-frame draw list of the LOD cut. Ports the old
 * engine's streaming from command_center/tiles/walk.h (priority refinement) + lru.h (grace-period
 * eviction, demand-scaled cache, non-blocking byte cache), with the CORRECTED coverage semantics.
 *
 * Refinement (recursive descent from a zROOT ring): a node draws ITSELF or its four CHILDREN, never
 * both. It splits only when its frustum-weighted screen-space error exceeds the threshold AND all
 * four children are viable AND all four are resident (child-ready gating — the parent keeps drawing
 * until the finer level has fully landed, so refinement never opens a hole). Fetch/mesh/albedo/upload
 * are budgeted per frame (lru.h bake-budget), spent worst-first (nearest, in-frustum).
 *
 * CORRECTED walk.h semantics (sim-critic): view distance may only PREVENT a split — a child past the
 * view radius makes the parent stay a drawn leaf (detail dropped, coverage NEVER). Children's
 * viability (map bounds + view distance) is tested SIDE-EFFECT-FREE before the parent is replaced;
 * the walk.h bug treated "past view" like "off-map" at the split and voided the parent's quadrant. */
#ifndef FBWORLD_H
#define FBWORLD_H

#include <cstdint>
#include <vector>

namespace FlightBox {

class FBRenderer;
class FBUnitRegistry;

class FBWorld {
public:
  FBWorld();

  /* The cast of the world, BORROWED: the registry itself lives in the core library (units/
   * FBUnitRegistry — see its banner for why it moved out of this class) and is owned by the client that
   * owns the units. FBWorld only carries a pointer to it, for the drawing side (unit markers,
   * render/stages/FBUnitsStage) — a renderer needs to know what is out there, but it is not where that
   * knowledge belongs, and fb-gym must have the same registry without linking any of world/. */
  void SetUnits(const FBUnitRegistry *units) { Units_ = units; }
  const FBUnitRegistry *Units() const { return Units_; }

  /* Open the streamer. `viewMeters` is the view radius (FB_VIEW_KM * 1000, the old w3_view_m). */
  bool Open(FBRenderer *renderer, const char *tilesBase, double lat, double lon, int grid,
            double viewMeters, int albedoTS);

  /* One refinement pass for a camera at ground track (camLat,camLon), ECEF eye + forward. Budgeted;
   * `nowMs` drives the once-per-second counter log (leaves/drawn/pending/evicted/vram). */
  void Update(double camLat, double camLon, const double eyeEcef[3], const double fwdEcef[3],
              double nowMs);

  /* Currently VIEWED ground mode (TAB): 0 = OSM, 1 = aerial photo. The mode that is NOT the boot default
   * (SetDefaultMode) is the lazy OVERLAY — fetched only while viewed, for on-screen tiles, then cached. */
  void SetGroundMode(int photo) { Photo = photo != 0; }
  /* Boot default = the EAGER base albedo source uploaded with every tile (1 = photo/Esri, 0 = OSM). */
  void SetDefaultMode(int photo) { DefaultPhoto = photo != 0; }

  /* Boot/teleport convergence: fraction of the geometry target cut whose leaves are GPU-ready this pass
   * (0..1). The app shows a LOADING SCREEN (and keeps JSBSim frozen) until this crosses the threshold,
   * then switches to the scene — so the first scene frame is already at full target resolution, no
   * low-res transition. TargetTot/TargetRdy are the raw counts for the "resident/total" readout. */
  float LoadProgress() const { return TargetTot > 0 ? (float)TargetRdy / (float)TargetTot : 0.0f; }
  int TargetTotal() const { return TargetTot; }
  int TargetReadyN() const { return TargetRdy; }

  /* Night-light streaming (EVS night). When on, drawn leaves stream /t/lights (lowest priority) and
   * the decoded ground-level ECEF sprite instances are handed to the renderer each pass. Off = no
   * fetch, no upload (the renderer draws none). The app gates this on the day/night fade. */
  void SetNightLights(bool on) { NightLights = on; }

private:
  struct Node {
    int z;
    long x, y;
    unsigned touch;
    int stale;
    int slot;              /* FBRenderer table slot, -1 until uploaded */
    int haveMesh, haveAlbedo;
    int alt;               /* the NON-base overlay albedo: 0 unfetched, 1 attached, -1 none/give-up */
    unsigned readyPass;    /* pass the GPU upload was issued; drawable only in a LATER pass (2-phase) */
    unsigned altPass;      /* pass the OVERLAY upload was issued (2-phase on the overlay axis) */
    unsigned emitPass;     /* last pass this tile was drawn — lets a mode switch keep it (old mode) vs re-coarsen */
    float *verts;
    int nverts;
    double origin[3];      /* tile-centre ECEF (from the mesh, once built) */
    float err;             /* geometric error (m), valid once haveMesh */
    std::vector<uint8_t> albedo;
    int lightState;        /* night lights: 0 unfetched, 1 decoded (lightInst valid), -1 pending/none */
    std::vector<float> lightInst;   /* decoded sprites: count * 7 floats [posRelAnchor.xyz, radM, col.rgb] */
  };
  struct Work { int idx; double prio; };

  int Ensure(int z, long x, long y);                              /* node index (creates on miss) */
  bool Uploaded(const Node &n) const { return n.haveMesh && n.haveAlbedo && n.slot >= 0; }
  /* Two-phase commit: a tile is drawable/splittable only ONE pass after its GPU upload was issued, so
   * the WriteTexture is submitted+visible before any draw references the layer (no empty-layer frame). */
  bool Ready(const Node &n) const { return Uploaded(n) && Pass > n.readyPass; }
  /* Mode-aware readiness: drawable IN THE CURRENT DISPLAY MODE. When viewing the base mode, base-ready is
   * enough; when viewing the overlay mode, the overlay must be resolved too (attached + 2-phase-committed,
   * or a genuine hole = the base is the only + stable option). Drives the refine/cover decisions so a fine
   * tile is never shown in the WRONG mode while its overlay streams — the coarser right-mode parent holds. */
  bool ReadyMode(const Node &n) const {
    if (!Ready(n)) return false;
    if (Photo == DefaultPhoto) return true;
    /* +1 matches the renderer's overlay 2-phase (FrameNo > PhotoUpTick+1) so FBWorld only refines to a
     * tile on the frame the renderer actually draws its overlay layer — no 1-frame wrong-mode gap. */
    return n.alt == -1 || (n.alt == 1 && Pass > n.altPass + 1);
  }
  /* Coverage for the refine/hold decision: a fine tile counts as covering iff it is mode-ready (draw the
   * right mode) OR it was already drawn last pass (a TAB switch KEEPS the resident fine tile in the old
   * mode until its new-mode overlay lands — no re-coarsen, no flash). A NEW streaming tile (never drawn)
   * only counts once mode-ready, so it never POPS in the wrong mode. */
  bool CoversInMode(const Node &n) const { return ReadyMode(n) || (Ready(n) && n.emitPass + 1 >= Pass); }
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

  const FBUnitRegistry *Units_ = nullptr;   /* borrowed, see SetUnits' banner */

  FBRenderer *R;
  bool Photo;            /* currently viewed mode (SetGroundMode) */
  bool DefaultPhoto;     /* boot default = eager base source (SetDefaultMode) */
  int Grid, TS;
  double ViewM, Lat0, Lon0;
  std::vector<Node> Nodes;
  std::vector<int> DrawSlots;
  std::vector<Work> WorkList;
  std::vector<uint8_t> Scratch;
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
};

} // namespace FlightBox
#endif
