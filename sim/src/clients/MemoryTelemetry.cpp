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

}  // namespace

void MemoryTelemetry::DeclareTelemetry(TelemetrySchema &schema) const {
  schema.Add("heapKB", "KiB");
  schema.Add("heapPeakKB", "KiB");
  schema.Add("heapCeilingKB", "KiB");
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
  row.Push((double)HeapProbe::Bytes() / kKb);
  row.Push((double)HeapProbe::PeakBytes() / kKb);
  const size_t ceiling = HeapProbe::CeilingBytes();
  if (ceiling) row.Push((double)ceiling / kKb); else row.Push(std::string());
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
    /* A purpose with no thread in this module leaves its fields EMPTY: the browser's tile pool runs
     * in wasm modules of its own, which this probe cannot reach, and an absent measurement is not a
     * measurement of zero. */
    const size_t capacity = StackProbe::CapacityBytes(p);
    if (capacity == 0) {
      for (int i = 0; i < 4; i++) row.Push(std::string());
      continue;
    }
    row.Push((double)StackProbe::PeakBytes(p) / kKb);
    row.Push((double)StackProbe::FloorBytes(p) / kKb);
    row.Push((double)StackProbe::LimitBytes(p) / kKb);
    row.Push((double)capacity / kKb);
  }
}

} // namespace outshine::Clients
