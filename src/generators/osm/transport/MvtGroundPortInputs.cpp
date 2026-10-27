#include "MvtGroundPortInputs.h"
#include "MvtTransportSnapshotInternal.h"
#include "OsmField.h"
#include "Sha256.h"
#include "StreetField.h"
#include "BinaryValueArchive.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
using Error = MvtGroundPortInputError;
using Signature = MvtTransport::Signature;
constexpr size_t kTileMetadataAllowanceBytes = 512;

struct Omission {
  uint32_t Source = 0;
  Signature Feature{};
  bool Matched = false;
};

auto OmissionKey(const Omission &one) {
  return std::tie(one.Source, one.Feature);
}

bool Charge(size_t count, size_t unit, size_t &remaining) {
  if (unit != 0 && count > remaining / unit) { return false; }
  remaining -= count * unit;
  return true;
}

bool Admit(const MvtTransportSnapshot &snapshot,
           const OsmField &field,
           const StreetField &streets,
           MvtGroundPortInputLimits limits) {
  const auto &graph = snapshot.Network->Topology();
  const auto &provenance = snapshot.Network->Provenance();
  if (graph.Nodes().size() > limits.Nodes || graph.Edges().size() > limits.DirectedEdges ||
      provenance.Paths.size() > limits.Paths || streets.Ways().size() > limits.SourceWays ||
      field.Tiles().size() > limits.SourceTiles || snapshot.Omissions.size() > limits.Omissions ||
      provenance.Sources.size() > field.Tiles().size() ||
      provenance.References.size() != provenance.Paths.size()) {
    return false;
  }
  size_t remaining = limits.ContainerBytes;
  for (const auto &tile : field.Tiles()) {
    if (!Charge(tile.Source.SourceId.size(), 4, remaining) ||
        !Charge(tile.Source.Revision.size(), 4, remaining) ||
        !Charge(kTileMetadataAllowanceBytes, 1, remaining)) {
      return false;
    }
  }
  return Charge(graph.Nodes().size(), sizeof(uint64_t) + sizeof(uint8_t), remaining) &&
         Charge(provenance.Paths.size(),
                sizeof(GroundPortSourceProfile) + sizeof(double) + 1,
                remaining) &&
         Charge(snapshot.Omissions.size(), sizeof(Omission), remaining) &&
         Charge(field.Tiles().size(),
                sizeof(Signature) + sizeof(uint32_t) + 2 * sizeof(World::TransportSource),
                remaining);
}

bool SameSource(const World::TransportSource &a, const World::TransportSource &b) {
  return a.Identity == b.Identity && a.Item == b.Item && a.Content == b.Content;
}

bool GroundProfile(const OsmField &field, const OsmField::Feature &feature) {
  constexpr std::array<std::pair<std::string_view, std::string_view>, 2> kAbsentValues{
      {{"indoor", "no"}, {"level", "0"}}};
  return std::ranges::all_of(kAbsentValues, [&](const auto &attribute) {
    const auto tag = field.ReadAttribute(feature, attribute.first);
    return !tag || (tag->IsNumber ? tag->Number == 0 : tag->String == attribute.second);
  });
}

struct Binding {
  double WidthM = 0;
  double MaximumGradient = 0;
  uint32_t Source = 0;
  Signature Feature{};
};

