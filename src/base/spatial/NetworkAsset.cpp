#include "Wayfinding.h"
#include "BinaryValueArchive.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <limits>
#include <array>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Path {
namespace {
static_assert(sizeof(size_t) == sizeof(uint64_t));
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
constexpr uint64_t kNetworkAssetFormat = 0x00013154454eULL;
constexpr size_t kWayRecordBytes = 107, kNodeRecordBytes = 68;
constexpr size_t kEdgeRecordBytes = 16, kCrossingRecordBytes = 32;

bool ValidCoordinates(LongitudeLatitude at) {
  return std::isfinite(at.LatitudeDeg) && std::abs(at.LatitudeDeg) <= kDegPerHalfTurn / 2 &&
         std::isfinite(at.LongitudeDeg) && std::abs(at.LongitudeDeg) <= kDegPerHalfTurn;
}

template <class Archive, class Way> bool WayFields(Archive &archive, Way &way) {
  return archive(way.First,
                 way.Count,
                 way.MinLat,
                 way.MinLon,
                 way.MaxLat,
                 way.MaxLon,
                 way.HalfWidthM,
                 way.MaxGradient,
                 way.MinRadiusM,
                 way.Friction,
                 way.SpeedMps,
                 way.Lanes,
                 way.Priority,
                 way.Oneway,
                 way.Sealed,
                 way.Spans,
                 way.Tag);
}

template <class Archive, class Node> bool NodeFields(Archive &archive, Node &node) {
  return archive(node.LatitudeDeg,
                 node.LongitudeDeg,
                 node.FirstEdge,
                 node.EdgeCount,
                 node.HalfWidthM,
                 node.MaxGradient,
                 node.MinRadiusM,
                 node.Friction,
                 node.Lanes);
}

template <class Archive, class Crossing> bool CrossingFields(Archive &archive, Crossing &crossing) {
  return archive(crossing.OverWay,
                 crossing.UnderWay,
                 crossing.LatitudeDeg,
                 crossing.LongitudeDeg,
                 crossing.OverAt,
                 crossing.UnderAt);
}

template <class T, class Fields>
bool WriteRecords(BinaryValueWriter &out, const std::vector<T> &records, Fields fields) {
  return out(static_cast<uint64_t>(records.size())) &&
         std::ranges::all_of(records, [&](const auto &record) { return fields(out, record); });
}

template <class T, class Fields>
bool ReadRecords(BinaryValueReader &in, std::vector<T> &records, size_t wireBytes, Fields fields) {
  uint64_t count = 0;
  if (!in(count) || count > in.In.Remaining() / wireBytes) { return false; }
  records.reserve(static_cast<size_t>(count));
  for (uint64_t index = 0; index < count; ++index) {
    T record;
    if (!fields(in, record)) { return false; }
    records.push_back(std::move(record));
  }
  return true;
}
}

class NetworkAssetCodec {
public:
  static bool Write(BinaryValueWriter &out, const Network &network) {
    if (!out(kNetworkAssetFormat,
             network.SnapM_,
             network.RadiusM_,
             network.IndexedCells_,
             network.Joined_,
             network.LeftAlone_,
             network.Tied_,
             network.Woven_) ||
        !out.Array(std::span(network.Points_)) || !out.Array(std::span(network.WayOf_)) ||
        !out.Array(std::span(network.NodeOfPoint_)) || !out.Array(std::span(network.HeightsM_)) ||
        !out.Array(std::span(network.StationM_)) || !out.Array(std::span(network.SlopeM_)) ||
        !WriteRecords(out, network.Ways_, WayFields<BinaryValueWriter, const Network::Way>) ||
        !WriteRecords(out, network.Nodes_, NodeFields<BinaryValueWriter, const Network::Node>) ||
        !WriteRecords(out,
                      network.Edges_,
                      [](auto &archive, auto &edge) { return archive(edge.To, edge.LengthM); }) ||
        !WriteRecords(out,
                      network.CachedCrossings_,
                      CrossingFields<BinaryValueWriter, const Network::Crossing>)) {
      return false;
    }
    const bool swept = network.CachedSweep_.has_value();
    if (!out(swept)) { return false; }
    return !swept || out(network.CachedSweep_->Found,
                         network.CachedSweep_->PairsTested,
                         network.CachedSweep_->FullestCell,
                         network.CachedSweep_->PairsPruned,
                         network.CachedSweep_->CandidatePairs);
  }

  static bool Read(BinaryValueReader &in, Network &network) {
    uint64_t format = 0;
    if (!in(format,
            network.SnapM_,
            network.RadiusM_,
            network.IndexedCells_,
            network.Joined_,
            network.LeftAlone_,
            network.Tied_,
            network.Woven_) ||
        format != kNetworkAssetFormat || !in.Array(network.Points_) || !in.Array(network.WayOf_) ||
        !in.Array(network.NodeOfPoint_) || !in.Array(network.HeightsM_) ||
        !in.Array(network.StationM_) || !in.Array(network.SlopeM_) ||
        !ReadRecords(
            in, network.Ways_, kWayRecordBytes, WayFields<BinaryValueReader, Network::Way>) ||
        !ReadRecords(
            in, network.Nodes_, kNodeRecordBytes, NodeFields<BinaryValueReader, Network::Node>) ||
        !ReadRecords(in,
                     network.Edges_,
                     kEdgeRecordBytes,
                     [](auto &archive, auto &edge) { return archive(edge.To, edge.LengthM); }) ||
        !ReadRecords(in,
                     network.CachedCrossings_,
                     kCrossingRecordBytes,
                     CrossingFields<BinaryValueReader, Network::Crossing>)) {
      return false;
    }
    bool swept = false;
    if (!in(swept)) { return false; }
    if (swept) {
      network.CachedSweep_.emplace();
      if (!in(network.CachedSweep_->Found,
              network.CachedSweep_->PairsTested,
              network.CachedSweep_->FullestCell,
              network.CachedSweep_->PairsPruned,
              network.CachedSweep_->CandidatePairs) ||
          network.CachedSweep_->Found != network.CachedCrossings_.size()) {
        return false;
      }
    } else if (!network.CachedCrossings_.empty()) {
      return false;
    }
    if (!Validate(network)) { return false; }
    Index(network);
    return true;
  }

