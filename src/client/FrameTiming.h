#ifndef OUTSHINE_CLIENT_FRAMETIMING_H
#define OUTSHINE_CLIENT_FRAMETIMING_H

#include <render/RenderConfiguration.h>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <string>

namespace outshine::Client {

inline constexpr std::uint64_t kNanosecondsPerSecond = 1'000'000'000;

struct FrameTiming {
  double RateHz = Render::kDefaultFrameRateHz;

  [[nodiscard]] static std::expected<FrameTiming, std::string> At(double rateHz) {
    if (!std::isfinite(rateHz) || rateHz <= 0.0 || rateHz > kNanosecondsPerSecond ||
        kNanosecondsPerSecond / rateHz >=
            static_cast<double>(std::numeric_limits<std::uint64_t>::max())) {
      return std::unexpected(
          "capture frame rate must have a finite positive, representable nanosecond period");
    }
    return FrameTiming{.RateHz = rateHz};
  }

  [[nodiscard]] double BudgetMs() const noexcept {
    return std::chrono::milliseconds::period::den / RateHz;
  }

  [[nodiscard]] std::size_t TurnFrames() const noexcept {
    return static_cast<std::size_t>(std::ceil(RateHz));
  }

  [[nodiscard]] std::uint64_t DeadlineNs(std::uint64_t frame) const noexcept {
    return static_cast<std::uint64_t>(
        std::ceil(static_cast<double>(frame) * kNanosecondsPerSecond / RateHz));
  }
};

}
#endif
