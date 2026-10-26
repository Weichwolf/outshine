#include "MvtTransportSnapshotInternal.h"
#include "OsmField.h"
#include "OsmLayer.h"
#include "OsmSurfacePlacement.h"
#include "StreetField.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <stop_token>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm::MvtTransport {
namespace {
std::expected<void, std::string> AppendWay(Sources &out,
                                           const OsmField &field,
                                           const StreetField &streets,
                                           size_t at,
                                           std::span<const Signature> sourceKeys,
                                           std::vector<bool> &represented) {
  if (streets.Ways()[at].PointCount < 2) {
    out.Ways.emplace_back();
    return {};
  }
  auto way = DescribeWay(field, streets, at);
  if (!way) { return std::unexpected(way.error()); }
  const auto reference = streets.SourceOf(field, at);
  if (!reference) { return std::unexpected("MVT transport street source lease changed"); }
  const auto &feature = field.Features()[reference->Feature];
  way->Source.Source = out.TileRows[feature.Tile];
  way->Key = WaySignature(sourceKeys[way->Source.Source],
                          feature.ContentDigest,
                          field.LayerName(static_cast<int>(feature.Layer)),
                          reference->Ring - feature.FirstRing);
  way->Source.Fragment = way->Key;
  if (!way->Usable && !represented[reference->Feature]) {
    out.Omissions.push_back({.Source = way->Source.Source,
                             .Feature = way->Feature,
                             .Reason = streets.Ways()[at].Form == StreetField::Shape::Area
                                           ? MvtTransportOmissionReason::UnsupportedArea
                                           : MvtTransportOmissionReason::InvalidAttributes});
  }
  represented[reference->Feature] = true;
  out.Ways.push_back(*way);
  return {};
}

MvtTransportOmissionReason OmissionReason(const OsmField &field, uint32_t feature) {
  const auto placement = ReadSurfacePlacement(field, feature);
  if (!placement) { return MvtTransportOmissionReason::InvalidAttributes; }
  if (placement->Tunnel == true) { return MvtTransportOmissionReason::UnsupportedTunnel; }
  return MvtTransportOmissionReason::UnclassifiedWay;
}
}

std::expected<Sources, std::string>
PrepareSources(const OsmField &field, const StreetField &streets, const std::stop_token &stop) {
  auto prepared = SourceRows(field);
  if (!prepared) { return prepared; }
  auto &out = *prepared;
  std::vector<Signature> sourceKeys;
  sourceKeys.reserve(out.Provenance.Sources.size());
  for (const auto &source : out.Provenance.Sources) {
    sourceKeys.push_back(SourceSignature(source));
  }
  std::vector<bool> represented(field.Features().size());
  out.Ways.reserve(streets.Ways().size());
  for (size_t at = 0; at < streets.Ways().size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
    const auto added = AppendWay(out, field, streets, at, sourceKeys, represented);
    if (!added) { return std::unexpected(added.error()); }
  }
  const int streetsLayer = field.Layer(OsmLayer::Streets);
  const int areasLayer = field.Layer(OsmLayer::StreetPolygons);
  for (size_t at = 0; at < field.Features().size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
    const auto &feature = field.Features()[at];
    if (represented[at] || (!std::cmp_equal(feature.Layer, streetsLayer) &&
                            !std::cmp_equal(feature.Layer, areasLayer))) {
      continue;
    }
    out.Omissions.push_back({.Source = out.TileRows[feature.Tile],
                             .Feature = feature.ContentDigest,
                             .Reason = OmissionReason(field, static_cast<uint32_t>(at))});
  }
  return prepared;
}
}
