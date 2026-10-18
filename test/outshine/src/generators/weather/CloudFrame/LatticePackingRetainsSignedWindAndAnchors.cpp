#include "CloudFrame.h"
#include "Check.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {
void CheckLattices() {
  using namespace outshine;
  using namespace outshine::Generators::Weather;
  using namespace outshine::Test;
  constexpr double kMonthS = 32.0 * 86400.0;
  constexpr double kCellM = 1400.0;
  constexpr double kDetailFrequency = 3.1;
  constexpr double kCover = 0.65;
  constexpr double kBaseM = 2572.0;
  constexpr double kVelocityMs = 8.0;
  constexpr double kLargePhaseS = 1792022400.0;
  constexpr double kTenYearsS = 315576000.0;
  constexpr double kTolerance = 6e-8;
  const std::array origins{
      Vec3{}, Vec3{{768.0, -512.0, 1024.0}}, Vec3{{22222.12345, -511.123456, -16845.34567}}};
  for (const auto profile : {CloudProfile::Stratus, CloudProfile::CumulusV1}) {
    CloudLayer layer{.BaseM = kBaseM, .Cover = kCover, .Profile = profile};
    for (const double speed : {kVelocityMs, -kVelocityMs}) {
      layer.WindMs = {{speed, 0.0, -speed}};
      for (const double phase : {0.0, 37.0, kMonthS, -kMonthS, kTenYearsS, kLargePhaseS}) {
        for (const auto &origin : origins) {
          const auto frame = MakeCloudFrame(layer, phase, origin);
          CHECK(frame.has_value(), "finite physical inputs produce the agreed frame");
          if (!frame) { continue; }
          CHECK(frame->Shell[0] == static_cast<float>(layer.RadiusM) &&
                    frame->Shell[1] == static_cast<float>(kBaseM) &&
                    frame->Field[0] == static_cast<float>(kCover) &&
                    frame->Field[1] == static_cast<float>(static_cast<uint32_t>(profile)),
                "field anchoring preserves physical shell, cover and selected profile");
          for (size_t axis = 0; axis < 3; ++axis) {
            const double offset =
                (static_cast<double>(frame->CoordinateOrigin[axis]) - layer.WindMs[axis] * phase) /
                kCellM;
            const auto reconstructed = [](uint32_t base, float fraction) {
              return static_cast<double>(std::bit_cast<int32_t>(base)) + fraction;
            };
            CHECK(std::abs(reconstructed(frame->BroadBase[axis], frame->BroadFraction[axis]) -
                           offset) <= kTolerance &&
                      std::abs(reconstructed(frame->DetailBase[axis], frame->DetailFraction[axis]) -
                               offset * kDetailFrequency) <= kTolerance,
                  "both signed lattices retain the independently requested displacement");
          }
          CHECK(frame->CoordinateOrigin[3] == 0.0f && frame->BroadBase[3] == 0 &&
                    frame->BroadFraction[3] == 0.0f && frame->DetailBase[3] == 0 &&
                    frame->DetailFraction[3] == 0.0f && frame->Field[2] == 0.0f &&
                    frame->Field[3] == 0.0f,
                "unused payload components have deterministic zero bits");
        }
      }
    }
  }
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators::Weather;
  using namespace outshine::Test;
  CheckLattices();
  constexpr double kBaseM = 2572.0;
  constexpr double kCover = 0.65;
  constexpr double kVelocityMs = 8.0;
  constexpr double kMonthS = 32.0 * 86400.0;
  CloudLayer layer{.BaseM = kBaseM, .Cover = kCover, .WindMs = {{kVelocityMs, 0.0, 0.0}}};
  const auto first = MakeCloudFrame(layer, kMonthS);
  for (const double rate : {30.0, 60.0}) {
    const auto next = MakeCloudFrame(layer, kMonthS + 1.0 / rate);
    CHECK(first && next &&
              (first->BroadBase != next->BroadBase || first->BroadFraction != next->BroadFraction ||
               first->DetailBase != next->DetailBase ||
               first->DetailFraction != next->DetailFraction),
          "individual30/60Hz steps survive the original month-long sticking case");
  }
  const auto invalidPhase = MakeCloudFrame(layer, std::numeric_limits<double>::infinity());
  CHECK(!invalidPhase && invalidPhase.error() == CloudFrameFailure::InvalidPhase,
        "nonfinite time cannot reach floor or integer conversion");
  layer.WindMs[0] = std::numeric_limits<double>::max();
  const auto huge = MakeCloudFrame(layer, kMonthS);
  CHECK(!huge && huge.error() == CloudFrameFailure::Unrepresentable,
        "overflowing displacement is rejected before integer conversion");
  return Report();
}
