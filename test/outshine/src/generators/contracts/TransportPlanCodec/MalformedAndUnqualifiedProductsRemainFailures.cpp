#include "TransportPlanFixture.h"
#include "Check.h"
#include "BinaryValueArchive.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {
void BodyFailures(const outshine::Generators::TransportPlan &source,
                  std::span<const uint8_t> bytes) {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  CHECK(!source.Bodies.empty(), "the body layout control contains a solid");
  if (source.Bodies.empty()) { return; }
  auto broken = source;
  auto rejects = [&](const char *reason) {
    const auto result = EncodeTransportPlan(broken, kTransportFixtureLimits);
    CHECK(!result && result.error() == TransportPlanCodecError::InvalidBody, reason);
    broken = source;
  };
  broken.Bodies.front().Faces.resize(broken.Bodies.front().SurfaceFaces);
  broken.Bodies.front().WallParents.clear();
  broken.Bodies.front().WallKinds.clear();
  rejects("a body without bottom and wall faces cannot be stored");
  broken.Bodies.front().WallParents.clear();
  broken.Bodies.front().WallKinds.clear();
  rejects("a body with missing wall metadata cannot be stored");
  broken.Bodies.front().WallParents.push_back(0);
  broken.Bodies.front().WallKinds.push_back(HeightSolidWallKind::Boundary);
  rejects("a body with excessive wall metadata cannot be stored");
  broken.Bodies.front().SurfaceFaces = std::numeric_limits<size_t>::max();
  rejects("a body surface count cannot overflow cap/wall layout checks");
  broken.Bodies.front().VolumeM3 = 0;
  rejects("a nonempty body with zero volume cannot be stored");
  broken.Bodies.front().VolumeM3 = -1;
  rejects("a nonempty body with negative volume cannot be stored");
  BinaryValueWriter marker(16);
  CHECK(marker(static_cast<uint64_t>(source.Bodies.front().SurfaceFaces),
               source.Bodies.front().VolumeM3),
        "the body scalar marker encodes");
  const auto found = std::ranges::search(bytes, marker.Out.Bytes());
  CHECK(!found.empty(), "the valid body scalar marker exists in the package");
  if (found.empty()) { return; }
  CHECK(std::ranges::search(bytes.subspan(static_cast<size_t>(found.end() - bytes.begin())),
                            marker.Out.Bytes())
            .empty(),
        "the body scalar marker is unambiguous");
  const size_t offset = static_cast<size_t>(found.begin() - bytes.begin());
  for (const auto [surfaces, volume] :
       {std::pair{static_cast<uint64_t>(source.Bodies.front().SurfaceFaces), 0.0},
        std::pair{static_cast<uint64_t>(source.Bodies.front().SurfaceFaces + 1),
                  source.Bodies.front().VolumeM3},
        std::pair{std::numeric_limits<uint64_t>::max(), source.Bodies.front().VolumeM3}}) {
    BinaryValueWriter replacement(16);
    CHECK(replacement(surfaces, volume), "the malformed body scalar replacement encodes");
    std::vector<uint8_t> corrupt(bytes.begin(), bytes.end());
    std::ranges::copy(replacement.Out.Bytes(),
                      corrupt.begin() + static_cast<std::ptrdiff_t>(offset));
    const auto decoded = DecodeTransportPlan(corrupt, kTransportFixtureLimits);
    CHECK(!decoded && decoded.error() == TransportPlanCodecError::InvalidBody,
          "stored malformed body layouts and nonpositive volumes fail during "
          "decoding");
  }
}
}

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
  BodyFailures(*source, *bytes);
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
  broken.Paths[1].FirstStation = 0;
  CHECK(!EncodeTransportPlan(broken, kTransportFixtureLimits),
        "overlapping ranges cannot multiply profile-validation work over the same stations");
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
