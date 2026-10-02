#include "EngineHeld.h"
#include "TerrainPress.h"

#include <chrono>
#include <ratio>
#include <format>
#include <utility>

namespace outshine {

void Engine::State::PublishEarthworkMeasurements(const Generators::PressedTerrain &pressed,
                                                 double longestSliceMs) {
  Published.RecordMetric("ground: pressing gather", pressed.GatherMs, "ms");
  Published.RecordMetric("ground: pressing decide", pressed.DecideMs, "ms");
  Published.RecordMetric("ground: pressing buckets", pressed.BucketMs, "ms");
  Published.RecordMetric("ground: pressing reject", pressed.RejectMs, "ms");
  Published.RecordMetric("ground: pressing apply", pressed.ApplyMs, "ms");
  Published.RecordMetric("ground: pressing write", pressed.WriteMs, "ms");
  Published.RecordMetric("ground: pressing floors", pressed.FloorsMs, "ms");
  Published.RecordMetric("ground: longest gather slice", pressed.LongestGatherMs, "ms");
  Published.RecordMetric("ground: longest decide slice", pressed.LongestDecideMs, "ms");
  Published.RecordMetric("ground: longest reject slice", pressed.LongestRejectMs, "ms");
  Published.RecordMetric("ground: longest initialize slice", pressed.LongestInitializeMs, "ms");
  Published.RecordMetric("ground: longest apply slice", pressed.LongestApplyMs, "ms");
  Published.RecordMetric("ground: longest write slice", pressed.LongestWriteMs, "ms");
  Published.RecordMetric("ground: longest reproject slice", pressed.LongestReprojectMs, "ms");
  Published.RecordMetric("ground: longest floors slice", pressed.LongestFloorsMs, "ms");
  Published.RecordMetric(
      "ground: lattice nodes the stamps pressed", static_cast<double>(pressed.Nodes), "nodes");
  Published.RecordMetric("ground: stamps refused as STRUCTURES, past the earthwork bound",
                         static_cast<double>(pressed.Structures),
                         "yields");
  Published.RecordMetric("ground: nodes held where a stamp still asked past the bound",
                         static_cast<double>(pressed.Held),
                         "nodes");
  Published.RecordMetric("ground: and the deepest it cut", pressed.DeepestM, "m");
  Published.RecordMetric("ground: and the highest it filled", pressed.RaisedM, "m");
  for (const auto &[what, floors] :
       {std::pair{"pads", &pressed.Pads}, std::pair{"corridor pieces", &pressed.Corridors}}) {
    Published.RecordMetric(std::format("ground: {} with a lattice node inside", what),
                           static_cast<double>(floors->Stamps),
                           "stamps");
    Published.RecordMetric(std::format("ground: {} no lattice node reaches", what),
                           static_cast<double>(floors->Unreached),
                           "stamps");
    Published.RecordMetric(std::format("ground: nodes inside those {}", what),
                           static_cast<double>(floors->Nodes),
                           "nodes");
    Published.RecordMetric(std::format("ground: of those {} nodes, another stamp decided", what),
                           static_cast<double>(floors->Contested),
                           "nodes");
    Published.RecordMetric(
        std::format("ground: nodes inside {} above their plane after the press, worst", what),
        floors->AboveM,
        "m");
    Published.RecordMetric(
        std::format("ground: nodes inside {} that fill, below it after the press, worst", what),
        floors->BelowM,
        "m");
    Published.RecordMetric(
        std::format("ground: nodes inside {} that do not fill, below it, worst", what),
        floors->UnfilledM,
        "m");
    Published.RecordMetric(
        std::format("ground: those {} nodes above it before the press, worst", what),
        floors->WasAboveM,
        "m");
    Published.RecordMetric(
        std::format("ground: those filling {} nodes below it before the press, worst", what),
        floors->WasBelowM,
        "m");
  }
  const double pressingMs =
      pressed.GatherMs + pressed.DecideMs + pressed.WriteMs + pressed.FloorsMs;
  Published.RecordMetric("ground: of that, pressing", pressingMs, "ms");
  Published.RecordMetric("ground candidate: earthworks", pressingMs, "ms");
  Published.RecordMetric("ground candidate: longest earthwork slice", longestSliceMs, "ms");
}

void Engine::State::PublishStreetGraphMeasurements(const Ground::StreetGraphBuilder::Built &mapped,
                                                   double longestSliceMs) {
  Published.RecordMetric("network: ways it holds", static_cast<double>(mapped.Ways), "ways");
  Published.RecordMetric("network: laying ways", mapped.LayMs, "ms");
  Published.RecordMetric("network: weaving topology", mapped.WeaveMs, "ms");
  Published.RecordMetric("network: beginning weave", mapped.BeginWeaveMs, "ms");
  Published.RecordMetric("network: cleaning weave temporaries", mapped.CleanupWeaveMs, "ms");
  Published.RecordMetric(
      "network: longest weave cleanup slice", mapped.CleanupWeaveLongestMs, "ms");
  Published.RecordMetric("network: longest weave slice", mapped.WeaveLongestMs, "ms");
  Published.RecordMetric("network: longest way sort slice", mapped.WeaveSlices.SortMs, "ms");
  Published.RecordMetric("network: longest way merge slice", mapped.WeaveSlices.MergeMs, "ms");
  Published.RecordMetric("network: longest way reserve slice", mapped.WeaveSlices.ReserveMs, "ms");
  Published.RecordMetric("network: longest way copy slice", mapped.WeaveSlices.CopyMs, "ms");
  Published.RecordMetric("network: beginning snap", mapped.WeaveSlices.BeginSnapMs, "ms");
  Published.RecordMetric("network: longest snap slice", mapped.WeaveSlices.SnapMs, "ms");
  Published.RecordMetric("network: longest edge creation slice", mapped.WeaveSlices.EdgesMs, "ms");
  Published.RecordMetric("network: longest edge index slice", mapped.WeaveSlices.IndexMs, "ms");
  Published.RecordMetric("network: beginning adjacency", mapped.WeaveSlices.AdjacencyBeginMs, "ms");
  Published.RecordMetric("network: longest adjacency slice", mapped.WeaveSlices.AdjacencyMs, "ms");
  Published.RecordMetric("network: longest tie slice", mapped.WeaveSlices.TieMs, "ms");
  Published.RecordMetric(
      "network: longest weave publish slice", mapped.WeaveSlices.PublishMs, "ms");
  Published.RecordMetric("network: classifying crossings", mapped.CrossingsMs, "ms");
  Published.RecordMetric("network: longest crossing slice", mapped.CrossingsLongestMs, "ms");
  Published.RecordMetric(
      "network: longest crossing setup slice", mapped.CrossingSlices.SetupMs, "ms");
  Published.RecordMetric(
      "network: longest crossing pair slice", mapped.CrossingSlices.TestMs, "ms");
  Published.RecordMetric("network: crossing publication", mapped.CrossingSlices.PublishMs, "ms");
  Published.RecordMetric("network: crossing point span", mapped.CrossingSweep.SpanMs, "ms");
  Published.RecordMetric("network: crossing segment list", mapped.CrossingSweep.SegmentsMs, "ms");
  Published.RecordMetric("network: crossing grid", mapped.CrossingSweep.GridMs, "ms");
  Published.RecordMetric("network: crossing cell filing", mapped.CrossingSweep.FilingMs, "ms");
  Published.RecordMetric("network: crossing pair tests", mapped.CrossingSweep.TestMs, "ms");
  Published.RecordMetric("network: crossing candidate pairs",
                         static_cast<double>(mapped.CrossingSweep.CandidatePairs),
                         "pairs");
  Published.RecordMetric("network: crossing cache", mapped.CrossingSweep.CacheMs, "ms");
  Published.RecordMetric("network: elevating nodes", mapped.ElevateMs, "ms");
  Published.RecordMetric("network: beginning elevation", mapped.BeginElevationMs, "ms");
  Published.RecordMetric("network: longest elevation slice", mapped.ElevateLongestMs, "ms");
  Published.RecordMetric(
      "network: longest node sample slice", mapped.ElevationSlices.SampleNodesMs, "ms");
  Published.RecordMetric(
      "network: longest point write slice", mapped.ElevationSlices.WritePointsMs, "ms");
  Published.RecordMetric("network: longest station slice", mapped.ElevationSlices.StationsMs, "ms");
  Published.RecordMetric("network: longest slope slice", mapped.ElevationSlices.SlopesMs, "ms");
  Published.RecordMetric("network: longest grade slice", mapped.ElevationSlices.GradesMs, "ms");
  Published.RecordMetric("network: publishing graph", mapped.PublishMs, "ms");
  Published.RecordMetric("network: longest build slice", longestSliceMs, "ms");
  Published.RecordMetric("network: nodes", static_cast<double>(mapped.Nodes), "nodes");
  Published.RecordMetric("network: edges", static_cast<double>(mapped.Edges), "edges");
  Published.RecordMetric("network: nodes where three or more edges meet",
                         static_cast<double>(mapped.Junctions),
                         "nodes");
  Published.RecordMetric(
      "network: points with a height", static_cast<double>(mapped.Elevated.Points), "points");
  Published.RecordMetric("network: points the ground refused a height",
                         static_cast<double>(mapped.Elevated.Refused),
                         "points");
  Published.RecordMetric("network: steepest grade", mapped.Elevated.SteepestGrade, "m/m");
  Published.RecordMetric(
      "network: steepest grade on a sealed way", mapped.Elevated.SteepestSealedGrade, "m/m");
}

void Engine::State::PublishGroundRenderMeasurements(std::chrono::steady_clock::time_point phaseAt) {
  Published.RecordMetric(
      "rebuild: of that, walking it into the proxy", Picture.Standing->BuildMs(), "ms");
  const Core::GeometryBuildSliceMetrics &slices = Picture.Standing->GeometrySlices();
  Published.RecordMetric("geometry slice: shape cooking", slices.CookMs, "ms");
  Published.RecordMetric("geometry slice: scene planning", slices.PlanMs, "ms");
  Published.RecordMetric("geometry slice: subject binding", slices.BindMs, "ms");
  Published.RecordMetric("geometry slice: draw planning", slices.DrawPlanMs, "ms");
  Published.RecordMetric("geometry slice: CPU packing", slices.PackMs, "ms");
  Published.RecordMetric("geometry slice: index upload", slices.IndexMs, "ms");
  Published.RecordMetric("geometry slice: stream completion", slices.FinishMs, "ms");
  Published.RecordMetric("geometry slice: finalization", slices.FinalizeMs, "ms");
  Published.RecordMetric("rebuild: standing render plan", Picture.Standing->PlanMs(), "ms");
  Published.RecordMetric(
      "rebuild: of THAT, copying the subject", Picture.Standing->CarryMs(), "ms");
  Published.RecordMetric(
      "rebuild: standing and submitting INSIDE Build", Picture.Standing->InsideMs(), "ms");
  Published.RecordMetric("rebuild: shaping what was built", Picture.Standing->ReshapeMs(), "ms");
  Published.RecordMetric("rebuild: composing it", Picture.Standing->ComposeMs(), "ms");
  Published.RecordMetric(
      "stand: shaping it a second time", Picture.Standing->ReshapeAgainMs(), "ms");
  Published.RecordMetric("stand: the proxy taking it", Picture.Standing->ProxyStandsMs(), "ms");
  Published.RecordMetric("stand: placing every part", Picture.Standing->PlacesMs(), "ms");
  Published.RecordMetric("stand: dressing them", Picture.Standing->WearsMs(), "ms");
  Published.RecordMetric("stand: their emitted radiance", Picture.Standing->LampsMs(), "ms");
  Published.RecordMetric("stand: the lamps and the key", Picture.Standing->LitMs(), "ms");
  Published.RecordMetric("stand: the medium's own tables", Picture.Standing->MediumMs(), "ms");

  Published.RecordMetric("stand: times the sky was integrated",
                         static_cast<double>(Picture.Standing->SkyIntegrations()),
                         "integrations");
  Published.RecordMetric(
      "stand: sweeping the bounds to frame it", Picture.Standing->FramingMs(), "ms");
  Published.RecordMetric("rebuild: resolving its surface", Picture.Standing->ResolveMs(), "ms");
  Published.RecordMetric("rebuild: and its bounds", Picture.Standing->BoundsMs(), "ms");
  Published.RecordMetric(
      "rebuild: cutting it into clusters", Picture.Standing->Clustering().BuildMs, "ms");
  Published.RecordMetric("cook: clusters with no parent above them",
                         static_cast<double>(Picture.Standing->Clustering().RootClusters),
                         "clusters");
  Published.RecordMetric("cook: clusters in all",
                         static_cast<double>(Picture.Standing->Clustering().Clusters),
                         "clusters");
  Published.RecordMetric(
      "rebuild: planning draw runs", Picture.Standing->TransferMetrics().DrawPlanMs, "ms");
  Published.RecordMetric(
      "rebuild: of the streams, packing them", Picture.Standing->TransferMetrics().PackingMs, "ms");
  Published.RecordMetric("restand: the geometry handed over, digested",
                         Picture.Standing->TransferMetrics().DigestValue(),
                         "");
  Published.RecordMetric(
      "rebuild: digesting what it handed over", Picture.Standing->TransferMetrics().DigestMs, "ms");
  Published.RecordMetric(
      "rebuild: and the device taking them", Picture.Standing->TransferMetrics().UploadMs, "ms");
  Published.RecordMetric(
      "rebuild: mesh admission", Picture.Standing->TransferMetrics().MeshAdmissionMs, "ms");
  Published.RecordMetric(
      "rebuild: index upload", Picture.Standing->TransferMetrics().IndexUploadMs, "ms");
  Published.RecordMetric(
      "rebuild: stream upload", Picture.Standing->TransferMetrics().StreamUploadMs, "ms");
  Published.RecordMetric(
      "rebuild: draw table upload", Picture.Standing->TransferMetrics().TableUploadMs, "ms");
  Published.RecordMetric("rebuild: residency upload attempts",
                         static_cast<double>(Picture.Device.TakeUploadAttempts()),
                         "uploads");
  Published.RecordMetric("rebuild: bytes offered for upload",
                         static_cast<double>(Picture.Device.TakeUploadBytes()),
                         "bytes");
  Published.RecordMetric("rebuild: device buffer allocation attempts",
                         static_cast<double>(Picture.Device.TakeBufferAllocationAttempts()),
                         "buffers");
  Published.RecordMetric("rebuild: staging buffer allocation attempts",
                         static_cast<double>(Picture.Device.TakeStagingAllocationAttempts()),
                         "buffers");
  Published.RecordMetric("rebuild: laying the surface", Picture.Standing->SurfaceMs(), "ms");
  Published.RecordMetric(
      "rebuild: settling placements and lights", Picture.Standing->StandMs(), "ms");
  Published.RecordMetric(
      "rebuild: and the streams to the device", Picture.Standing->SubmitMs(), "ms");
  Published.RecordMetric(
      "rebuild: and handing it to the device took",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count(),
      "ms");
}

}
