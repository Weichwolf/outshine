/* Multi-LOD terrain streaming: a chunked-LOD quadtree that feeds FBRenderer a per-frame draw list of
 * the LOD cut. A node draws ITSELF or its four children, never both.
 *
 * THE CORRECTED walk.h SEMANTICS, because getting it wrong opens holes in the world: view distance may
 * only PREVENT a split — a child past the view radius makes the parent stay a drawn LEAF (detail
 * dropped, coverage NEVER), and children's viability is tested side-effect-free BEFORE the parent is
 * replaced. Refinement, Budgets und Konstanten: doc/world/terrain.md, Abschnitt 2. */
#ifndef FBWORLD_H
#define FBWORLD_H

#include <cstdint>
#include <vector>
#include "FBUnitDraw.h"

namespace FlightBox::Render { class FBRenderer; }
namespace FlightBox::Units { class FBUnitRegistry; }
namespace FlightBox { class FBWeatherProvider; }

namespace FlightBox::World {

class FBWorld {
public:
  FBWorld();

  /* BORROWED: the cast of the world is simulation state and lives in the core library, owned by the
   * client — fb-gym needs the same registry without linking any of world/. This is the drawing side. */
  void SetUnits(const Units::FBUnitRegistry *units) { Units_ = units; }
  const Units::FBUnitRegistry *Units() const { return Units_; }

  /* WHICH unit the camera is riding, by id: it is drawn from the inside otherwise, and an eye sitting
   * at the model origin sees the cockpit tub from within. -1 = draw every unit. */
  void SetEyeUnitId(int id) { EyeUnitId_ = id; }

  /* BORROWED for the same reason: the atmosphere is simulation state (core/FBWeatherProvider), and the
   * drawing side only ever ASKS it — cover, cloud base and wind for the cloud rebuild. Null until a
   * client sets one, and nothing here draws weather today. */
  void SetWeather(const FBWeatherProvider *weather) { Weather_ = weather; }
  const FBWeatherProvider *Weather() const { return Weather_; }

  /* `viewMeters` = the view radius (FB_VIEW_KM * 1000). */
  bool Open(Render::FBRenderer *renderer, const char *tilesBase, double lat, double lon, int grid,
            double viewMeters, int albedoTS);

  /* One budgeted refinement pass; `nowMs` drives the 1 Hz counter log. */
  void Update(double camLat, double camLon, const double eyeEcef[3], const double fwdEcef[3],
              double nowMs);

  /* Currently VIEWED mode (0 = OSM, 1 = photo). Whichever is NOT the boot default is the lazy
   * OVERLAY: fetched only while viewed, for on-screen tiles, then cached. */
  void SetGroundMode(int photo) { Photo = photo != 0; }
  /* The EAGER base albedo source, uploaded with every tile. */
  void SetDefaultMode(int photo) { DefaultPhoto = photo != 0; }

  /* Fraction of the geometry target cut that is GPU-ready. The client holds the loading screen (and
   * JSBSim) until this crosses its threshold. */
  float LoadProgress() const { return TargetTot > 0 ? (float)TargetRdy / (float)TargetTot : 0.0f; }
  int TargetTotal() const { return TargetTot; }
  int TargetReadyN() const { return TargetRdy; }

  /* Off = no fetch, no upload. The client gates this on the day/night fade. */
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
  /* Two-phase commit: drawable only ONE pass after the upload was issued, so the WriteTexture is
   * submitted and visible before any draw references the layer. */
  bool Ready(const Node &n) const { return Uploaded(n) && Pass > n.readyPass; }
  /* Drawable IN THE CURRENT DISPLAY MODE, so a fine tile is never shown in the WRONG mode while its
   * overlay streams — the coarser right-mode parent holds instead. */
  bool ReadyMode(const Node &n) const {
    if (!Ready(n)) return false;
    if (Photo == DefaultPhoto) return true;
    /* +1 mirrors the renderer's own overlay 2-phase, else there is a 1-frame wrong-mode gap. */
    return n.alt == -1 || (n.alt == 1 && Pass > n.altPass + 1);
  }
  /* A TAB switch KEEPS the resident fine tile in the old mode until its new overlay lands (no
   * re-coarsen, no flash); a NEW tile counts only once mode-ready, so it never pops in the wrong one. */
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

  /* The published cast turned into draw records, rebuilt every Update() and handed to the renderer.
   * Reused, so a steady cast allocates nothing after the first frame. */
  void PublishUnits();

  const Units::FBUnitRegistry *Units_ = nullptr;   /* borrowed, see SetUnits' banner */
  int EyeUnitId_ = -1;
  std::vector<Render::FBUnitDraw> UnitDraws_;
  const FBWeatherProvider *Weather_ = nullptr;   /* borrowed, see SetWeather's banner */

  Render::FBRenderer *R;
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

} // namespace FlightBox::World
#endif
