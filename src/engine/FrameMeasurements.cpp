#include "EngineHeld.h"
#include "Heap.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {

void Engine::State::PublishResourcePayloadMeasurements() {
  if (!Picture.Standing) { return; }
  Published.Places("streamed piece CPU payload capacity",
                   static_cast<double>(Picture.Device.PieceSourceBytes()),
                   "bytes");
  Published.Places("streamed piece slot capacity",
                   static_cast<double>(Picture.Device.PieceSlotBytes()),
                   "bytes");
  Published.Places("height page slot capacity",
                   static_cast<double>(Picture.Device.HeightPageSlotBytes()),
                   "bytes");
  Published.Places("height page CPU payload capacity",
                   static_cast<double>(Picture.Device.HeightPageSourceBytes()),
                   "bytes");
}

void Engine::State::PublishFrameMeasurements() {
  static const Heap::Tag kFrameMeasurementsTag("frame-measurements");
  const Heap::Tagged measuring(kFrameMeasurementsTag);
  if (Heap::ProcessInstrumentationEnabled()) {
    PublishResourcePayloadMeasurements();
    Published.Places(
        "process C++ heap live bytes", static_cast<double>(Heap::LiveBytes()), "bytes");
    for (size_t at = 0; at < Heap::TagCount(); ++at) {
      const char *const tag = Heap::TagAt(at);
      if (tag == nullptr || Heap::TakenAt(at) == 0) { continue; }
      Published.Places(std::string("process C++ bytes allocated under ") + tag,
                       static_cast<double>(Heap::TakenAt(at)),
                       "bytes");
    }
  }
  if (Cost.Advance.Taken() > 0) {
    Published.Places("the step's own time, last", Cost.Advance.LastMs(), "ms");
    Published.Places("the step's own time, least", Cost.Advance.LeastMs(), "ms");
    Published.Places("the step's own time, most", Cost.Advance.MostMs(), "ms");
    Published.Places("steps taken", static_cast<double>(Cost.Advance.Taken()), "steps");
    Published.Places("world update time, most", Cost.Update.MostMs(), "ms");
    const Spent::UpdateComponents &worst = Cost.WorstSuccessfulUpdate;
    if (worst.TotalMs > 0.0) {
      Published.Places("worst successful update: total", worst.TotalMs, "ms");
      Published.Places("worst successful update: streaming and bakes", worst.StreamingMs, "ms");
      Published.Places("worst successful update: piece handoff", worst.PieceHandoffMs, "ms");
      Published.Places("worst successful update: tile restand", worst.RestandMs, "ms");
      Published.Places("worst successful update: structure bakes", worst.BakesMs, "ms");
      Published.Places("worst successful update: world growth", worst.GrowthMs, "ms");
      Published.Places("worst successful update: streaming other",
                       worst.StreamingMs - worst.PieceHandoffMs - worst.RestandMs - worst.BakesMs -
                           worst.GrowthMs,
                       "ms");
      Published.Places("worst successful update: simulation", worst.SimulationMs, "ms");
      Published.Places("worst successful update: ground", worst.GroundMs, "ms");
      Published.Places("worst successful update: vegetation", worst.CrownsMs, "ms");
      Published.Places("worst successful update: other",
                       worst.TotalMs - worst.StreamingMs - worst.SimulationMs - worst.GroundMs -
                           worst.CrownsMs,
                       "ms");
    }
    Published.Places("frame publication time, most", Cost.FramePublication.MostMs(), "ms");
    Published.Places("scene advance time, most", Cost.SceneAdvance.MostMs(), "ms");
    Published.Places("streaming and bake time, most", Cost.Streaming.MostMs(), "ms");
    Published.Places("piece handoff time, most", Cost.PieceHandoff.MostMs(), "ms");
    Published.Places("tile restand time, most", Cost.Restand.MostMs(), "ms");
    const Ground::RestandMetrics &restand = World.Stack.WorstRestand();
    Published.Places("worst tile restand: total", restand.TotalMs, "ms");
    Published.Places("worst tile restand: classification", restand.ClassificationMs, "ms");
    Published.Places("worst tile restand: vectors", restand.VectorsMs, "ms");
    Published.Places("worst vector restand: tile fetch", restand.VectorBuild.FetchMs, "ms");
    Published.Places("worst vector restand: tile parsing", restand.VectorBuild.ParseMs, "ms");
    Published.Places(
        "worst vector restand: longest layer", restand.VectorBuild.LongestLayerMs, "ms");
    Published.Places("worst vector restand: longest layer index",
                     static_cast<double>(restand.VectorBuild.LongestLayerIndex),
                     "index");
    Published.Places("worst vector restand: capacity check", restand.VectorBuild.CapacityMs, "ms");
    Published.Places("worst vector restand: publication", restand.VectorBuild.PublicationMs, "ms");
    Published.Places("worst tile restand: streets", restand.StreetsMs, "ms");
    Published.Places("worst tile restand: water", restand.WaterMs, "ms");
    const Ground::WaterField::IngestMetrics &water = World.Stack.WaterBodies().WorstIngest();
    Published.Places("worst water ingest: total", water.TotalMs, "ms");
    Published.Places("worst water ingest: tile admission", water.AdmissionMs, "ms");
    Published.Places("worst water ingest: height validation", water.ValidationMs, "ms");
    Published.Places("worst water ingest: longest height query", water.LongestQueryMs, "ms");
    Published.Places("worst water ingest: validated points",
                     static_cast<double>(water.ValidationPoints),
                     "points");
    Published.Places("worst water ingest: materialization", water.MaterializationMs, "ms");
    Published.Places("worst tile restand: settlement", restand.SettlementMs, "ms");
    Published.Places("worst tile restand: other",
                     restand.TotalMs - restand.ClassificationMs - restand.VectorsMs -
                         restand.StreetsMs - restand.WaterMs - restand.SettlementMs,
                     "ms");
    Published.Places("structure bake time, most", Cost.Bakes.MostMs(), "ms");
    Published.Places("structure worker collection time, most", Cost.BakeResume.MostMs(), "ms");
    Published.Places("structure landing selection time, most", Cost.BakeLanding.MostMs(), "ms");
    Published.Places("structure transfer time, most", Cost.BakeTransfer.MostMs(), "ms");
    Published.Places("structure live transfer time, most", Cost.BakeLiveTransfer.MostMs(), "ms");
    Published.Places(
        "structure candidate transfer time, most", Cost.BakeCandidateTransfer.MostMs(), "ms");
    Published.Places("structure landing commit time, most", Cost.BakeCommit.MostMs(), "ms");
    Published.Places("structure posting time, most", Cost.BakePosting.MostMs(), "ms");
    Published.Places("structure candidate selection time, most",
                     World.StructureBuilds.SlowestCandidateSelectionMs(),
                     "ms");
    Published.Places("structure height resolution time, most",
                     World.StructureBuilds.SlowestHeightResolutionMs(),
                     "ms");
    Published.Places("structure raw extraction time, most",
                     World.StructureBuilds.SlowestRawExtractionMs(),
                     "ms");
    Published.Places(
        "structure task posting time, most", World.StructureBuilds.SlowestTaskPostingMs(), "ms");
    Published.Places("world growth time, most", Cost.Growth.MostMs(), "ms");
    Published.Places("simulation core time, most", Cost.Simulation.MostMs(), "ms");
    Published.Places("ground candidate time, most", Cost.Ground.MostMs(), "ms");
    Published.Places("ground request time, most", Cost.GroundRequest.MostMs(), "ms");
    Published.Places("ground candidate begin time, most", Cost.GroundBuildBegin.MostMs(), "ms");
    Published.Places("ground candidate creation time, most", Cost.GroundBuildCreate.MostMs(), "ms");
    Published.Places(
        "ground candidate preparation time, most", Cost.GroundBuildPrepare.MostMs(), "ms");
    Published.Places("ground retirement time, most", Cost.GroundRetirement.MostMs(), "ms");
    Published.Places("ground retirement time, last", Cost.GroundRetirement.LastMs(), "ms");
    Published.Places(
        "ground retirement slices", static_cast<double>(Cost.GroundRetirement.Taken()), "slices");
    static constexpr std::array<std::string_view, Spent::kGroundPhaseCount> kGroundPhases{
        "candidate",
        "patchwork",
        "sheet fields",
        "sheet refinement",
        "sheet halos",
        "sheet mesh",
        "classes",
        "surface",
        "models",
        "network",
        "road alignments",
        "corridors",
        "structure bake",
        "earthworks",
        "terrain mesh",
        "water",
        "geometry",
        "publication"};
    for (size_t phase = 0; phase < Cost.GroundPhases.size(); ++phase) {
      if (Cost.GroundPhases[phase].Taken() == 0) { continue; }
      Published.Places(std::string("ground phase ") + std::string(kGroundPhases[phase]) +
                           " time, most",
                       Cost.GroundPhases[phase].MostMs(),
                       "ms");
      Published.Places(std::string("ground phase ") + std::string(kGroundPhases[phase]) +
                           " advances",
                       static_cast<double>(Cost.GroundPhases[phase].Taken()),
                       "frames");
    }
    Published.Places("vegetation update time, most", Cost.Crowns.MostMs(), "ms");
  }
  if (Picture.Standing) {
    for (size_t at = 0; at < Render::kStageCount; ++at) {
      const auto stage = static_cast<Render::Stage>(at);
      const Render::SceneRenderer::Effort &spent = Picture.Device.Spent(stage);
      if (spent.TookMs <= 0.0 && spent.Draws == 0) { continue; }
      Published.Places(std::string(Row(stage).Name) + ", took", spent.TookMs, "ms");
      Published.Places(
          std::string(Row(stage).Name) + ", drew", static_cast<double>(spent.Draws), "draws");
      Published.Places(std::string(Row(stage).Name) + ", triangles",
                       static_cast<double>(spent.Triangles),
                       "triangles");
      Published.Places(std::string(Row(stage).Name) + ", surfaces",
                       static_cast<double>(spent.Surfaces),
                       "slots");
      Published.Places(std::string(Row(stage).Name) + ", placements",
                       static_cast<double>(spent.Placements),
                       "slots");
      Published.Places(std::string(Row(stage).Name) + ", textured",
                       static_cast<double>(spent.Textured),
                       "slots");
      Published.Places(std::string(Row(stage).Name) + ", colour images",
                       static_cast<double>(spent.Palettes),
                       "images");
      Published.Places(std::string(Row(stage).Name) + ", device bytes",
                       static_cast<double>(spent.DeviceBytes),
                       "bytes");
      Published.Places(std::string(Row(stage).Name) + ", placements that differ",
                       static_cast<double>(spent.Distinct),
                       "rows");
      Published.Places(std::string(Row(stage).Name) + ", vertex layouts",
                       static_cast<double>(spent.Layouts),
                       "layouts");
    }
  }
  if (Cost.Render.Taken() > 0) {
    Published.Places("the picture's own time, last", Cost.Render.LastMs(), "ms");
    Published.Places("the picture's own time, least", Cost.Render.LeastMs(), "ms");
    Published.Places("the picture's own time, most", Cost.Render.MostMs(), "ms");
    Published.Places("pictures drawn", static_cast<double>(Cost.Render.Taken()), "pictures");
  }
  {
    const std::vector<std::string> clashed = Published.Clashed();
    Published.Places(
        "measures published twice in one round", static_cast<double>(clashed.size()), "rows");
    for (const std::string &one : clashed) {
      Published.Places("published twice in one round: " + one, 1.0, "rows");
    }
  }
}

}
