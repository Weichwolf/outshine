#include "TransportPlanFixture.h"
#include "Check.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto source = TransportPlanFixture();
  CHECK(source.has_value(), "the native counterexample fixture is available");
  if (!source) { return Report(); }
  const auto bytes = EncodeTransportPlan(*source, kTransportFixtureLimits);
  CHECK(bytes.has_value(), "the valid control encodes before corruption");
  if (!bytes) { return Report(); }
  bool prefixesFail = true;
  for (size_t size = 0; size < bytes->size(); ++size) {
    const auto result = DecodeTransportPlan(std::span(*bytes).first(size), kTransportFixtureLimits);
    prefixesFail = !result && prefixesFail;
  }
  CHECK(prefixesFail, "every truncated prefix fails without publishing partially decoded state");
  auto extra = *bytes;
  extra.push_back(0);
  CHECK(!DecodeTransportPlan(extra, kTransportFixtureLimits),
        "trailing bytes are not another schema");
  auto version = *bytes;
  version[0] ^= uint8_t{1};
  const auto unsupported = DecodeTransportPlan(version, kTransportFixtureLimits);
  CHECK(!unsupported && unsupported.error() == TransportPlanCodecError::UnsupportedFormat,
        "a foreign format remains distinguishable from malformed current data");
  const TransportPlanCodecLimits tooSmall{.PackageBytesMost = bytes->size() - 1,
                                          .DecodedCapacityMost =
                                              kTransportFixtureLimits.DecodedCapacityMost};
  CHECK(!EncodeTransportPlan(*source, tooSmall) && !DecodeTransportPlan(*bytes, tooSmall),
        "the package cap is enforced in both directions");
  const TransportPlanCodecLimits noCapacity{.PackageBytesMost =
                                                kTransportFixtureLimits.PackageBytesMost,
                                            .DecodedCapacityMost = sizeof(TransportPlan) - 1};
  const auto deferred = DecodeTransportPlan(*bytes, noCapacity);
  CHECK(!deferred && deferred.error() == TransportPlanCodecError::CapacityExceeded,
        "decoded object capacity is bounded independently of encoded bytes");
  const TransportPlanCodecLimits noInputCapacity{
      .PackageBytesMost = kTransportFixtureLimits.PackageBytesMost,
      .DecodedCapacityMost = sizeof(TransportPlan) + sizeof(TransportInputReference) - 1};
  const auto nested = DecodeTransportPlan(*bytes, noInputCapacity);
  CHECK(!nested && nested.error() == TransportPlanCodecError::CapacityExceeded,
        "input record allocation is refused before a nested container exceeds its admission");
  auto hugeCount = *bytes;
  constexpr std::string_view manifestDomain = "outshine/transport-inputs/1";
  const auto manifest = std::ranges::search(hugeCount, manifestDomain);
  CHECK(!manifest.empty(), "the independent schema marker identifies the input list header");
  if (!manifest.empty() &&
      hugeCount.end() - manifest.end() >= static_cast<std::ptrdiff_t>(sizeof(uint32_t))) {
    std::fill_n(manifest.end(), sizeof(uint32_t), std::numeric_limits<uint8_t>::max());
    const auto bomb = DecodeTransportPlan(hugeCount, kTransportFixtureLimits);
    CHECK(!bomb && bomb.error() == TransportPlanCodecError::Malformed,
          "an impossible list length fails before allocation despite generous capacity");
  }
  auto unknown = *source;
  unknown.Coverage = TransportCoverage::Unknown;
  const auto unqualified = EncodeTransportPlan(unknown, kTransportFixtureLimits);
  CHECK(!unqualified && unqualified.error() == TransportPlanCodecError::UnknownCoverage,
        "unknown closure cannot be persisted as an accepted plan");
  auto broken = *source;
  broken.Paths[0].Ends[1] =
      TransportBoundaryPortReference{.Port = std::numeric_limits<uint32_t>::max()};
  CHECK(!EncodeTransportPlan(broken, kTransportFixtureLimits), "dangling boundary references fail");
  broken = *source;
  broken.Paths[0].FirstStation = std::numeric_limits<uint32_t>::max();
  CHECK(!EncodeTransportPlan(broken, kTransportFixtureLimits),
        "station ranges are checked without overflow");
  broken = *source;
  broken.Inputs[0].Content[0] ^= uint8_t{1};
  const auto binding = EncodeTransportPlan(broken, kTransportFixtureLimits);
  CHECK(!binding && binding.error() == TransportPlanCodecError::InvalidInputs,
        "source contents cannot silently disagree with the stored input binding");
  auto corrupt = *bytes;
  const auto digest = std::ranges::search(corrupt, source->Inputs[0].Content);
  CHECK(!digest.empty(), "the manifest contains its explicit source digest");
  if (!digest.empty()) {
    *digest.begin() ^= uint8_t{1};
    const auto invalid = DecodeTransportPlan(corrupt, kTransportFixtureLimits);
    CHECK(!invalid && invalid.error() == TransportPlanCodecError::InvalidInputs,
          "the decoder verifies the persisted source manifest against its binding");
  }
  broken = *source;
  broken.Ports[0].Contact.Plane.SlopeE = std::numeric_limits<double>::quiet_NaN();
  CHECK(!EncodeTransportPlan(broken, kTransportFixtureLimits), "nonfinite CAD conditions fail");
  return Report();
}
