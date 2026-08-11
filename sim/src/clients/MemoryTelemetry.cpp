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

}  // namespace

void MemoryTelemetry::DeclareTelemetry(TelemetrySchema &schema) const {
  schema.Add("heapKB", "KiB");
  schema.Add("heapPeakKB", "KiB");
  schema.Add("heapReservedKB", "KiB");
  schema.Add("poolTilesKB", "KiB");
  schema.Add("poolVectorsKB", "KiB");
  schema.Add("poolBuildingsKB", "KiB");
  schema.Add("poolWaterKB", "KiB");
  schema.Add("poolClassKB", "KiB");
  schema.Add("poolRegionsKB", "KiB");
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
  /* ONE LINEAR MEMORY, so the client's heap IS this heap: the four worker columns and the two client
   * totals beside them were the ledger's way of saying the pool lived somewhere it could not
   * measure, and there is no longer a second module to sum over. */
  const double reserved = (double)HeapProbe::ReservedBytes();
  row.Push((double)HeapProbe::Bytes() / kKb);
  row.Push((double)HeapProbe::PeakBytes() / kKb);
  /* A module that cannot say how much memory it reserved leaves the reserved side EMPTY: an unknown
   * is not a zero. */
  if (reserved > 0) row.Push(reserved / kKb); else row.Push(std::string());
  row.Push((double)pools.TileNodes / kKb);
  row.Push((double)pools.Vectors / kKb);
  row.Push((double)pools.Buildings / kKb);
  row.Push((double)pools.Water / kKb);
  row.Push((double)pools.Class / kKb);
  row.Push((double)Sim_.GeneratorHeapBytes() / kKb);
  row.Push((double)(pools.Sum() + Sim_.GeneratorHeapBytes()) / kKb);
  row.Push((double)World_.ByteCacheBytes() / kKb);
  row.Push((double)Renderer_.TileMeshBytes() / kMb);
  row.Push((double)Renderer_.ClassVramBytes() / kMb);
  row.Push((double)Renderer_.TemporalVramBytes() / kMb);
  row.Push((double)(Renderer_.TileMeshBytes() + Renderer_.ClassVramBytes() +
                    Renderer_.TemporalVramBytes()) / kMb);
  for (StackProbe::Purpose p : kStacks) PushStack(row, p);
}

} // namespace outshine::Clients
