#include "MemoryTelemetry.h"

#include "HeapProbe.h"
#include "StackProbe.h"

namespace outshine::Clients {

namespace {

constexpr double kKb = 1024.0;
constexpr double kMb = 1024.0 * 1024.0;

const StackProbe::Purpose kStacks[StackProbe::kPurposeCount] = {
    StackProbe::Purpose::Frame, StackProbe::Purpose::Class, StackProbe::Purpose::Tile,
    StackProbe::Purpose::Region};

std::string StackColumn(StackProbe::Purpose p, const char *suffix) {
  return std::string("stack") + StackProbe::Name(p) + suffix;
}

void PushStack(TelemetryRow &row, StackProbe::Purpose p) {
  /* A purpose no thread has entered leaves its four fields EMPTY: an absent measurement is not a
   * measurement of zero, and the probe's own range is what makes the peak falsifiable. */
  if (StackProbe::CapacityBytes(p) == 0) {
    for (int i = 0; i < 4; i++) row.Push(std::string());
    return;
  }
  row.Push((double)StackProbe::PeakBytes(p) / kKb);
  row.Push((double)StackProbe::FloorBytes(p) / kKb);
  row.Push((double)StackProbe::LimitBytes(p) / kKb);
  row.Push((double)StackProbe::CapacityBytes(p) / kKb);
}

/* An unmeasured quantity leaves its field EMPTY rather than reading as a zero, which is a number a
 * reader would average. */
void PushKnown(TelemetryRow &row, bool known, double kib) {
  if (known) row.Push(kib); else row.Push(std::string());
}

}  // namespace

void MemoryTelemetry::DeclareTelemetry(TelemetrySchema &schema) const {
  schema.Add("heapKB", "KiB");
  schema.Add("heapPeakKB", "KiB");
  schema.Add("heapBreakKB", "KiB");
  schema.Add("heapReservedKB", "KiB");
  schema.Add("heapProbeMs", "ms");
  schema.Add("poolTilesKB", "KiB");
  schema.Add("poolVectorsKB", "KiB");
  schema.Add("poolBuildingsKB", "KiB");
  schema.Add("poolWaterKB", "KiB");
  schema.Add("poolStreetsKB", "KiB");
  schema.Add("poolClassKB", "KiB");
  schema.Add("poolRegionsKB", "KiB");
  schema.Add("byteCacheKB", "KiB");
  schema.Add("poolDemCacheKB", "KiB");
  schema.Add("poolSchedulerKB", "KiB");
  schema.Add("poolSumKB", "KiB");
  schema.Add("heapResidualKB", "KiB");
  schema.Add("devTileMeshMB", "MiB");
  schema.Add("devClassMB", "MiB");
  schema.Add("devTemporalMB", "MiB");
  schema.Add("devSumMB", "MiB");
  for (StackProbe::Purpose p : kStacks) {
    schema.Add(StackColumn(p, "PeakKB"), "KiB");
    schema.Add(StackColumn(p, "FloorKB"), "KiB");
    schema.Add(StackColumn(p, "LimitKB"), "KiB");
    schema.Add(StackColumn(p, "CapKB"), "KiB");
  }
}

/* ONE LINEAR MEMORY, so the client's heap IS this heap: the four worker columns and the two client
 * totals beside them were the ledger's way of saying the pool lived somewhere it could not measure,
 * and there is no longer a second module to sum over. */
void MemoryTelemetry::PushHeap(TelemetryRow &row, size_t live) const {
  const bool known = HeapProbe::LiveBytesKnown();
  PushKnown(row, known, (double)live / kKb);
  PushKnown(row, known, (double)HeapProbe::PeakLiveBytes() / kKb);
  row.Push((double)HeapProbe::BreakBytes() / kKb);
  /* A module that cannot say how much memory it reserved leaves the reserved side EMPTY. */
  const double reserved = (double)HeapProbe::ReservedBytes();
  if (reserved > 0) row.Push(reserved / kKb); else row.Push(std::string());
  row.Push(HeapProbe::SampleCostMs());
}

void MemoryTelemetry::PushPools(TelemetryRow &row, const World::World::Pools &pools,
                                size_t generator) const {
  row.Push((double)pools.TileNodes / kKb);
  row.Push((double)pools.Vectors / kKb);
  row.Push((double)pools.Buildings / kKb);
  row.Push((double)pools.Water / kKb);
  row.Push((double)pools.Streets / kKb);
  row.Push((double)pools.Class / kKb);
  row.Push((double)generator / kKb);
  row.Push((double)pools.ByteCache / kKb);
  row.Push((double)pools.DemCache / kKb);
  row.Push((double)pools.Scheduler / kKb);
  row.Push((double)(pools.Sum() + generator) / kKb);
}

void MemoryTelemetry::PushDevice(TelemetryRow &row) const {
  row.Push((double)Renderer_.TileMeshBytes() / kMb);
  row.Push((double)Renderer_.ClassVramBytes() / kMb);
  row.Push((double)Renderer_.TemporalVramBytes() / kMb);
  row.Push((double)(Renderer_.TileMeshBytes() + Renderer_.ClassVramBytes() +
                    Renderer_.TemporalVramBytes()) / kMb);
}

void MemoryTelemetry::SampleTelemetry(TelemetryRow &row) const {
  /* The frame thread marks its own stack here, which is the only thread this call runs on. */
  StackProbe::Mark();
  const size_t live = HeapProbe::Sample();

  const World::World::Pools pools = World_.HeapPools();
  const size_t generator = Sim_.GeneratorHeapBytes();
  PushHeap(row, live);
  PushPools(row, pools, generator);
  /* WHAT IS HELD THAT NO POOL CLAIMS. Everything this ledger does not name is in here — the
   * allocator's per-chunk overhead, the thread stacks' committed pages, every buffer nobody has
   * given a column — so a rise with no owner has a place to appear instead of no place at all. */
  PushKnown(row, HeapProbe::LiveBytesKnown(),
            ((double)live - (double)(pools.Sum() + generator)) / kKb);
  PushDevice(row);
  for (StackProbe::Purpose p : kStacks) PushStack(row, p);
}

} // namespace outshine::Clients
