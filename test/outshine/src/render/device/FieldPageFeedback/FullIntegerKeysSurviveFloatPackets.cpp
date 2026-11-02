#include "FieldPageFeedback.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace {
using namespace outshine::Render;
using namespace outshine::Test;

void RoundTrips() {
  constexpr std::array<int32_t, 7> values{0,
                                          -1,
                                          1,
                                          16777217,
                                          -16777217,
                                          std::numeric_limits<int32_t>::min(),
                                          std::numeric_limits<int32_t>::max()};
  for (const auto x : values) {
    for (const auto y : values) {
      for (const auto mip : {0u, 5u, 31u}) {
        const FieldPageAddress key{.X = x, .Y = y, .Mip = mip};
        const auto encoded = EncodeFieldPageFeedback(key);
        CHECK(encoded, "every supported integer address has an exact finite FP32 packet");
        if (!encoded) { continue; }
        const auto decoded = DecodeFieldPageFeedback(*encoded);
        CHECK(decoded && *decoded == key,
              "low and high coordinate bits survive without lossy int32-to-float conversion");
      }
    }
  }
  const auto empty = DecodeFieldPageFeedback(FieldPageFeedbackPixel{});
  CHECK(empty && !*empty, "the clear colour requests no asset, including virtual tile zero");
  CHECK(!EncodeFieldPageFeedback({.Mip = 32}), "out-of-range mips cannot alias a valid packet");
}

void InvalidPackets() {
  const auto packet = EncodeFieldPageFeedback({.X = -1, .Y = 16777217, .Mip = 5});
  CHECK(packet, "a negative-coordinate counterexample encodes");
  if (!packet) { return; }
  for (const auto bad : {std::numeric_limits<float>::quiet_NaN(),
                         std::numeric_limits<float>::infinity(),
                         -1.0f,
                         0.5f,
                         16777216.0f}) {
    auto changed = *packet;
    changed.Words[0] = bad;
    CHECK(!DecodeFieldPageFeedback(changed), "nonfinite/fractional/out-of-range words are invalid");
  }
  auto changed = *packet;
  changed.Words[2] = 0;
  CHECK(!DecodeFieldPageFeedback(changed), "an absent valid bit is not a request");
  changed = *packet;
  changed.Words[3] = 1;
  CHECK(!DecodeFieldPageFeedback(changed), "unknown payload words cannot silently extend the ABI");
  const std::array packets{FieldPageFeedbackPixel{}, *packet, FieldPageFeedbackPixel{}};
  std::array<FieldPageAddress, 3> output;
  const auto bytes = std::as_bytes(std::span(packets));
  const auto decoded = DecodeFieldPageFeedback(bytes, output);
  constexpr FieldPageAddress expected{.X = -1, .Y = 16777217, .Mip = 5};
  CHECK(decoded && *decoded == 1 && output[0] == expected,
        "batch decoding retains only initialized requests");
  CHECK(!DecodeFieldPageFeedback(bytes.first(bytes.size() - 1), output) &&
            !DecodeFieldPageFeedback(bytes, std::span(output).first(2)),
        "truncated packets and insufficient workspace reject before decoding");
}
}

int main() {
  RoundTrips();
  InvalidPackets();
  return outshine::Test::Report();
}
