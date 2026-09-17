#ifndef OUTSHINE_ENGINE_GROUNDCANDIDATEREVISION_H
#define OUTSHINE_ENGINE_GROUNDCANDIDATEREVISION_H

#include "GroundPublication.h"
#include <cstdint>

namespace outshine {
class GroundCandidateRevision {
public:
  enum class Stage : uint8_t { NeedsClasses, NeedsGroundSurface, NeedsModels, NeedsGeometry };

  explicit GroundCandidateRevision(GroundRevision revision) noexcept : Revision_(revision) {}

  [[nodiscard]] bool Accepts(const GroundRevision &revision) noexcept {
    if (Revision_.Region != revision.Region || Revision_.Projection != revision.Projection) {
      return false;
    }
    if (Revision_.Classes != revision.Classes && Stage_ != Stage::NeedsClasses) { return false; }
    if (Revision_.Footprints != revision.Footprints && Stage_ == Stage::NeedsGeometry) {
      return false;
    }
    GroundRevision admitted = revision;
    admitted.ResidentTiles = Revision_.ResidentTiles;
    Revision_ = admitted;
    return true;
  }

  [[nodiscard]] const GroundRevision &Current() const noexcept { return Revision_; }

  [[nodiscard]] Stage NextStage() const noexcept { return Stage_; }

  void AdvancesTo(Stage stage) noexcept { Stage_ = stage; }

private:
  GroundRevision Revision_;
  Stage Stage_ = Stage::NeedsClasses;
};
}
#endif
