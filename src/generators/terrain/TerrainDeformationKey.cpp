#include "PreparedTerrainDeformation.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include <cmath>
#include <utility>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace outshine::Generators {
namespace {
constexpr uint32_t kDeformationFormat = 0x31445450;

bool Number(ByteWriter &out, double value) {
  return std::isfinite(value) && out.Number(value == 0.0 ? 0.0 : value);
}

bool Numbers(ByteWriter &out, std::span<const double> values) {
  if (!out.Number(static_cast<uint64_t>(values.size()))) { return false; }
  for (const double value : values) {
    if (!Number(out, value)) { return false; }
  }
  return true;
}

bool Profile(ByteWriter &out, const ProfiledCorridorSpan &span) {
  return out.Number(span.CorridorKey) && Numbers(out,
                                                 std::array{span.BeginM.EastM,
                                                            span.BeginM.NorthM,
                                                            span.EndM.EastM,
                                                            span.EndM.NorthM,
                                                            span.BeginDerivativeM.EastM,
                                                            span.BeginDerivativeM.NorthM,
                                                            span.BeginDerivativeM.UpM,
                                                            span.EndDerivativeM.EastM,
                                                            span.EndDerivativeM.NorthM,
                                                            span.EndDerivativeM.UpM,
                                                            span.StationLengthM,
                                                            span.BeginBedM,
                                                            span.EndBedM,
                                                            span.BeginPavementHalfWidthM,
                                                            span.EndPavementHalfWidthM,
                                                            span.BeginHalfWidthM,
                                                            span.EndHalfWidthM});
}

bool Stamp(ByteWriter &out, const EarthworkStamp &stamp) {
  if (!Numbers(out, stamp.RingEastNorthM) ||
      !out.Number(static_cast<uint64_t>(stamp.HoleRingsEastNorthM.size()))) {
    return false;
  }
  for (const auto &hole : stamp.HoleRingsEastNorthM) {
    if (!Numbers(out, hole)) { return false; }
  }
  return Numbers(out,
                 std::array{stamp.LowE,
                            stamp.HighE,
                            stamp.LowN,
                            stamp.HighN,
                            stamp.AtE,
                            stamp.AtN,
                            stamp.PlateauM,
                            stamp.SlopeE,
                            stamp.SlopeN,
                            stamp.ApronM,
                            stamp.YieldM,
                            stamp.SagInv}) &&
         Numbers(out, stamp.SeamEastNorthM) &&
         out.Number(static_cast<uint8_t>(stamp.Profile.has_value())) &&
         (!stamp.Profile || Profile(out, *stamp.Profile)) &&
         out.Number(static_cast<uint8_t>(stamp.Fills)) &&
         out.Number(static_cast<uint8_t>(stamp.Kind));
}
}

std::string TerrainDeformationKey(const Patchwork &input,
                                  std::span<const EarthworkStamp> stamps,
                                  const TangentFrame &frame,
                                  TerrainPageLayout layout,
                                  double mostEarthworkM) {
  if (!layout.Valid() || !std::isfinite(mostEarthworkM) || mostEarthworkM < 0) { return {}; }
  const auto encoded = EncodeTerrainDeformation(std::string(64, '0'), input.Sheets, {});
  if (!encoded) { return {}; }
  ByteWriter out(kTerrainDeformationBytesMost);
  if (!out.Number(kDeformationFormat) || !out.Put(Sha256Digest(encoded->data(), encoded->size())) ||
      !out.Number(layout.Side) || !out.Number(layout.Halo) || !Number(out, mostEarthworkM) ||
      !Numbers(out, std::span(frame.OriginEcef().data(), size_t{3})) ||
      !Numbers(out, std::span(frame.EastEcef().data(), size_t{3})) ||
      !Numbers(out, std::span(frame.NorthEcef().data(), size_t{3})) ||
      !Numbers(out, std::span(frame.UpEcef().data(), size_t{3})) ||
      !out.Number(static_cast<uint64_t>(stamps.size()))) {
    return {};
  }
  for (const EarthworkStamp &stamp : stamps) {
    if (!Stamp(out, stamp)) { return {}; }
  }
  return Sha256Hex(out.Bytes().data(), out.Bytes().size());
}
}
