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
  schema.Add("tilesSettled");
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
  /* THE REFUSALS, beside the give-ups and never merged with them: a give-up is a server that is slow
   * and a refusal is a request this tree may not repeat unchanged. A coarse quadrant with no refusal
   * in the row has a different cause from one with them. */
  schema.Add("poolHttpRefused", "count");
  schema.Add("poolMeshRefused", "count");
  schema.Add("poolFetchedMB", "MiB");
  schema.Add("poolMeshTiles", "count");
  schema.Add("poolMeshCpuMs", "ms");
  schema.Add("poolDagMs", "ms");
  schema.Add("poolEvictions", "count");
  schema.Add("poolPosts", "count");
  schema.Add("poolRepeats", "count");
  schema.Add("poolQueued", "count");
  schema.Add("meshWanted", "count");
  schema.Add("meshAsked", "count");
  schema.Add("meshAdmitted", "count");
  schema.Add("meshWaiting", "count");
  schema.Add("meshAbsent", "count");
  schema.Add("meshCapped", "count");
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
  row.Push(Last_.TilesSettled);
  row.Push(Last_.TilesInView);
  row.Push(Last_.VectorTilesPending);
  row.Push(WindowBuilt_);
  row.Push(WindowEvicted_);
  row.Push(Last_.Resident);
  row.Push(LoadMs_);
  row.Push(Last_.Pool.Fetches);
  row.Push(Last_.Pool.FetchMs);
  row.Push(Last_.Pool.FetchBlockedMs);
  row.Push(Last_.Pool.FetchGaveUp);
  row.Push(Last_.Pool.FetchRefused);
  row.Push(Last_.Pool.MeshRefused);
  row.Push(Last_.Pool.FetchedMB);
  row.Push(Last_.Pool.MeshTiles);
  row.Push(Last_.Pool.MeshCpuMs);
  row.Push(Last_.Pool.DagMs);
  row.Push(Last_.Pool.Evictions);
  row.Push(Last_.Pool.Posts);
  row.Push(Last_.Pool.Repeats);
  row.Push(Last_.Pool.QueueDepth);
  row.Push(Last_.Admission.Wanted);
  row.Push(Last_.Admission.Asked);
  row.Push(Last_.Admission.Admitted);
  row.Push(Last_.Admission.Waiting);
  row.Push(Last_.Admission.Absent);
  row.Push(Last_.Admission.Capped);
}

} // namespace outshine::Clients
