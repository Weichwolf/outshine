#ifndef OUTSHINE_ENGINE_GROUNDPUBLICATION_H
#define OUTSHINE_ENGINE_GROUNDPUBLICATION_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace outshine {
enum class GroundQuality : uint8_t { Playable, Refined };

struct GroundCoverage {
  double ContactRadiusM = 0.0;
  double VisualRadiusM = 0.0;
  int MinimumSourceZoom = 0;
  int TargetSourceZoom = 0;

  auto operator<=>(const GroundCoverage &) const = default;
};

struct GroundRevision {
  uint64_t Region = 0;
  size_t ResidentTiles = 0;
  uint64_t Classes = 0;
  uint64_t Footprints = 0;
  size_t StreetTiles = 0;
  size_t WaterTiles = 0;
  std::array<double, 3> Projection{};
  GroundCoverage Coverage;
  GroundQuality Quality = GroundQuality::Refined;
};

class GroundPublication {
public:
  enum class Phase : uint8_t { Open, Captured };

  [[nodiscard]] bool NeedsRebuild(const GroundRevision &requested,
                                  bool includeResidency,
                                  bool missingRims) const noexcept {
    return !Current_ || Current_->Region != requested.Region ||
           Current_->Classes != requested.Classes || Current_->Footprints != requested.Footprints ||
           Current_->StreetTiles != requested.StreetTiles ||
           Current_->WaterTiles != requested.WaterTiles ||
           Current_->Projection != requested.Projection ||
           Current_->Coverage != requested.Coverage || Current_->Quality < requested.Quality ||
           (includeResidency &&
            (Current_->ResidentTiles != requested.ResidentTiles || missingRims));
  }

  [[nodiscard]] const std::optional<GroundRevision> &Current() const noexcept { return Current_; }

  [[nodiscard]] bool BeginCapture() noexcept {
    if (!Current_ || Phase_ != Phase::Open) { return false; }
    Phase_ = Phase::Captured;
    return true;
  }

  void EndCapture() noexcept { Phase_ = Phase::Open; }

  [[nodiscard]] bool CanPublish() const noexcept { return Phase_ == Phase::Open; }

  [[nodiscard]] bool Publish(const GroundRevision &revision) noexcept {
    if (!CanPublish()) { return false; }
    Current_ = revision;
    return true;
  }

  void Reset() noexcept {
    Current_.reset();
    Phase_ = Phase::Open;
  }

private:
  std::optional<GroundRevision> Current_;
  Phase Phase_ = Phase::Open;
};
}
#endif
