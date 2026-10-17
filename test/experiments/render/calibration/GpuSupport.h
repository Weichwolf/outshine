#ifndef OUTSHINE_DIAGNOSTICS_CALIBRATION_GPUSUPPORT_H
#define OUTSHINE_DIAGNOSTICS_CALIBRATION_GPUSUPPORT_H
#include "Calibration.h"
#include <chrono>
#include <cmath>
#include <expected>
#include <ratio>
#include <utility>

namespace outshine::Diagnostics::Calibration {
[[nodiscard]] Native::Result<Workload> Raster(Native::Device &device, Shaders shaders);
inline constexpr auto kGpuWait = std::chrono::seconds(5);
[[nodiscard]] Native::Result<void> Finish(Native::Device &device,
                                          std::unique_ptr<Native::Commands> commands);
[[nodiscard]] Native::Result<bool> VerifyBuffer(Native::Device &device,
                                                const Native::Buffer &source,
                                                Native::Buffer &readback,
                                                std::span<const std::byte> expected);
[[nodiscard]] Native::Result<void> Upload(Native::Device &device,
                                          Native::Commands &commands,
                                          Native::Buffer &destination,
                                          std::span<const std::byte> bytes);

template <typename Encode> Native::Result<Sample> Measure(Native::Device &device, Encode encode) {
  using Clock = std::chrono::steady_clock;
  const auto begin = Clock::now();
  auto commands = device.Begin();
  if (!commands) { return std::unexpected(commands.error()); }
  const auto encoded = encode(**commands);
  if (!encoded) { return std::unexpected(encoded.error()); }
  const auto afterEncode = Clock::now();
  auto submitted = device.Submit(std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  const auto afterSubmit = Clock::now();
  const auto waited = device.Wait(*submitted, kGpuWait);
  const auto afterWait = Clock::now();
  if (!waited) { return std::unexpected(waited.error()); }
  if (*waited != Native::CompletionStatus::Ready) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Busy,
                                         .Message = "calibration GPU wait exceeded five seconds"});
  }
  const auto ns = [](auto a, auto b) {
    return std::chrono::duration<double, std::nano>(b - a).count();
  };
  Sample result{};
  result.WallNs = ns(begin, afterWait);
  result.EncodeNs = ns(begin, afterEncode);
  result.SubmitNs = ns(afterEncode, afterSubmit);
  result.WaitNs = ns(afterSubmit, afterWait);
  const auto timing = device.SubmissionGpuTime(*submitted);
  if (timing && timing->Status == Native::CompletionStatus::Ready &&
      std::isfinite(timing->ElapsedNanoseconds) && timing->ElapsedNanoseconds > 0) {
    result.GpuNs = timing->ElapsedNanoseconds;
  } else if (!timing && timing.error().Kind != Native::ErrorKind::Unsupported) {
    return std::unexpected(timing.error());
  }
  return result;
}
}
#endif
