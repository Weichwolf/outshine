#include "PreparedTerrainDeformation.h"
#include "ByteArchive.h"
#include "BlockedDigest.h"
#include "Sha256.h"
#include <cmath>
#include <expected>
#include <utility>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace outshine::Generators {
namespace {
constexpr uint32_t kDeformationFormat = 0x31445450;

template <class Archive> bool Number(Archive &out, double value) {
  return std::isfinite(value) && out.Number(value == 0.0 ? 0.0 : value);
}

template <class Archive> bool Numbers(Archive &out, std::span<const double> values) {
  if (!out.Number(static_cast<uint64_t>(values.size()))) { return false; }
  for (const double value : values) {
    if (!Number(out, value)) { return false; }
  }
  return true;
}

template <class Archive> bool Profile(Archive &out, const ProfiledCorridorSpan &span) {
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

template <class Archive> bool Stamp(Archive &out, const EarthworkStamp &stamp) {
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

std::expected<std::string, std::string>
TerrainDeformationKey(const Patchwork &input,
                      std::span<const EarthworkStamp> stamps,
                      const TangentFrame &frame,
                      TerrainPageLayout layout,
                      double mostEarthworkM) {
  if (!layout.Valid() || !std::isfinite(mostEarthworkM) || mostEarthworkM < 0) {
    return std::unexpected("invalid terrain deformation layout or height limit");
  }
  const auto encoded = EncodeTerrainDeformation(std::string(64, '0'), input.Sheets, {});
  if (!encoded) {
    size_t nodes = 0;
    for (const Sheet &page : input.Sheets) { nodes += page.Nodes.size(); }
    return std::unexpected("terrain deformation pages exceed schema or byte budget: pages=" +
                           std::to_string(input.Sheets.size()) + " nodes=" + std::to_string(nodes));
  }
  ByteWriter out(4096);
  if (!out.Number(kDeformationFormat) || !out.Number(uint32_t{2}) ||
      !out.Put(Sha256Digest(encoded->data(), encoded->size())) || !out.Number(layout.Side) ||
      !out.Number(layout.Halo) || !Number(out, mostEarthworkM) ||
      !Numbers(out, std::span(frame.OriginEcef().data(), size_t{3})) ||
      !Numbers(out, std::span(frame.EastEcef().data(), size_t{3})) ||
      !Numbers(out, std::span(frame.NorthEcef().data(), size_t{3})) ||
      !Numbers(out, std::span(frame.UpEcef().data(), size_t{3})) ||
      !out.Number(static_cast<uint64_t>(stamps.size()))) {
    return std::unexpected("invalid terrain deformation physical frame");
  }
  BlockedDigest contacts;
  for (size_t index = 0; index < stamps.size(); ++index) {
    if (!Stamp(contacts, stamps[index])) {
      return std::unexpected("invalid terrain deformation contact: index=" + std::to_string(index) +
                             " encoded_bytes=" + std::to_string(contacts.Count()));
    }
  }
  if (!out.Number(contacts.Count()) || !out.Put(contacts.Finish())) {
    return std::unexpected("terrain deformation descriptor exceeds its byte budget");
  }
  return Sha256Hex(out.Bytes().data(), out.Bytes().size());
}
}
