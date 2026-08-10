#include "MemoryTelemetry.h"

#include "HeapProbe.h"
#include "StackProbe.h"

namespace outshine::Clients {

namespace {

constexpr double kKb = 1024.0;
constexpr double kMb = 1024.0 * 1024.0;

const StackProbe::Purpose kStacks[StackProbe::kPurposeCount] = {
    StackProbe::Purpose::Frame, StackProbe::Purpose::Class, StackProbe::Purpose::Tile};

std::string StackColumn(StackProbe::Purpose p, const char *suffix) {
  return std::string("stack") + StackProbe::Name(p) + suffix;
}

void PushStack(TelemetryRow &row, const ModuleMemory &m) {
  /* A purpose no thread anywhere has entered leaves its four fields EMPTY: an absent measurement is
   * not a measurement of zero, and the probe's own range is what makes the peak falsifiable. */
  if (m.StackCapacityBytes == 0) {
    for (int i = 0; i < 4; i++) row.Push(std::string());
    return;
  }
  row.Push(m.StackPeakBytes / kKb);
  row.Push(m.StackFloorBytes / kKb);
  row.Push(m.StackLimitBytes / kKb);
  row.Push(m.StackCapacityBytes / kKb);
}

}  // namespace

void MemoryTelemetry::DeclareTelemetry(TelemetrySchema &schema) const {
  schema.Add("heapKB", "KiB");
  schema.Add("heapPeakKB", "KiB");
  schema.Add("heapReservedKB", "KiB");
  schema.Add("workerModules", "count");
  schema.Add("workerHeapKB", "KiB");
  schema.Add("workerHeapPeakKB", "KiB");
  schema.Add("workerReservedKB", "KiB");
  schema.Add("clientHeapKB", "KiB");
  schema.Add("clientReservedKB", "KiB");
  schema.Add("poolTilesKB", "KiB");
  schema.Add("poolVectorsKB", "KiB");
  schema.Add("poolBuildingsKB", "KiB");
  schema.Add("poolWaterKB", "KiB");
  schema.Add("poolClassKB", "KiB");
  schema.Add("poolForestKB", "KiB");
  schema.Add("poolSumKB", "KiB");
  schema.Add("byteCacheKB", "KiB");
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

void MemoryTelemetry::SampleTelemetry(TelemetryRow &row) const {
  /* The frame thread marks its own stack here, which is the only thread this call runs on. */
  StackProbe::Mark();
  HeapProbe::Sample();

  const World::World::Pools pools = World_.HeapPools();
  const std::vector<ModuleMemory> workers = World_.WorkerMemory();
  double workerHeap = 0, workerHeapPeak = 0, workerReserved = 0;
  for (const ModuleMemory &w : workers) {
    workerHeap += w.HeapBytes;
    workerHeapPeak += w.HeapPeakBytes;
    workerReserved += w.ReservedBytes;
  }
  ModuleMemory deepestWorker;
  for (const ModuleMemory &w : workers)
    if (w.StackPeakBytes > deepestWorker.StackPeakBytes) deepestWorker = w;
  const double mainHeap = (double)HeapProbe::Bytes();
  const double mainReserved = (double)HeapProbe::ReservedBytes();
  row.Push(mainHeap / kKb);
  row.Push((double)HeapProbe::PeakBytes() / kKb);
  /* A module that cannot say how much memory it reserved leaves the reserved side of the ledger
   * EMPTY, and the client total with it: a sum missing one term is not a total. */
  if (mainReserved > 0) row.Push(mainReserved / kKb); else row.Push(std::string());
  row.Push((double)workers.size());
  row.Push(workerHeap / kKb);
  row.Push(workerHeapPeak / kKb);
  row.Push(workerReserved / kKb);
  row.Push((mainHeap + workerHeap) / kKb);
  if (mainReserved > 0) row.Push((mainReserved + workerReserved) / kKb); else row.Push(std::string());
  row.Push((double)pools.TileNodes / kKb);
  row.Push((double)pools.Vectors / kKb);
  row.Push((double)pools.Buildings / kKb);
  row.Push((double)pools.Water / kKb);
  row.Push((double)pools.Class / kKb);
  row.Push((double)Forest_.HeapBytes() / kKb);
  row.Push((double)(pools.Sum() + Forest_.HeapBytes()) / kKb);
  row.Push((double)World_.ByteCacheBytes() / kKb);
  row.Push((double)Renderer_.TileMeshBytes() / kMb);
  row.Push((double)Renderer_.ClassVramBytes() / kMb);
  row.Push((double)Renderer_.TemporalVramBytes() / kMb);
  row.Push((double)(Renderer_.TileMeshBytes() + Renderer_.ClassVramBytes() +
                    Renderer_.TemporalVramBytes()) / kMb);
  for (StackProbe::Purpose p : kStacks) {
    ModuleMemory local;
    local.StackPeakBytes = (double)StackProbe::PeakBytes(p);
    local.StackFloorBytes = (double)StackProbe::FloorBytes(p);
    local.StackLimitBytes = (double)StackProbe::LimitBytes(p);
    local.StackCapacityBytes = (double)StackProbe::CapacityBytes(p);
    /* The tile stack sits where the tile work does — threads of this module natively, worker modules
     * in the browser — and what a size gets set from is the deepest of them either way. */
    PushStack(row, p == StackProbe::Purpose::Tile && local.StackCapacityBytes == 0 ? deepestWorker
                                                                                  : local);
  }
}

} // namespace outshine::Clients
