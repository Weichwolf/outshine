/* THE STREAMING STAGES, MEASURING THEMSELVES. Every pass hands its per-stage times here and a row
 * goes out once a second on the SAME bus the frame spectrum rides — there is no measurement mode and
 * no bench code path. Whether a frame carried a tile decode or an upload is therefore a FIELD of the
 * row, readable afterwards, and never a reason to discard a run.
 *
 * TWO AGGREGATIONS, and they answer different questions. The MEAN over the second prices a stage —
 * where does the time go. The MAX over the second is the only one that can see a hitch, because a
 * single 60 ms upload inside a 60-frame window moves the mean by 1 ms and is invisible (CLAUDE.md).
 * Both are published for every stage; neither is derivable from the other. */
#ifndef STREAMTELEMETRY_H
#define STREAMTELEMETRY_H

#include "Telemetry.h"
#include "TilePool.h"
#include "World.h"

namespace outshine::Clients {

class StreamTelemetry : public TelemetrySource {
public:
  /* WHAT ONE STREAMING PASS COST AND WHERE IT STANDS — a parameter object, because eleven
   * positional arguments is the flag list I.23 exists to forbid. */
  struct Pass {
    double WorldMs = 0.0, MeshMs = 0.0, UploadMs = 0.0;
    double BuildingMs = 0.0, BuildingDecodeMs = 0.0, ClassMs = 0.0;
    /* The ring's own frame cost. Separate from WorldMs because the ring runs beside World::Refine
     * and not inside it, and a crossing is where the two are told apart. */
    double PopulateMs = 0.0;
    int TilesTotal = 0, TilesSettled = 0, TilesInView = 0, VectorTilesPending = 0;
    long long Built = 0, Evicted = 0;
    bool Resident = false;
    /* The pool's own cumulative ledger, read once per pass: where the wait actually was. */
    World::TilePool::Ledger Pool;
    /* WHAT THE PICTURE PASS ASKED FOR, cumulative like the pool's ledger and for the same reason. */
    World::World::Admission Admission;
  };

  void Open(double nowMs) { OpenedMs_ = nowMs; }
  void AddPass(const Pass &p);
  /* The load is over exactly once, and its duration then stands in every later row: a frame-time
   * distribution read without knowing whether the load was finished is unreadable. */
  void MarkResident(double nowMs);
  double LoadMs() const { return LoadMs_; }
  long long PassCount() const { return Passes_; }
  /* The window closes where FrameTelemetry's does — one Tick, one second, both sources. */
  void Reset();

  const char *TelemetryName() const override { return "stream"; }
  void DeclareTelemetry(TelemetrySchema &schema) const override;
  void SampleTelemetry(TelemetryRow &row) const override;

private:
  struct Stat {
    double Sum = 0.0, Max = 0.0;
    int N = 0;
    void Add(double v) { Sum += v; N++; if (v > Max) Max = v; }
    double Mean() const { return N ? Sum / (double)N : 0.0; }
    void Reset() { Sum = 0.0; Max = 0.0; N = 0; }
  };

  Stat World_, Mesh_, Upload_, Building_, Decode_, Class_, Populate_;
  Pass Last_;
  long long Passes_ = 0, WindowBuilt_ = 0, WindowEvicted_ = 0, PrevBuilt_ = 0, PrevEvicted_ = 0;
  int WindowPasses_ = 0;
  double OpenedMs_ = 0.0, LoadMs_ = 0.0;
};

} // namespace outshine::Clients
#endif