std::expected<Binding, Error> ReadBinding(const MvtTransportSnapshot &snapshot,
                                          const OsmField &field,
                                          const StreetField &streets,
                                          const MvtTransport::Sources &actual,
                                          std::span<const Signature> sourceKeys,
                                          size_t index,
                                          MvtGroundPortInputLimits limits,
                                          MvtGroundPortInputWork &work) {
  const auto &provenance = snapshot.Network->Provenance();
  const auto &path = provenance.Paths[index];
  if (path.ReferenceCount != 1 || path.FirstReference >= provenance.References.size() ||
      (index != 0 && provenance.Paths[index - 1].PathId >= path.PathId)) {
    return std::unexpected(Error::InvalidBinding);
  }
  const auto &reference = provenance.References[path.FirstReference];
  const uint32_t wayIndex = snapshot.StreetWays[index];
  if (wayIndex >= streets.Ways().size()) { return std::unexpected(Error::InvalidBinding); }
  const auto source = streets.SourceOf(field, wayIndex);
  const auto &way = streets.Ways()[wayIndex];
  if (!source || way.PointCount < 2 || source->Feature >= field.Features().size()) {
    return std::unexpected(Error::SourceChanged);
  }
  const auto &feature = field.Features()[source->Feature];
  if (feature.Tile >= actual.TileRows.size() || source->Ring < feature.FirstRing ||
      source->Ring - feature.FirstRing >= feature.RingCount ||
      reference.Source != actual.TileRows[feature.Tile] || reference.Source >= sourceKeys.size() ||
      reference.ProviderElementId != feature.ProviderFeatureId ||
      reference.SegmentOrdinal >= way.PointCount - 1 || source->Ring >= field.Rings().size()) {
    return std::unexpected(Error::InvalidBinding);
  }
  const auto &ring = field.Rings()[source->Ring];
  const size_t pointCount = field.Points().size() / 2;
  if (way.FirstPoint != ring.First || way.PointCount != ring.Count || way.FirstPoint > pointCount ||
      way.PointCount > pointCount - way.FirstPoint) {
    return std::unexpected(Error::InvalidBinding);
  }
  const auto fragment = MvtTransport::WaySignature(sourceKeys[reference.Source],
                                                   feature.ContentDigest,
                                                   field.LayerName(static_cast<int>(feature.Layer)),
                                                   source->Ring - feature.FirstRing);
  if (reference.Fragment != fragment ||
      path.PathId != MvtTransport::Identifier(
                         MvtTransport::SegmentSignature(fragment, reference.SegmentOrdinal))) {
    return std::unexpected(Error::InvalidBinding);
  }
  if (feature.TagCount > (limits.AttributeWords - work.AttributeWords) / 2) {
    return std::unexpected(Error::CapacityExceeded);
  }
  work.AttributeWords += static_cast<size_t>(feature.TagCount) * 2;
  const bool ground = GroundProfile(field, feature);
  const auto gradient = static_cast<double>(way.MaxGradient);
  const bool profile = ground && std::isfinite(gradient) && gradient > 0 && gradient <= 10;
  return Binding{.WidthM = static_cast<double>(way.HalfWidthM) * 2,
                 .MaximumGradient = profile ? gradient : 0,
                 .Source = reference.Source,
                 .Feature = feature.ContentDigest};
}

std::expected<void, Error> MarkUnknown(uint64_t node,
                                       const World::TransportTopology &graph,
                                       std::span<uint8_t> unknown,
                                       MvtGroundPortInputWork &work) {
  ++work.NodeLookups;
  const auto nodes = graph.Nodes();
  const auto found = std::ranges::lower_bound(nodes, node, {}, &World::TransportNode::SourceNodeId);
  if (found == nodes.end() || found->SourceNodeId != node) {
    return std::unexpected(Error::InvalidBinding);
  }
  unknown[static_cast<size_t>(found - nodes.begin())] = 1;
  return {};
}

