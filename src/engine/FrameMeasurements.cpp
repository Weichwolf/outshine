#include "EngineHeld.h"
#include "Heap.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace outshine {

namespace {

constexpr std::array<const char *, static_cast<size_t>(Render::RenderFramePhase::Count)>
    kSlowestRenderPhaseNames = {"render host slowest frame: prepare",
                                "render host slowest frame: acquire",
                                "render host slowest frame: upload",
                                "render host slowest frame: swapchain",
                                "render host slowest frame: cull",
                                "render host slowest frame: encode",
                                "render host slowest frame: fence wait",
                                "render host slowest frame: submit",
                                "render host slowest frame: finish"};

void PublishRenderFrameTiming(Core::DiagnosticLedger &published,
                              const Render::SceneRenderer &device) {
  const Render::RenderFrameTiming &slowest = device.SlowestRenderFrameTiming();
  published.RecordMetric("render host slowest frame: total", slowest.TotalMs, "ms");
  for (size_t phase = 0; phase < kSlowestRenderPhaseNames.size(); ++phase) {
    published.RecordMetric(kSlowestRenderPhaseNames[phase], slowest.PhaseMs[phase], "ms");
  }
}

}

void Engine::State::PublishResourcePayloadMeasurements() {
  if (!Picture.Standing) { return; }
  Published.RecordMetric("streamed piece CPU payload capacity",
                         static_cast<double>(Picture.Device.PieceSourceBytes()),
                         "bytes");
  Published.RecordMetric("streamed piece slot capacity",
                         static_cast<double>(Picture.Device.PieceSlotBytes()),
                         "bytes");
  Published.RecordMetric("height page slot capacity",
                         static_cast<double>(Picture.Device.HeightPageSlotBytes()),
                         "bytes");
  Published.RecordMetric("height page CPU payload capacity",
                         static_cast<double>(Picture.Device.HeightPageSourceBytes()),
                         "bytes");
  const auto frame = Picture.Device.FrameGraphAllocationCounts();
  Published.RecordMetric(
      "frame graph GPU textures held", static_cast<double>(frame.Textures), "textures");
  Published.RecordMetric(
      "frame graph GPU buffers held", static_cast<double>(frame.Buffers), "buffers");
  Published.RecordMetric(
      "frame graph GPU samplers held", static_cast<double>(frame.Samplers), "samplers");
}

void Engine::State::PublishSubmittedCameraMeasurements() {
  const auto &frame = Picture.Device.LastSubmittedCamera();
  if (frame.Serial == 0) { return; }
  Published.RecordMetric(
      "render last submitted camera: serial", static_cast<double>(frame.Serial), "frame");
  constexpr std::array<const char *, 3> eyeNames{"render last submitted camera: eye east",
                                                 "render last submitted camera: eye up",
                                                 "render last submitted camera: eye south"};
  constexpr std::array<const char *, 3> forwardNames{"render last submitted camera: forward east",
                                                     "render last submitted camera: forward up",
                                                     "render last submitted camera: forward south"};
  constexpr std::array<const char *, 3> upNames{"render last submitted camera: up east",
                                                "render last submitted camera: up up",
                                                "render last submitted camera: up south"};
  for (size_t axis = 0; axis < 3; ++axis) {
    Published.RecordMetric(
        eyeNames[axis], frame.Basis.EyeM[axis] - frame.GeometryAnchorM[axis], "m");
    Published.RecordMetric(forwardNames[axis], frame.Basis.Forward[axis], "unit");
    Published.RecordMetric(upNames[axis], frame.Basis.Up[axis], "unit");
  }
}

void Engine::State::PublishGroundPreparationMeasurements() {
  Published.RecordMetric("ground candidate time, most", Cost.Ground.MostMs(), "ms");
  Published.RecordMetric("ground request time, most", Cost.GroundRequest.MostMs(), "ms");
  Published.RecordMetric("ground candidate begin time, most", Cost.GroundBuildBegin.MostMs(), "ms");
  Published.RecordMetric(
      "ground candidate creation time, most", Cost.GroundBuildCreate.MostMs(), "ms");
  Published.RecordMetric(
      "ground candidate preparation time, most", Cost.GroundBuildPrepare.MostMs(), "ms");
  Published.RecordMetric("ground retirement time, most", Cost.GroundRetirement.MostMs(), "ms");
  Published.RecordMetric("ground retirement time, last", Cost.GroundRetirement.LastMs(), "ms");
  Published.RecordMetric(
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
    Published.RecordMetric(std::string("ground phase ") + std::string(kGroundPhases[phase]) +
                               " time, most",
                           Cost.GroundPhases[phase].MostMs(),
                           "ms");
    Published.RecordMetric(std::string("ground phase ") + std::string(kGroundPhases[phase]) +
                               " advances",
                           static_cast<double>(Cost.GroundPhases[phase].Taken()),
                           "frames");
  }
  Published.RecordMetric("vegetation update time, most", Cost.Crowns.MostMs(), "ms");
}

