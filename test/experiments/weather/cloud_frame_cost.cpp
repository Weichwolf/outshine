#include "CloudFrame.h"

#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <print>
#include <ratio>

namespace {
constexpr size_t kFrames = 100000;
constexpr size_t kRepeats = 5;
constexpr double kRateHz = 60.0;

bool Measure(outshine::CloudProfile profile, double phaseS, const outshine::Vec3 &anchor) {
  using namespace outshine::Generators::Weather;
  const outshine::CloudLayer layer{
      .BaseM = 2572.0, .Cover = 0.65, .WindMs = {{-8.0, 0.0, 4.0}}, .Profile = profile};
  for (size_t repeat = 0; repeat < kRepeats; ++repeat) {
    uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (size_t index = 0; index < kFrames; ++index) {
      const auto frame =
          MakeCloudFrame(layer, phaseS + static_cast<double>(index) / kRateHz, anchor);
      if (!frame) { return false; }
      checksum += frame->BroadBase[0];
      checksum += std::bit_cast<uint32_t>(frame->BroadFraction[0]);
      checksum += frame->DetailBase[2];
      checksum += std::bit_cast<uint32_t>(frame->DetailFraction[2]);
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const double nanoseconds = std::chrono::duration<double, std::nano>(elapsed).count();
    std::println(
        R"({{"profile":{},"phase_s":{},"anchor_m":[{},{},{}],"repeat":{},"frames":{},"wall_ns":{},"ns_per_frame":{},"checksum":{},"payload_bytes":{}}})",
        static_cast<uint32_t>(profile),
        phaseS,
        anchor[0],
        anchor[1],
        anchor[2],
        repeat,
        kFrames,
        nanoseconds,
        nanoseconds / static_cast<double>(kFrames),
        checksum,
        sizeof(CloudFrame));
  }
  return true;
}
}

int main() {
  using namespace outshine;
  constexpr double kMonthS = 32.0 * 86400.0;
  constexpr double kLargePhaseS = 1792022400.0;
  const std::array origins{Vec3{}, Vec3{{22222.12345, -511.123456, -16845.34567}}};
  for (const auto profile : {CloudProfile::Stratus, CloudProfile::CumulusV1}) {
    for (const auto &anchor : origins) {
      for (const double phaseS : {37.0, kMonthS, kLargePhaseS}) {
        if (!Measure(profile, phaseS, anchor)) { return 1; }
      }
    }
  }
}