void BindProfileInput(MvtGroundPortInputs &out, std::span<const Signature> sources) {
  constexpr std::string_view kVersion = "outshine/mvt/ground-port-inputs/1";
  Sha256Prefix hash(kVersion.data(), kVersion.size());
  const auto sourceCount = LittleEndian(static_cast<uint64_t>(sources.size()));
  hash.Append(&sourceCount, sizeof(sourceCount));
  for (const auto &source : sources) { hash.Append(source.data(), source.size()); }
  const auto profileCount = LittleEndian(static_cast<uint64_t>(out.Profiles.size()));
  hash.Append(&profileCount, sizeof(profileCount));
  for (const auto &profile : out.Profiles) {
    const auto path = LittleEndian(profile.PathId);
    const auto gradient = LittleEndian(profile.MaximumGradient);
    hash.Append(&path, sizeof(path));
    hash.Append(&gradient, sizeof(gradient));
  }
  const auto unknownCount = LittleEndian(static_cast<uint64_t>(out.UnknownNodes.size()));
  hash.Append(&unknownCount, sizeof(unknownCount));
  for (const auto node : out.UnknownNodes) {
    const auto encoded = LittleEndian(node);
    hash.Append(&encoded, sizeof(encoded));
  }
  out.ProfileInput = {.DatasetId = "outshine/mvt/ground-port-inputs",
                      .Version = "1",
                      .Revision = "native-way-profile-and-frontier",
                      .ItemId = "ground",
                      .Content = hash.Digest(nullptr, 0)};
}

size_t SourceCapacity(const MvtTransport::Sources &sources) {
  size_t bytes = sources.TileRows.capacity() * sizeof(uint32_t) +
                 sources.Provenance.Sources.capacity() * sizeof(World::TransportSource);
  for (const auto &source : sources.Provenance.Sources) {
    bytes += source.Identity.DatasetId.capacity() + source.Identity.Revision.capacity() +
             source.Item.capacity();
  }
  return bytes;
}

struct ProfileBindings {
  std::vector<double> Widths;
  std::vector<uint8_t> Omitted;
  size_t CapacityBytes = 0;
};

std::expected<ProfileBindings, Error> ReadProfiles(const MvtTransportSnapshot &snapshot,
                                                   const OsmField &field,
                                                   const StreetField &streets,
                                                   const MvtTransport::Sources &actual,
                                                   std::span<const Signature> sourceKeys,
                                                   MvtGroundPortInputLimits limits,
                                                   MvtGroundPortInputs &out,
                                                   const std::stop_token &stop) {
  const auto &paths = snapshot.Network->Provenance().Paths;
  std::vector<Omission> omissions;
  omissions.reserve(snapshot.Omissions.size());
  for (const auto &one : snapshot.Omissions) {
    if (one.Reason == MvtTransportOmissionReason::UnqualifiedLevelConnection ||
        one.Reason == MvtTransportOmissionReason::MissingTopology) {
      omissions.push_back({.Source = one.Source, .Feature = one.Feature});
    }
  }
  std::ranges::sort(omissions, {}, OmissionKey);
  const auto duplicate = std::ranges::unique(omissions, {}, OmissionKey);
  omissions.erase(duplicate.begin(), duplicate.end());
  ProfileBindings result;
  auto &widths = result.Widths;
  auto &omitted = result.Omitted;
  widths.reserve(paths.size());
  omitted.reserve(paths.size());
  out.Profiles.reserve(paths.size());
  for (size_t at = 0; at < paths.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const auto binding =
        ReadBinding(snapshot, field, streets, actual, sourceKeys, at, limits, out.Work);
    if (!binding) { return std::unexpected(binding.error()); }
    ++out.Work.Paths;
    widths.push_back(binding->WidthM);
    ++out.Work.OmissionLookups;
    const auto key = std::tie(binding->Source, binding->Feature);
    const auto found = std::ranges::lower_bound(omissions, key, {}, OmissionKey);
    const bool unknown = found != omissions.end() && OmissionKey(*found) == key;
    if (unknown) { found->Matched = true; }
    omitted.push_back(static_cast<uint8_t>(unknown));
    if (binding->MaximumGradient > 0) {
      out.Profiles.push_back(
          {.PathId = paths[at].PathId, .MaximumGradient = binding->MaximumGradient});
    } else {
      ++out.Work.MissingProfiles;
    }
  }
  if (std::ranges::any_of(omissions, [](const Omission &one) { return !one.Matched; })) {
    return std::unexpected(Error::MissingIncidence);
  }
  result.CapacityBytes = omissions.capacity() * sizeof(Omission) +
                         widths.capacity() * sizeof(double) + omitted.capacity();
  return result;
}

