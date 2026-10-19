#include "WorldRegionBuildState.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace outshine {

std::string_view Engine::State::GroundBuildStatus() const noexcept {
  return World.GroundBuild ? World.GroundBuild->Status() : "absent";
}

std::string Engine::State::GroundBuildDiagnostic() const {
  std::string diagnostic = "ground build=" + std::string(GroundBuildStatus());
  const auto failure = World.StructureBuilds.LastHeightFailure();
  const std::array<std::string_view, 3> causes{"none", "unavailable", "unqualified"};
  diagnostic += ", height failure=" + std::string(causes[static_cast<size_t>(failure.Cause)]) +
                " at " + std::to_string(failure.Tile.Zoom) + "/" + std::to_string(failure.Tile.X) +
                "/" + std::to_string(failure.Tile.Y);
  if (World.Region) {
    const auto *pinned = World.Region->Vectors();
    const auto *live = World.Stack.Vectors();
    diagnostic +=
        ", region current=" +
        std::to_string(
            static_cast<int>(World.Region->MatchesLiveSources(live, World.Stack.Footprints()))) +
        ", region vector generations=" +
        std::to_string(pinned != nullptr ? pinned->Generation() : 0) + "/" +
        std::to_string(live != nullptr ? live->Generation() : 0) +
        ", region footprint revisions=" + std::to_string(World.Region->Footprints().Revision()) +
        "/" + std::to_string(World.Stack.Footprints().Revision());
  } else {
    diagnostic += ", region absent";
  }
  if (!World.GroundBuild) { return diagnostic; }
  const auto &footprints = World.GroundBuild->Footprints();
  const auto *vectors = World.Stack.Vectors();
  diagnostic += World.GroundBuild->Revision().Quality == GroundQuality::Refined
                    ? ", candidate quality=refined"
                    : ", candidate quality=playable";
  diagnostic += ", structure accepted/taken=" + std::to_string(footprints.AcceptedTiles().size()) +
                "/" + std::to_string(footprints.IngestedTiles());
  diagnostic += ", source refinement complete=" +
                std::to_string(static_cast<int>(footprints.RefinementComplete()));
  diagnostic +=
      ", unqualified structure tiles=" +
      std::to_string(std::ranges::count_if(footprints.AcceptedInputs(),
                                           [](const auto &input) { return !input.Qualified; }));
  diagnostic += ", structure refinement=" + std::to_string(footprints.RefinementRemaining());
  if (const auto *worker = World.GroundBuild->StreetGraphWorker(); worker != nullptr) {
    diagnostic += ", network phase=" + std::string(worker->PhaseName()) +
                  ", network advances=" + std::to_string(worker->Advances());
  }
  diagnostic +=
      ", footprints ingested=" +
      std::to_string(static_cast<int>(vectors != nullptr && footprints.Ingested(*vectors)));
  diagnostic +=
      ", terrain fields=" + World.GroundBuild->Candidate().Products().Sheets.FieldDiagnostic();
  const auto *pinned = World.GroundBuild->Candidate().Sources().Vectors.get();
  diagnostic +=
      ", vector generations=" + std::to_string(pinned != nullptr ? pinned->Generation() : 0) + "/" +
      std::to_string(vectors != nullptr ? vectors->Generation() : 0);
  return diagnostic;
}

void Engine::State::PublishGroundCandidateMeasurements(const GroundWorldCandidate &candidate,
                                                       const WorldRegionBuildState &state) {
  const GroundWorldCandidate::PublicationMetrics &publication = candidate.Publication();
  Published.RecordMetric("ground publication: world owner swap", publication.WorldMs, "ms");
  Published.RecordMetric("ground publication: CPU products", publication.ProductsMs, "ms");
  Published.RecordMetric("ground publication: pieces", publication.PiecesMs, "ms");
  Published.RecordMetric("ground publication: orphan structure pieces reclaimed",
                         static_cast<double>(publication.OrphanStructurePieces),
                         "pieces");
  Published.RecordMetric("ground publication: resource binding", publication.BindingMs, "ms");
  Published.RecordMetric("ground publication: revision", publication.RevisionMs, "ms");
  Published.RecordMetric("ground publication: quality",
                         state.Revision().Quality == GroundQuality::Refined ? 1.0 : 0.0,
                         "0=playable 1=refined");
  const uint64_t geometry = World.Pieces.Digest();
  Published.RecordMetric(
      "the geometry the world built, high half", static_cast<double>(geometry >> 32U), "digest");
  Published.RecordMetric(
      "and its low half", static_cast<double>(geometry & kRegionDigestLowWordMask), "digest");
  Published.RecordMetric("ground candidate: direct CPU product peak",
                         static_cast<double>(state.ProductPeakBytes()),
                         "bytes");
  Published.RecordMetric("ground candidate: CPU products retained for retirement",
                         static_cast<double>(state.RetainedProductBytes()),
                         "bytes");
  Published.RecordMetric("ground candidate: candidate products retained for retirement",
                         static_cast<double>(state.RetainedCandidateBytes()),
                         "bytes");
  Published.RecordMetric("ground candidate: patchwork retained for retirement",
                         static_cast<double>(state.RetainedPatchworkBytes()),
                         "bytes");
  Published.RecordMetric("ground candidate: terrain survey retained for retirement",
                         static_cast<double>(state.RetainedSurveyBytes()),
                         "bytes");
}

}
