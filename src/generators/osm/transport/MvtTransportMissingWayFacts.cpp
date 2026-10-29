#include "MvtTransportSourceFactsInternal.h"
#include "OsmField.h"
#include "StreetField.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include "Wayfinding.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm::MvtTransport {
namespace {
constexpr uint32_t kNoWay = std::numeric_limits<uint32_t>::max();

struct OriginalWayIndex {
  std::vector<uint32_t> First;
  std::vector<uint32_t> Next;
};

std::expected<OriginalWayIndex, std::string> IndexOriginalWays(const SourceFactInput &input,
                                                               const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
  OriginalWayIndex index{.First = std::vector<uint32_t>(input.SourceRows.Ways.size(), kNoWay),
                         .Next = std::vector<uint32_t>(input.Network.WayCount(), kNoWay)};
  for (size_t at = 0; at < input.Network.WayCount(); ++at) {
    if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
    const size_t tag = input.Network.TagOf(at);
    if (tag >= index.First.size()) { continue; }
    index.Next[at] = index.First[tag];
    index.First[tag] = static_cast<uint32_t>(at);
  }
  return index;
}

std::expected<bool, std::string> SameOriginalRing(const Path::Network &network,
                                                  uint32_t way,
                                                  std::span<const LongitudeLatitude> points,
                                                  const std::stop_token &stop) {
  const auto coordinates = network.CoordinatesOfWay(way);
  if (coordinates.size() / 2 != points.size() || coordinates.size() % 2 != 0) { return false; }
  for (size_t at = 0; at < points.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
    if (coordinates[2 * at] != points[at].LatitudeDeg ||
        coordinates[2 * at + 1] != points[at].LongitudeDeg) {
      return false;
    }
  }
  return true;
}

std::expected<void, std::string> BindOriginalPoints(MvtTransportSourceFacts::MissingWay &missing,
                                                    const SourceFactInput &input,
                                                    const OriginalWayIndex &index,
                                                    std::span<const uint64_t> mapped,
                                                    const std::stop_token &stop) {
  using State = MvtTransportOriginalWayState;
  missing.OriginalWayState = State::AbsentInCapturedNetwork;
  std::optional<uint32_t> exact;
  for (uint32_t at = index.First[missing.Way.StreetWay]; at != kNoWay; at = index.Next[at]) {
    if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
    if (missing.OriginalWayState == State::AbsentInCapturedNetwork) {
      missing.OriginalWayState = State::InexactTagCandidate;
    }
    const auto same = SameOriginalRing(input.Network, at, missing.Points, stop);
    if (!same) { return std::unexpected(same.error()); }
    if (!*same) { continue; }
    if (exact) {
      missing.OriginalWayState = State::AmbiguousExact;
      exact.reset();
      break;
    }
    missing.OriginalWayState = State::Exact;
    exact = at;
  }
  missing.OriginalPoints.resize(missing.Points.size());
  if (!exact) { return {}; }
  for (size_t at = 0; at < missing.OriginalPoints.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
    const auto node = input.Network.NodeOfWayPoint(*exact, at);
    if (!node || *node >= mapped.size()) { continue; }
    auto &point = missing.OriginalPoints[at];
    point.State = MvtTransportOriginalPointState::ValidNativeNodeWithoutPublishedGroup;
    if (mapped[*node] == 0) { continue; }
    point.State = MvtTransportOriginalPointState::PublishedGroup;
    point.Group = static_cast<uint32_t>(mapped[*node] - 1);
  }
  return {};
}

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
  bool missing = false;
  for (const auto &way : input.SourceRows.Ways) {
    if (stop.stop_requested()) { return false; }
    if (!OmissionOf(input.SourceRows, way)) { continue; }
    missing = true;
    if (sizeof(MvtTransportSourceFacts::MissingWay) > bytesMost) { return false; }
    bytesMost -= sizeof(MvtTransportSourceFacts::MissingWay);
    constexpr size_t kPointBytes = sizeof(LongitudeLatitude) + sizeof(MvtTransportOriginalPoint);
    if (way.PointCount > bytesMost / kPointBytes) { return false; }
    bytesMost -= way.PointCount * kPointBytes;
  }
  if (!missing) { return true; }
  if (input.Network.WayCount() >= kNoWay || input.SourceRows.Ways.size() > kNoWay ||
      input.SourceRows.Ways.size() > bytesMost / sizeof(uint32_t)) {
    return false;
  }
  bytesMost -= input.SourceRows.Ways.size() * sizeof(uint32_t);
  if (input.Network.WayCount() > bytesMost / sizeof(uint32_t)) { return false; }
  return true;
}

std::expected<void, std::string> FillMissingSourceWays(MvtTransportSourceFacts &facts,
                                                       const SourceFactInput &input,
                                                       std::span<const uint64_t> mapped,
                                                       const std::stop_token &stop) {
  size_t count = 0;
  for (const auto &way : input.SourceRows.Ways) {
    if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
    if (OmissionOf(input.SourceRows, way)) { ++count; }
  }
  facts.MissingWays.reserve(count);
  if (count == 0) { return BindOmissions(facts, input.SourceRows, stop); }
  const auto originals = IndexOriginalWays(input, stop);
  if (!originals) { return std::unexpected(originals.error()); }
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
                                                .Points = {},
                                                .OriginalPoints = {}};
    missing.Points.reserve(way.PointCount);
    const size_t first = input.Streets.Ways()[at].FirstPoint;
    for (size_t point = 0; point < way.PointCount; ++point) {
      if (stop.stop_requested()) {
        return std::unexpected("MVT transport normalization cancelled");
      }
      missing.Points.push_back({.LongitudeDeg = points[2 * (first + point) + 1],
                                .LatitudeDeg = points[2 * (first + point)]});
    }
    if (auto bound = BindOriginalPoints(missing, input, *originals, mapped, stop); !bound) {
      return bound;
    }
    facts.MissingWays.push_back(std::move(missing));
  }
  return BindOmissions(facts, input.SourceRows, stop);
}
}