  static bool Validate(const Network &network) {
    const size_t points = network.PointCount();
    if (!Network::Create({.CellM = network.SnapM_}, {.RadiusM = network.RadiusM_}) ||
        (!network.Woven_ && !network.Ways_.empty()) || network.Points_.size() % 2 != 0 ||
        points > kMaxNetworkPoints || network.WayOf_.size() != points ||
        network.NodeOfPoint_.size() != points || network.HeightsM_.size() != points ||
        network.StationM_.size() != points || network.SlopeM_.size() != points ||
        !std::ranges::all_of(network.HeightsM_,
                             [](double value) { return std::isfinite(value); }) ||
        !std::ranges::all_of(network.StationM_,
                             [](double value) { return std::isfinite(value) && value >= 0; }) ||
        !std::ranges::all_of(network.SlopeM_, [](double value) { return std::isfinite(value); })) {
      return false;
    }
    for (size_t at = 0; at < points; ++at) {
      if (!ValidCoordinates({.LongitudeDeg = network.Points_[2 * at + 1],
                             .LatitudeDeg = network.Points_[2 * at]})) {
        return false;
      }
    }
    return ValidateWays(network) && ValidateNodes(network) && ValidateCrossings(network) &&
           std::ranges::all_of(network.Edges_, [&network](const auto &edge) {
             return edge.To < network.Nodes_.size() && std::isfinite(edge.LengthM) &&
                    edge.LengthM >= 0;
           });
  }

private:
  static bool ValidateWays(const Network &network) {
    const size_t points = network.PointCount();
    for (const auto &way : network.Ways_) {
      if (way.First > points || way.Count < 2 || way.Count > points - way.First || way.Lanes < 0) {
        return false;
      }
      for (const double value :
           {way.HalfWidthM, way.MaxGradient, way.MinRadiusM, way.Friction, way.SpeedMps}) {
        if (!std::isfinite(value) || value < 0) { return false; }
      }
      for (size_t at = way.First; at < way.First + way.Count; ++at) {
        if (std::cmp_not_equal(network.WayOf_[at], &way - network.Ways_.data())) { return false; }
      }
    }
    return true;
  }

  static bool ValidateNodes(const Network &network) {
    for (const auto &node : network.Nodes_) {
      if (!ValidCoordinates({.LongitudeDeg = node.LongitudeDeg, .LatitudeDeg = node.LatitudeDeg}) ||
          node.Lanes < 0 || node.FirstEdge > network.Edges_.size() ||
          node.EdgeCount > network.Edges_.size() - node.FirstEdge) {
        return false;
      }
      for (const double value :
           {node.HalfWidthM, node.MaxGradient, node.MinRadiusM, node.Friction}) {
        if (!std::isfinite(value) || value < 0) { return false; }
      }
    }
    return std::ranges::all_of(network.WayOf_,
                               [&network](uint32_t way) { return way < network.Ways_.size(); }) &&
           std::ranges::all_of(network.NodeOfPoint_,
                               [&network](uint32_t node) { return node < network.Nodes_.size(); });
  }

  static bool ValidateCrossings(const Network &network) {
    return std::ranges::all_of(network.CachedCrossings_, [&network](const auto &crossing) {
      if (crossing.OverWay >= network.Ways_.size() || crossing.UnderWay >= network.Ways_.size() ||
          !ValidCoordinates(
              {.LongitudeDeg = crossing.LongitudeDeg, .LatitudeDeg = crossing.LatitudeDeg})) {
        return false;
      }
      const auto &over = network.Ways_[crossing.OverWay];
      const auto &under = network.Ways_[crossing.UnderWay];
      return crossing.OverAt >= over.First && crossing.OverAt < over.First + over.Count - 1 &&
             crossing.UnderAt >= under.First && crossing.UnderAt < under.First + under.Count - 1;
    });
  }

  static void Index(Network &network) {
    for (size_t index = 0; index < network.Nodes_.size(); ++index) {
      const auto &node = network.Nodes_[index];
      network
          .Cells_[network.CellOf(
              {.LongitudeDeg = node.LongitudeDeg, .LatitudeDeg = node.LatitudeDeg})]
          .push_back(index);
    }
  }
};

std::optional<std::vector<uint8_t>> Network::EncodeAsset(size_t bytesMost) const {
  if (!NetworkAssetCodec::Validate(*this)) { return std::nullopt; }
  BinaryValueWriter out(bytesMost);
  if (!NetworkAssetCodec::Write(out, *this)) { return std::nullopt; }
  return std::move(out.Out).TakeBytes();
}

std::optional<Network> Network::DecodeAsset(std::span<const uint8_t> bytes, size_t bytesMost) {
  if (bytes.size() > bytesMost) { return std::nullopt; }
  BinaryValueReader in(bytes);
  Network network({}, {});
  if (!NetworkAssetCodec::Read(in, network) || in.In.Remaining() != 0) { return std::nullopt; }
  return network;
}
}