void Engine::State::PublishAdvanceMeasurements() {
  if (Cost.Advance.Taken() > 0) {
    Published.RecordMetric("the step's own time, last", Cost.Advance.LastMs(), "ms");
    Published.RecordMetric("the step's own time, least", Cost.Advance.LeastMs(), "ms");
    Published.RecordMetric("the step's own time, most", Cost.Advance.MostMs(), "ms");
    Published.RecordMetric("steps taken", static_cast<double>(Cost.Advance.Taken()), "steps");
    Published.RecordMetric("world update time, most", Cost.Update.MostMs(), "ms");
    const Spent::UpdateComponents &worst = Cost.WorstSuccessfulUpdate;
    if (worst.TotalMs > 0.0) {
      Published.RecordMetric("worst successful update: total", worst.TotalMs, "ms");
      Published.RecordMetric(
          "worst successful update: streaming and bakes", worst.StreamingMs, "ms");
      Published.RecordMetric("worst successful update: piece handoff", worst.PieceHandoffMs, "ms");
      Published.RecordMetric("worst successful update: tile restand", worst.AdvanceAtMs, "ms");
      Published.RecordMetric("worst successful update: structure bakes", worst.BakesMs, "ms");
      Published.RecordMetric("worst successful update: world growth", worst.GrowthMs, "ms");
      Published.RecordMetric("worst successful update: streaming other",
                             worst.StreamingMs - worst.PieceHandoffMs - worst.AdvanceAtMs -
                                 worst.BakesMs - worst.GrowthMs,
                             "ms");
      Published.RecordMetric("worst successful update: simulation", worst.SimulationMs, "ms");
      Published.RecordMetric("worst successful update: ground", worst.GroundMs, "ms");
      Published.RecordMetric("worst successful update: vegetation", worst.CrownsMs, "ms");
      Published.RecordMetric("worst successful update: other",
                             worst.TotalMs - worst.StreamingMs - worst.SimulationMs -
                                 worst.GroundMs - worst.CrownsMs,
                             "ms");
    }
    Published.RecordMetric("frame publication time, most", Cost.FramePublication.MostMs(), "ms");
    Published.RecordMetric("scene advance time, most", Cost.SceneAdvance.MostMs(), "ms");
    Published.RecordMetric("streaming and bake time, most", Cost.Streaming.MostMs(), "ms");
    Published.RecordMetric("piece handoff time, most", Cost.PieceHandoff.MostMs(), "ms");
    Published.RecordMetric("tile restand time, most", Cost.AdvanceAt.MostMs(), "ms");
    const Ground::SurfacePreparationMetrics &restand = World.Stack.WorstAdvance();
    Published.RecordMetric("worst tile restand: total", restand.TotalMs, "ms");
    Published.RecordMetric("worst tile restand: classification", restand.ClassificationMs, "ms");
    Published.RecordMetric("worst tile restand: vectors", restand.VectorsMs, "ms");
    Published.RecordMetric("worst vector restand: tile fetch", restand.VectorBuild.FetchMs, "ms");
    Published.RecordMetric("worst vector restand: tile parsing", restand.VectorBuild.ParseMs, "ms");
    Published.RecordMetric(
        "worst vector restand: longest layer", restand.VectorBuild.LongestLayerMs, "ms");
    Published.RecordMetric("worst vector restand: longest layer index",
                           static_cast<double>(restand.VectorBuild.LongestLayerIndex),
                           "index");
    Published.RecordMetric(
        "worst vector restand: capacity check", restand.VectorBuild.CapacityMs, "ms");
    Published.RecordMetric(
        "worst vector restand: publication", restand.VectorBuild.PublicationMs, "ms");
    Published.RecordMetric("worst tile restand: streets", restand.StreetsMs, "ms");
    Published.RecordMetric("worst tile restand: water", restand.WaterMs, "ms");
    const ::outshine::Generators::Osm::WaterField::IngestMetrics &water =
        World.Stack.WaterBodies().WorstIngest();
    Published.RecordMetric("worst water ingest: total", water.TotalMs, "ms");
    Published.RecordMetric("worst water ingest: tile admission", water.AdmissionMs, "ms");
    Published.RecordMetric("worst water ingest: height validation", water.ValidationMs, "ms");
    Published.RecordMetric("worst water ingest: longest height query", water.LongestQueryMs, "ms");
    Published.RecordMetric("worst water ingest: validated points",
                           static_cast<double>(water.ValidationPoints),
                           "points");
    Published.RecordMetric("worst water ingest: materialization", water.MaterializationMs, "ms");
    Published.RecordMetric("worst tile restand: settlement", restand.SettlementMs, "ms");
    Published.RecordMetric("worst tile restand: other",
                           restand.TotalMs - restand.ClassificationMs - restand.VectorsMs -
                               restand.StreetsMs - restand.WaterMs - restand.SettlementMs,
                           "ms");
    Published.RecordMetric("structure bake time, most", Cost.Bakes.MostMs(), "ms");
    Published.RecordMetric(
        "structure worker collection time, most", Cost.BakeResume.MostMs(), "ms");
    Published.RecordMetric(
        "structure landing selection time, most", Cost.BakeLanding.MostMs(), "ms");
    Published.RecordMetric("structure transfer time, most", Cost.BakeTransfer.MostMs(), "ms");
    Published.RecordMetric(
        "structure live transfer time, most", Cost.BakeLiveTransfer.MostMs(), "ms");
    Published.RecordMetric(
        "structure candidate transfer time, most", Cost.BakeCandidateTransfer.MostMs(), "ms");
    Published.RecordMetric("structure landing commit time, most", Cost.BakeCommit.MostMs(), "ms");
    Published.RecordMetric("structure posting time, most", Cost.BakePosting.MostMs(), "ms");
    Published.RecordMetric("structure candidate selection time, most",
                           World.StructureBuilds.SlowestCandidateSelectionMs(),
                           "ms");
    Published.RecordMetric("structure height resolution time, most",
                           World.StructureBuilds.SlowestHeightResolutionMs(),
                           "ms");
    Published.RecordMetric("structure raw extraction time, most",
                           World.StructureBuilds.SlowestRawExtractionMs(),
                           "ms");
    Published.RecordMetric(
        "structure task posting time, most", World.StructureBuilds.SlowestTaskPostingMs(), "ms");
    Published.RecordMetric("world growth time, most", Cost.Growth.MostMs(), "ms");
    Published.RecordMetric("simulation core time, most", Cost.Simulation.MostMs(), "ms");
    PublishGroundPreparationMeasurements();
  }
}

