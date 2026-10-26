#include "MvtTransportSnapshotInternal.h"
#include "BinaryValueArchive.h"
#include "OsmField.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>

namespace outshine::Generators::Osm::MvtTransport {
namespace {
auto SourceKey(const World::TransportSource &source) {
  return std::tie(source.Identity.DatasetId, source.Identity.Revision, source.Item, source.Content);
}

bool ReadDigest(std::string_view text, Signature &out) {
  if (text.size() != out.size() * 2) { return false; }
  for (size_t at = 0; at < out.size(); ++at) {
    unsigned value = 0;
    const auto *first = text.data() + at * 2;
    const auto parsed = std::from_chars(first, first + 2, value, 16);
    if (parsed.ec != std::errc{} || parsed.ptr != first + 2) { return false; }
    out[at] = static_cast<uint8_t>(value);
  }
  return true;
}

std::expected<World::TransportSource, std::string> TileSource(const OsmField::Tile &tile,
                                                              MvtSchema schema) {
  World::TransportSource source;
  source.Identity = {.DatasetId = tile.Source.SourceId.empty() ? "outshine/direct-mvt"
                                                               : tile.Source.SourceId,
                     .Revision = tile.Source.Revision};
  source.Item = std::format("mvt/{}/{}/{}/{}/{}/{}",
                            static_cast<unsigned>(schema),
                            static_cast<unsigned>(tile.Source.From),
                            static_cast<unsigned>(tile.Source.Kind),
                            tile.Z,
                            tile.X,
                            tile.Y);
  if (tile.Source.NativeCell) {
    source.Item += std::format(
        "/cell/{}/{}", tile.Source.NativeCell->SouthDeg, tile.Source.NativeCell->WestDeg);
  }
  if (!ReadDigest(tile.InputDigest, source.Content)) {
    return std::unexpected("MVT transport source lacks its actual input digest");
  }
  return source;
}
}

std::expected<Sources, std::string> SourceRows(const OsmField &field) {
  Sources out;
  out.Provenance.IdentitySpace = World::TransportIdentitySpace::DerivedFragments;
  if (field.Tiles().size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected("MVT transport has too many source tiles");
  }
  for (const auto &tile : field.Tiles()) {
    auto source = TileSource(tile, field.Schema());
    if (!source) { return std::unexpected(source.error()); }
    out.Provenance.Sources.push_back(std::move(*source));
  }
  std::ranges::sort(out.Provenance.Sources, {}, SourceKey);
  const auto duplicate = std::ranges::unique(out.Provenance.Sources, {}, SourceKey);
  out.Provenance.Sources.erase(duplicate.begin(), duplicate.end());
  out.TileRows.reserve(field.Tiles().size());
  for (const auto &tile : field.Tiles()) {
    const auto source = TileSource(tile, field.Schema());
    if (!source) { return std::unexpected(source.error()); }
    const auto found =
        std::ranges::lower_bound(out.Provenance.Sources, SourceKey(*source), {}, SourceKey);
    out.TileRows.push_back(static_cast<uint32_t>(found - out.Provenance.Sources.begin()));
  }
  return out;
}

Signature SourceSignature(const World::TransportSource &source) {
  BinaryValueWriter encoded(std::numeric_limits<size_t>::max());
  const bool valid = encoded.Text("outshine/mvt/transport/1") &&
                     encoded.Text(source.Identity.DatasetId) &&
                     encoded.Text(source.Identity.Revision) && encoded.Text(source.Item) &&
                     encoded.Value(source.Content);
  assert(valid);
  const auto bytes = encoded.Out.Bytes();
  return Sha256Digest(bytes.data(), bytes.size());
}

Signature WaySignature(const Signature &source,
                       const Signature &feature,
                       std::string_view layer,
                       uint32_t ring) {
  constexpr size_t kFieldsBytes = 100;
  std::array<uint8_t, kFieldsBytes> fields{};
  const Signature layerKey = Sha256Digest(layer.data(), layer.size());
  const uint32_t ordinal = LittleEndian(ring);
  auto *at = fields.data();
  for (const auto &key : {source, feature, layerKey}) {
    std::memcpy(at, key.data(), key.size());
    at += key.size();
  }
  std::memcpy(at, &ordinal, sizeof(ordinal));
  return Sha256Digest(fields.data(), fields.size());
}

Signature SegmentSignature(const Signature &way, uint32_t step) {
  const auto encoded = LittleEndian(step);
  const Sha256Prefix prefix(way.data(), way.size());
  return prefix.Digest(&encoded, sizeof(encoded));
}

uint64_t Identifier(const Signature &signature) noexcept {
  uint64_t id = 0;
  std::memcpy(&id, signature.data(), sizeof(id));
  return LittleEndian(id);
}
}
