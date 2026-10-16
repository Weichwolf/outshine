#ifndef OUTSHINE_CLIENT_PLACEFRAMEPROFILE_H
#define OUTSHINE_CLIENT_PLACEFRAMEPROFILE_H

#include "FramePacer.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>

namespace outshine::Shots {

class FrameProfile {
public:
  FrameProfile() = default;

  [[nodiscard]] static std::expected<FrameProfile, std::string> ForPlace(double rateHz) {
    constexpr std::array<std::uint64_t, 3> rates{25, 30, Client::kFrameRateHz};
    for (const auto rate : rates) {
      if (rateHz == static_cast<double>(rate)) { return FrameProfile(rate); }
    }
    return std::unexpected("place frame rate must be 25, 30 or 60 Hz");
  }

  [[nodiscard]] std::uint64_t RateHz() const noexcept { return RateHz_; }

  [[nodiscard]] std::size_t Frames() const noexcept { return static_cast<std::size_t>(RateHz_); }

  [[nodiscard]] double BudgetMs() const noexcept {
    return kMillisecondsPerSecond / static_cast<double>(RateHz_);
  }

private:
  static constexpr double kMillisecondsPerSecond = 1000.0;

  explicit FrameProfile(std::uint64_t rateHz) noexcept : RateHz_(rateHz) {}

  std::uint64_t RateHz_ = Client::kFrameRateHz;
};

}
#endif