void Engine::State::PublishGroundMemoryMeasurements() {
  Published.RecordMetric("building triangles the world meshed",
                         static_cast<double>(World.Stack.Footprints().TrianglesHanded()),
                         "triangles");
  Published.RecordMetric(
      "world: the bytes its fields hold", static_cast<double>(World.Stack.HeapBytes()), "bytes");
  Published.RecordMetric("world: published semantic region",
                         World.Region ? static_cast<double>(World.Region->HeapBytes()) : 0.0,
                         "bytes");
  Published.RecordMetric("world: of that, the land classes",
                         static_cast<double>(World.Stack.Classes().HeapBytes()),
                         "bytes");
  Published.RecordMetric(
      "world: the buildings", static_cast<double>(World.Stack.Footprints().HeapBytes()), "bytes");
  Published.RecordMetric("world: of those, the footprints it keeps",
                         static_cast<double>(World.Stack.Footprints().PrintBytes()),
                         "bytes");
  Published.RecordMetric("world: tiles baking on the workers right now",
                         static_cast<double>(World.StructureBuilds.Queued()),
                         "tiles");
  Published.RecordMetric("world: the building pieces the device holds",
                         Picture.Standing ? static_cast<double>(Picture.Device.PieceBytesHeld())
                                          : 0.0,
                         "bytes");
  if (Picture.Standing) {
    const auto memory = Picture.Device.PieceAllocations();
    constexpr std::array names{"world: subject position buffer",
                               "world: subject emitted buffer",
                               "world: subject normal buffer",
                               "world: subject tangent buffer",
                               "world: subject uv buffer",
                               "world: subject uv1 buffer",
                               "world: subject colour buffer",
                               "world: subject previous position buffer",
                               "world: subject placement buffer",
                               "world: subject index buffer",
                               "world: subject cluster sphere buffer",
                               "world: subject cluster job buffer",
                               "world: subject cluster batch buffer",
                               "world: subject cluster kept buffer",
                               "world: subject cluster slot buffer",
                               "world: subject draw index buffer",
                               "world: subject draw argument buffer"};
    static_assert(names.size() == std::tuple_size_v<decltype(memory.StreamBytes)>);
    for (size_t stream = 0; stream < names.size(); ++stream) {
      Published.RecordMetric(
          names[stream], static_cast<double>(memory.StreamBytes[stream]), "bytes");
    }
    Published.RecordMetric("world: subject transfer buffer capacity",
                           static_cast<double>(memory.TransferBytes),
                           "bytes");
    Published.RecordMetric(
        "world: subject vertex arena extent", static_cast<double>(memory.VertexSlots), "slots");
    Published.RecordMetric(
        "world: subject free vertex slots", static_cast<double>(memory.FreeVertexSlots), "slots");
    Published.RecordMetric(
        "world: subject index arena extent", static_cast<double>(memory.IndexSlots), "slots");
    Published.RecordMetric(
        "world: subject free index slots", static_cast<double>(memory.FreeIndexSlots), "slots");
  }
  Published.RecordMetric(
      "world: the water", static_cast<double>(World.Stack.WaterBodies().HeapBytes()), "bytes");
  Published.RecordMetric(
      "world: the streets", static_cast<double>(World.Stack.Ways().HeapBytes()), "bytes");
  Published.RecordMetric("world: the ceiling its fields stand under",
                         static_cast<double>(Ground::SurfacePreparation::kHoldsBytes),
                         "bytes");
  Published.RecordMetric("world: times a round stopped at that ceiling",
                         static_cast<double>(World.Stack.OverCeiling()),
                         "rounds");
  Published.RecordMetric("world: and the OSM features",
                         World.Stack.Vectors() != nullptr
                             ? static_cast<double>(World.Stack.Vectors()->HeapBytes())
                             : 0.0,
                         "bytes");
}

