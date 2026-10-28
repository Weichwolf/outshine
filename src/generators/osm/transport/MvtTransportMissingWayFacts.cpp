#include "MvtTransportSourceFactsInternal.h"
#include "OsmField.h"
#include "StreetField.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace outshine::Generators::Osm::MvtTransport {
namespace {
std::optional<uint32_t> OmissionOf(const Sources &sources, const Way &way) {
  if (!way.Usable) { return std::nullopt; }
  const std::optional<Signature> fragment{way.Key};
  const auto key = std::tie(way.Source.Source, way.Feature, fragment);
  const auto identity = [](const MvtTransportOmission &omission) {
    return std::tie(omission.Source, omission.Feature, omission.Fragment);
  };
  auto at = std::ranges::lower_bound(sources.Omissions, key, {}, identity);
  while (at != sources.Omissions.end() && identity(*at) == key) {
    if (at->Reason == MvtTransportOmissionReason::MissingTopology) {
      return static_cast<uint32_t>(at - sources.Omissions.begin());
    }
    ++at;
  }
  return std::nullopt;
}

template <class T> void Append(Sha256Prefix &hash, T value) {
  const auto word = LittleEndian(value);
  hash.Append(&word, sizeof(word));
}

std::expected<void, std::string>
BindOmissions(MvtTransportSourceFacts &facts, const Sources &sources, const std::stop_token &stop) {
  constexpr std::string_view version = "outshine/mvt-source-omissions/1";
  Sha256Prefix hash(version.data(), version.size());
  Append(hash, static_cast<uint64_t>(sources.Omissions.size()));
  for (const auto &omission : sources.Omissions) {
    if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
    Append(hash, omission.Source);
    hash.Append(omission.Feature.data(), omission.Feature.size());
    Append(hash, static_cast<uint8_t>(omission.Fragment.has_value()));
    if (omission.Fragment) { hash.Append(omission.Fragment->data(), omission.Fragment->size()); }
    Append(hash, static_cast<uint8_t>(omission.Reason));
  }
  facts.OmissionsDigest = hash.Digest(nullptr, 0);
  return {};
}
}

bool FitsMissingSourceWays(const SourceFactInput &input,
                           size_t bytesMost,
                           const std::stop_token &stop) {
  if (input.SourceRows.Omissions.size() > std::numeric_limits<uint32_t>::max()) { return false; }
  for (const auto &way : input.SourceRows.Ways) {
    if (stop.stop_requested()) { return false; }
    if (!OmissionOf(input.SourceRows, way)) { continue; }
    if (sizeof(MvtTransportSourceFacts::MissingWay) > bytesMost) { return false; }
    bytesMost -= sizeof(MvtTransportSourceFacts::MissingWay);
    if (way.PointCount > bytesMost / sizeof(LongitudeLatitude)) { return false; }
    bytesMost -= way.PointCount * sizeof(LongitudeLatitude);
  }
  return true;
}

std::expected<void, std::string> FillMissingSourceWays(MvtTransportSourceFacts &facts,
                                                       const SourceFactInput &input,
                                                       const std::stop_token &stop) {
  size_t count = 0;
  for (const auto &way : input.SourceRows.Ways) {
    if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
    if (OmissionOf(input.SourceRows, way)) { ++count; }
  }
  facts.MissingWays.reserve(count);
  const auto points = input.Field.Points();
  for (size_t at = 0; at < input.SourceRows.Ways.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
    const auto &way = input.SourceRows.Ways[at];
    const auto omission = OmissionOf(input.SourceRows, way);
    if (!omission) { continue; }
    auto captured = SourceWayFacts(input, static_cast<uint32_t>(at));
    if (!captured) { return std::unexpected(std::move(captured.error())); }
    MvtTransportSourceFacts::MissingWay missing{.Omission = *omission,
                                                .Way = *captured,
                                                .Layer = way.Attributes.Layer,
                                                .Bridge = way.Attributes.Bridge,
                                                .Tunnel = way.Attributes.Tunnel,
                                                .Points = {}};
    missing.Points.reserve(way.PointCount);
    const size_t first = input.Streets.Ways()[at].FirstPoint;
    for (size_t point = 0; point < way.PointCount; ++point) {
      if (stop.stop_requested()) {
        return std::unexpected("MVT transport normalization cancelled");
      }
      missing.Points.push_back({.LongitudeDeg = points[2 * (first + point) + 1],
                                .LatitudeDeg = points[2 * (first + point)]});
    }
    facts.MissingWays.push_back(std::move(missing));
  }
  return BindOmissions(facts, input.SourceRows, stop);
}
}
