#include "PreparedStreetGraph.h"
#include "BinaryValueArchive.h"
#include "OsmField.h"
#include "Sha256.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>

namespace outshine::Generators::Osm {
namespace {
constexpr size_t kKeyBytesMost = size_t{256} * 1024u * 1024u;

bool WriteWay(BinaryValueWriter &out, const StreetField::Way &way, std::span<const double> points) {
  const size_t first = static_cast<size_t>(way.FirstPoint) * 2;
  const size_t count = static_cast<size_t>(way.PointCount) * 2;
  return first <= points.size() && count <= points.size() - first &&
         out(way.FirstPoint,
             way.PointCount,
             way.HalfWidthM,
             way.CoverRow,
             way.Form,
             way.Lanes,
             way.Layer,
             way.ClearanceM,
             way.MaxGradient,
             way.SpeedMps,
             way.Priority,
             way.Sealed,
             way.Oneway,
             way.Bridge) &&
         out.Array(points.subspan(first, count));
}
}

std::expected<std::string, std::string>
PreparedStreetGraph::Key(const OsmField &vectors,
                         const StreetField &ways,
                         const ::outshine::Ground::ShapedGround &shape,
                         int sourceZoom) const {
  BinaryValueWriter out(kKeyBytesMost);
  if (!out.Text(Recipe_) || !out.Text(shape.Kind) ||
      !out(shape.AmplitudeM,
           shape.WavelengthM,
           shape.Gradient,
           shape.BearingDeg,
           shape.FocusLatDeg,
           shape.FocusLonDeg,
           shape.Seed,
           sourceZoom,
           vectors.Schema(),
           static_cast<uint64_t>(vectors.Tiles().size())) ||
      !std::ranges::all_of(vectors.Tiles(),
                           [&](const auto &tile) {
                             return out(tile.Z, tile.X, tile.Y) && out.Text(tile.InputDigest);
                           }) ||
      !out(static_cast<uint64_t>(ways.Ways().size())) ||
      !std::ranges::all_of(ways.Ways(),
                           [&](const auto &way) { return WriteWay(out, way, vectors.Points()); })) {
    return std::unexpected("street graph asset inputs exceed the key encoding limit");
  }
  return Sha256Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}
}