std::expected<size_t, Error> ReadUnknownNodes(const MvtTransportSnapshot &snapshot,
                                              const ProfileBindings &profiles,
                                              MvtGroundPortInputs &out,
                                              const std::stop_token &stop) {
  const auto &paths = snapshot.Network->Provenance().Paths;
  const auto &graph = snapshot.Network->Topology();
  std::vector<uint8_t> unknown(graph.Nodes().size());
  size_t path = 0;
  for (const auto &edge : graph.Edges()) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    ++out.Work.DirectedEdges;
    while (path < paths.size() && paths[path].PathId < edge.Id.PathId) { ++path; }
    if (path == paths.size() || paths[path].PathId != edge.Id.PathId ||
        edge.WidthM != profiles.Widths[path]) {
      return std::unexpected(Error::InvalidBinding);
    }
    if (profiles.Omitted[path] != 0) {
      const auto from = MarkUnknown(edge.FromNodeId, graph, unknown, out.Work);
      const auto to = MarkUnknown(edge.ToNodeId, graph, unknown, out.Work);
      if (!from || !to) { return std::unexpected(Error::InvalidBinding); }
    }
  }
  out.UnknownNodes.reserve(graph.Nodes().size());
  for (size_t at = 0; at < unknown.size(); ++at) {
    if (unknown[at] != 0) { out.UnknownNodes.push_back(graph.Nodes()[at].SourceNodeId); }
  }
  return unknown.capacity();
}

}

std::expected<MvtGroundPortInputs, MvtGroundPortInputError>
BindMvtGroundPortInputs(const MvtTransportSnapshot &snapshot,
                        const OsmField &field,
                        const StreetField &streets,
                        MvtGroundPortInputLimits limits,
                        const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (!snapshot.Network) { return std::unexpected(Error::MissingSource); }
  if (snapshot.SourceOrigin.get() != field.OriginToken() ||
      snapshot.SourceGeneration != field.Generation() ||
      snapshot.SourceWayCount != streets.Ways().size() || !streets.SourceDigest(field, 0)) {
    return std::unexpected(Error::SourceChanged);
  }
  if (!Admit(snapshot, field, streets, limits)) { return std::unexpected(Error::CapacityExceeded); }
  const auto &paths = snapshot.Network->Provenance().Paths;
  if (snapshot.StreetWays.size() != paths.size()) { return std::unexpected(Error::InvalidBinding); }
  auto actual = MvtTransport::SourceRows(field);
  if (!actual || !std::ranges::equal(actual->Provenance.Sources,
                                     snapshot.Network->Provenance().Sources,
                                     SameSource)) {
    return std::unexpected(Error::InvalidBinding);
  }
  std::vector<Signature> sourceKeys;
  sourceKeys.reserve(actual->Provenance.Sources.size());
  for (const auto &source : actual->Provenance.Sources) {
    sourceKeys.push_back(MvtTransport::SourceSignature(source));
  }
  MvtGroundPortInputs out;
  out.Network = snapshot.Network;
  out.Work.SourceTiles = field.Tiles().size();
  const auto profiles =
      ReadProfiles(snapshot, field, streets, *actual, sourceKeys, limits, out, stop);
  if (!profiles) { return std::unexpected(profiles.error()); }
  const auto unknownBytes = ReadUnknownNodes(snapshot, *profiles, out, stop);
  if (!unknownBytes) { return std::unexpected(unknownBytes.error()); }
  out.Work.ContainerCapacityBytes = SourceCapacity(*actual) +
                                    sourceKeys.capacity() * sizeof(Signature) +
                                    profiles->CapacityBytes + *unknownBytes +
                                    out.Profiles.capacity() * sizeof(GroundPortSourceProfile) +
                                    out.UnknownNodes.capacity() * sizeof(uint64_t);
  if (out.Work.ContainerCapacityBytes > limits.ContainerBytes) {
    return std::unexpected(Error::CapacityExceeded);
  }
  BindProfileInput(out, sourceKeys);
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  return out;
}
}
