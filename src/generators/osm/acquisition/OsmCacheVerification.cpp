#include "OsmSourceAcquisitionState.h"
#include "OsmApiRegion.h"

#include <expected>
#include <memory>
#include <stop_token>
#include <utility>
#include <variant>

namespace outshine::Generators::Osm {

void SourceAcquisition::VerifyCache() {
  CancelCellPipeline();
  Phase_ = Phase::Verifying;
  const auto output = std::make_shared<Result>();
  const std::stop_source stop;
  const auto handle = Io_.Post([access = Access_,
                                output,
                                stop = stop.get_token(),
                                registry = Access_->Registry,
                                roots = Cells_->Roots,
                                provider = Requested_.front(),
                                root = Root_] {
    for (const auto cell : roots) {
      if (stop.stop_requested()) {
        output->Value = CacheProofResult(std::unexpected("OSM cache verification canceled"));
        return;
      }
      auto region =
          ApiRegion::Create(provider, *access->Store, *access->Wire, registry, root, cell);
      if (!region || !(*region)->HasCachedCoverage(*access->Store)) {
        output->Value = CacheProofResult(std::unexpected(
            region ? "OSM source cache does not retain complete verified cell coverage"
                   : region.error()));
        return;
      }
    }
    output->Value = CacheProofResult(std::monostate{});
  });
  if (handle == Tasks::kNoTask) {
    Error_ = "OSM cache verification worker queue closed";
    Phase_ = Phase::Failed;
    Cells_->Preparing.clear();
    return;
  }
  Pending_ = Pending{
      .Handle = handle, .Owner = &Io_, .Revision = Revision_, .Output = output, .Stop = stop};
}

}
