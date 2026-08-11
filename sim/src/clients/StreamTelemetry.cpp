#include "StreamTelemetry.h"

namespace outshine::Clients {

void StreamTelemetry::AddPass(const Pass &p) {
  World_.Add(p.WorldMs);
  Mesh_.Add(p.MeshMs);
  Upload_.Add(p.UploadMs);
  Building_.Add(p.BuildingMs);
  Decode_.Add(p.BuildingDecodeMs);
  Class_.Add(p.ClassMs);
  Populate_.Add(p.PopulateMs);
  WindowBuilt_ += p.Built - PrevBuilt_;
  WindowEvicted_ += p.Evicted - PrevEvicted_;
  PrevBuilt_ = p.Built;
  PrevEvicted_ = p.Evicted;
  Last_ = p;
  Passes_++;
  WindowPasses_++;
}

void StreamTelemetry::MarkResident(double nowMs) {
  if (LoadMs_ == 0.0) LoadMs_ = nowMs - OpenedMs_;
}

void StreamTelemetry::Reset() {
  World_.Reset(); Mesh_.Reset(); Upload_.Reset();
  Building_.Reset(); Decode_.Reset(); Class_.Reset(); Populate_.Reset();
  WindowBuilt_ = WindowEvicted_ = 0;
  WindowPasses_ = 0;
}

void StreamTelemetry::DeclareTelemetry(TelemetrySchema &schema) const {
  schema.Add("streamPasses");
  schema.Add("worldMeanMs", "ms");
  schema.Add("worldMaxMs", "ms");
  schema.Add("meshMeanMs", "ms");
  schema.Add("meshMaxMs", "ms");
  schema.Add("uploadMeanMs", "ms");
  schema.Add("uploadMaxMs", "ms");
  schema.Add("buildingMeanMs", "ms");
  schema.Add("buildingMaxMs", "ms");
  schema.Add("decodeMaxMs", "ms");
  schema.Add("classMeanMs", "ms");
  schema.Add("classMaxMs", "ms");
  schema.Add("populateMeanMs", "ms");
  schema.Add("populateMaxMs", "ms");
  schema.Add("tilesTotal");
  schema.Add("tilesReady");
  schema.Add("tilesInView");
  schema.Add("vectorPending");
  schema.Add("tilesBuilt");
  schema.Add("tilesEvicted");
  schema.Add("resident");
  schema.Add("loadMs", "ms");
  schema.Add("poolHttpGets", "count");
  schema.Add("poolHttpMs", "ms");
  schema.Add("poolFetchBlockedMs", "ms");
  schema.Add("poolHttpGaveUp", "count");
  schema.Add("poolFetchedMB", "MiB");
  schema.Add("poolMeshTiles", "count");
  schema.Add("poolMeshCpuMs", "ms");
  schema.Add("poolDagMs", "ms");
  schema.Add("poolEvictions", "count");
}

void StreamTelemetry::SampleTelemetry(TelemetryRow &row) const {
  row.Push(WindowPasses_);
  row.Push(World_.Mean());   row.Push(World_.Max);
  row.Push(Mesh_.Mean());    row.Push(Mesh_.Max);
  row.Push(Upload_.Mean());  row.Push(Upload_.Max);
  row.Push(Building_.Mean()); row.Push(Building_.Max);
  row.Push(Decode_.Max);
  row.Push(Class_.Mean()); row.Push(Class_.Max);
  row.Push(Populate_.Mean()); row.Push(Populate_.Max);
  row.Push(Last_.TilesTotal);
  row.Push(Last_.TilesReady);
  row.Push(Last_.TilesInView);
  row.Push(Last_.VectorTilesPending);
  row.Push((int)WindowBuilt_);
  row.Push((int)WindowEvicted_);
  row.Push(Last_.Resident);
  row.Push(LoadMs_);
  row.Push((int)Last_.Pool.Fetches);
  row.Push(Last_.Pool.FetchMs);
  row.Push(Last_.Pool.FetchBlockedMs);
  row.Push((int)Last_.Pool.FetchGaveUp);
  row.Push(Last_.Pool.FetchedMB);
  row.Push((int)Last_.Pool.MeshTiles);
  row.Push(Last_.Pool.MeshCpuMs);
  row.Push(Last_.Pool.DagMs);
  row.Push((int)Last_.Pool.Evictions);
}

} // namespace outshine::Clients
