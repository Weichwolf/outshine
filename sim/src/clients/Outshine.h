/* THE PICTURE HALF. A renderer over one simulation (clients/Sim.h), and the ONLY thing in the tree
 * that builds a scene: it collects the world's products — tile meshes, the LOD cut, the class grid,
 * the water and building surfaces, the grown tree — and puts them into the picture. The world never
 * calls back into it. Every client is an entry point plus an output medium over this object, so what
 * one client shows the other cannot fail to show. */
#ifndef OUTSHINE_H
#define OUTSHINE_H

#include <cstdint>
#include <string>

#include <optional>

#include "BuildingMesh.h"
#include "DrawSet.h"
#include "ExposureParams.h"
#include "ForestDraw.h"
#include "FrameTelemetry.h"
#include "MemoryTelemetry.h"
#include "Renderer.h"
#include "Sim.h"
#include "StandField.h"
#include "TreePrototype.h"

namespace outshine::Clients {

class Outshine {
public:
  using Assets = Sim::Assets;
  using Stance = Sim::Stance;

  /* WHERE THE PICTURE LANDS, and nothing else: the size is the scene's (clients/Scene.h), read
   * where the device is created, so no caller can hand in a frame the ladder does not measure. */
  struct Gpu {
    const char *Canvas = nullptr;   /* null = offscreen */
  };
  struct Progress {
    float Fraction = 0.0f;
    bool Resident = false;
  };
  /* WHAT A CLIENT MAY REPORT. An entry point states what it saw without reaching into the peers to
   * find out — a hand into Renderer is a hand that can also build (make verify-clients). */
  struct Counters {
    int Draws = 0;
    long Triangles = 0, TreeTriangles = 0, TreeStands = 0;
    unsigned BuildingVerts = 0;
    long Built = 0;
    double WorldMs = 0.0, MeshMs = 0.0, UploadMs = 0.0;
    double BuildingMs = 0.0, BuildingDecodeMs = 0.0;
    double GroundAslM = 0.0, AltAslM = 0.0;
    float Fraction = 0.0f;
    bool Resident = false;
  };
  /* A DEVICE IS A PROMISE AND A TILE SERVER IS A NETWORK, so bring-up cannot finish in a
   * constructor — and it may not stand still either, because the thread it would stand still on is
   * the one those promises complete on. It is a state machine and not a pile of booleans: every
   * call below states the phase it needs, and one consumer — the subject bench — deliberately stops
   * at `Prepared`.
   *
   * `Prepared` and `Playing` are the two RESTING states; the four between them are turns of Step().
   * `Loading` and `Playing` are the APPLICATION's two working states and neither is the renderer's
   * business: what is loading and when that ends is Outshine's, the renderer is only told which
   * frame to draw. Loading holds the world back; Playing never holds anything back — what arrives
   * after it enters the picture beside the loop, never instead of it. */
  enum class Phase { Declared, Device, Prepared, Ground, Stars, Loading, Playing, Failed };

  Outshine(const Scene &scene, const Assets &assets);

  void SetTilesBase(const std::string &url) { if (Phase_ == Phase::Declared) Sim_.SetTilesBase(url); }
  void SetStance(const Stance &s) { if (Phase_ == Phase::Declared) Sim_.SetStance(s); }

  /* WHAT AN ANIMATION CHANNEL MAY MOVE PER FRAME (Animation.h). Each one ends in a renderer setter
   * that is safe to call inside a run; the quantities that are NOT here are the ones a mid-run
   * change would have to re-bake. */
  void SetFovDeg(double deg);
  void SetExposureCompEv(double ev);
  void SetSkyOffsetS(double s);

  /* THE FPS SPECTRUM AND THE STREAMING STAGES RIDE ALONG (FrameTelemetry.h, StreamTelemetry.h).
   * Frame() and Stream() feed them and one row per second goes out, so a client that only draws is
   * observable without declaring a measurement. A null sink makes the whole path a clock read. */
  void SetTelemetrySink(TelemetrySink *sink) { Sim_.SetTelemetrySink(sink); }
  void SetTelemetryIdentity(TelemetrySource *id) { Sim_.SetTelemetryIdentity(id); }