void Engine::State::PublishFrameMeasurements() {
  static const Heap::Tag kFrameMeasurementsTag("frame-measurements");
  const Heap::Tagged measuring(kFrameMeasurementsTag);
  if (Heap::ProcessInstrumentationEnabled()) {
    PublishResourcePayloadMeasurements();
    Published.RecordMetric(
        "process C++ heap live bytes", static_cast<double>(Heap::LiveBytes()), "bytes");
    for (size_t at = 0; at < Heap::TagCount(); ++at) {
      const char *const tag = Heap::TagAt(at);
      if (tag == nullptr || Heap::TakenAt(at) == 0) { continue; }
      Published.RecordMetric(std::string("process C++ bytes allocated under ") + tag,
                             static_cast<double>(Heap::TakenAt(at)),
                             "bytes");
    }
  }
  PublishAdvanceMeasurements();
  if (Picture.Standing) {
    for (size_t at = 0; at < Render::kStageCount; ++at) {
      const auto stage = static_cast<Render::Stage>(at);
      const Render::SceneRenderer::Effort &spent = Picture.Device.Spent(stage);
      if (spent.TookMs <= 0.0 && spent.Draws == 0) { continue; }
      Published.RecordMetric(std::string(Row(stage).Name) + ", took", spent.TookMs, "ms");
      Published.RecordMetric(
          std::string(Row(stage).Name) + ", drew", static_cast<double>(spent.Draws), "draws");
      Published.RecordMetric(std::string(Row(stage).Name) + ", triangles",
                             static_cast<double>(spent.Triangles),
                             "triangles");
      Published.RecordMetric(std::string(Row(stage).Name) + ", surfaces",
                             static_cast<double>(spent.Surfaces),
                             "slots");
      Published.RecordMetric(std::string(Row(stage).Name) + ", placements",
                             static_cast<double>(spent.Placements),
                             "slots");
      Published.RecordMetric(std::string(Row(stage).Name) + ", textured",
                             static_cast<double>(spent.Textured),
                             "slots");
      Published.RecordMetric(std::string(Row(stage).Name) + ", colour images",
                             static_cast<double>(spent.Palettes),
                             "images");
      Published.RecordMetric(std::string(Row(stage).Name) + ", device bytes",
                             static_cast<double>(spent.DeviceBytes),
                             "bytes");
      Published.RecordMetric(std::string(Row(stage).Name) + ", placements that differ",
                             static_cast<double>(spent.Distinct),
                             "rows");
      Published.RecordMetric(std::string(Row(stage).Name) + ", vertex layouts",
                             static_cast<double>(spent.Layouts),
                             "layouts");
    }
  }
  if (Cost.Render.Taken() > 0) {
    Published.RecordMetric("the picture's own time, last", Cost.Render.LastMs(), "ms");
    Published.RecordMetric("the picture's own time, least", Cost.Render.LeastMs(), "ms");
    Published.RecordMetric("the picture's own time, most", Cost.Render.MostMs(), "ms");
    Published.RecordMetric("pictures drawn", static_cast<double>(Cost.Render.Taken()), "pictures");
    PublishRenderFrameTiming(Published, Picture.Device);
  }
  {
    const std::vector<std::string> clashed = Published.ConflictingMetricNames();
    Published.RecordMetric(
        "measures published twice in one round", static_cast<double>(clashed.size()), "rows");
    for (const std::string &one : clashed) {
      Published.RecordMetric("published twice in one round: " + one, 1.0, "rows");
    }
  }
}

}
