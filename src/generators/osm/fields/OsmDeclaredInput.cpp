#include "OsmField.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <string_view>

namespace outshine::Generators::Osm {

std::expected<std::string, std::string_view>
OsmField::DeclarationDigest(std::span<const Declared> these) {
  BinaryValueWriter out(std::numeric_limits<std::size_t>::max());
  if (!out.Text("outshine-declared-osm-1") || !out(static_cast<std::uint64_t>(these.size()))) {
    return std::unexpected("declared vector identity exceeds its encoding limit");
  }
  for (const auto &one : these) {
    if (!out.Text(one.Layer) || !out.Text(one.Key) || !out.Text(one.Value) ||
        !out(one.WidthM, one.HeightM, one.Area, one.Bridge, one.Tunnel, one.Level) ||
        !out.Array(std::span(one.LatLon))) {
      return std::unexpected("declared vector identity exceeds its encoding limit");
    }
  }
  const auto bytes = out.Out.Bytes();
  return Sha256Hex(bytes.data(), bytes.size());
}

}
