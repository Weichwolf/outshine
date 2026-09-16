#ifndef OUTSHINE_ENGINE_GROUNDPUBLICATION_H
#define OUTSHINE_ENGINE_GROUNDPUBLICATION_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace outshine {
struct GroundRevision {
  uint64_t Region = 0;
  size_t ResidentTiles = 0;
  uint64_t Classes = 0;
  uint64_t Footprints = 0;
  std::array<double, 3> Projection{};
};

class GroundPublication {
public:
  [[nodiscard]] bool NeedsRebuild(const GroundRevision &requested,
                                  bool includeResidency,
                                  bool missingRims) const noexcept {
    return !Current_ || Current_->Region != requested.Region ||
           Current_->Classes != requested.Classes || Current_->Footprints != requested.Footprints ||
           Current_->Projection != requested.Projection ||
           (includeResidency &&
            (Current_->ResidentTiles != requested.ResidentTiles || missingRims));
  }

  [[nodiscard]] const std::optional<GroundRevision> &Current() const noexcept { return Current_; }

  void Publish(const GroundRevision &revision) noexcept { Current_ = revision; }

  void Reset() noexcept { Current_.reset(); }

private:
  std::optional<GroundRevision> Current_;
};
}
#endif