  /* TWO ASKS, because one consumer stops between them: the subject bench judges a plant with no
   * world and no network at all, so it prepares and never opens. Both only STATE the wish; Step()
   * is what carries it out, one turn at a time, and Busy() says whether another turn is owed. */
  bool Prepare(const Gpu &gpu);
  void Open();
  Phase Step();
  bool Busy() const;
  Phase Stage() const { return Phase_; }

  /* The wind clock is not the sky clock: the sun stands where the scene declared it while the flow
   * runs. It is the one quantity a sequence moves. */
  void SetWindClock(double s);

  void Look(const Stance &s);

  /* One streaming pass and the collection that follows it. It decides nothing about images: the
   * caller renders, or does not. */
  Progress Stream(double nowMs);
  void Frame();
  Counters Measured() const;
  /* What the last frame spent collecting instances, milliseconds of the thread that draws. Zero on
   * a frame that reused the standing collection, which is most of them — so it is a per-frame
   * column and never a mean. */
  double CollectMs() const { return CollectMs_; }

  Render::Renderer &Renderer() { return R_; }
  Sim &Simulation() { return Sim_; }
  const Sim &Simulation() const { return Sim_; }

private:
  /* ONE TURN OF EACH BRING-UP PHASE. Each ends by moving Phase_ on, by leaving it where it is
   * because the answer has not arrived, or by failing it. */
  void AwaitDevice();
  void AwaitGround();
  void AwaitStars();
  /* THE FIRST LOAD, AND IT IS NOT A WARM-UP. Outshine does not converge on residency as a
   * side-effect of drawn world frames — it declares the standpoint, asks the streamer what is
   * missing, takes it, and renders a progress bar at full rate meanwhile. The world is held back
   * until every stream is in; nothing half-arrived is ever presented. There is no ceiling: a tile
   * server that never answers is a tile server that never answers, and a picture of a half-loaded
   * scene would be the worse report. Ends in `Playing`, which nothing returns from. */
  void LoadPass();

  /* THE PRODUCTS, TAKEN. Every renderer call that puts a piece of the world into the picture starts
   * here; the world knows none of them. */
  void Collect();
  void CollectTiles();
  void CollectClass();
  /* Every instance the generators produced for the regions that stand, gathered into one frame. */
  void CollectStands();
  /* How far the eye may walk before the collection is made again, metres. */
  double RecollectStepM() const;
  /* The frame clock, around whichever picture the renderer just drew. */
  double OpenFrame();
  void CloseFrame(double startedMs);

  Sim Sim_;
  ExposureParams Exposure_;
  Render::Renderer R_;
  FrameTelemetry Frames_;
  MemoryTelemetry Memory_{Sim_.Scenery(), R_, Sim_};

  /* WHAT A FOOTPRINT BECOMES. It lives here because this is the one translation unit that may
   * name both a generator and the world the outlines arrive in. */
  Generators::BuildingMesh Structures_;
  std::optional<Generators::TreePrototype> Tree_;
  std::optional<Generators::ForestDraw> TreeDraw_;
  Generators::DrawSet Draws_;
  StandField Stands_;

  /* NO TILE TABLE HERE, and that is the point: the renderer's slot goes back to the world as the
   * tile's handle, and the world hands it out again in its draw list — so a frame costs no lookup. */
  uint64_t ClassVersion_ = 0, WaterSeq_ = 0, BuildingSeq_ = 0;
  uint64_t StandingRegions_ = 0;
  /* Where the standing collection was made, in its own frame's metres — the eye's distance from it
   * is what says the cut has gone stale. */
  TangentFrame CollectedFrom_;
  bool Collected_ = false;
  double CollectMs_ = 0.0;
  double UploadMs_ = 0.0;
  double LastFrameMs_ = 0.0, ClockOriginMs_ = 0.0;
  /* When the phase that is running now started asking, so a wait that will never end is named
   * rather than counted in turns whose length nobody declared. */
  double PhaseFromMs_ = 0.0;
  double LoadFromMs_ = 0.0, LoadMovedMs_ = 0.0, LoadSaidMs_ = 0.0;
  long LoadPasses_ = 0;
  int LoadSettledWas_ = -1;
  Phase Phase_ = Phase::Declared;
};

} // namespace outshine::Clients
#endif
