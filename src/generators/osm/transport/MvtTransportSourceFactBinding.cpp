#include "MvtTransportSnapshotInternal.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include "Sha256Stream.h"
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace outshine::Generators::Osm::MvtTransport {
namespace {
template <class Hash, class T> void Append(Hash &hash, T value) {
  const auto word = LittleEndian(value);
  hash.Append(&word, sizeof(word));
}

template <class Hash> void AppendWay(Hash &hash, const MvtTransportWayFacts &way) {
  Append(hash, way.StreetWay);
  Append(hash, way.Feature);
  Append(hash, way.Ring);
  Append(hash, way.PointCount);
  Append(hash, way.HalfWidthWord);
  Append(hash, way.MaximumGradientWord);
  for (const auto &end : way.EndStencil) {
    Append(hash, end.LongitudeDeg);
    Append(hash, end.LatitudeDeg);
  }
  for (const bool owns : way.OwnsEnds) { Append(hash, static_cast<uint8_t>(owns)); }
}

template <class Hash> void WriteFacts(Hash &hash, const MvtTransportSourceFacts &facts) {
  constexpr std::string_view kVersion = "outshine/mvt-source-facts/4";
  hash.Append(kVersion.data(), kVersion.size());
  const auto &revision = facts.Network->SourceIdentity().Revision;
  hash.Append(revision.data(), revision.size());
  hash.Append(facts.NodeGroups->Digest.data(), facts.NodeGroups->Digest.size());
  Append(hash, facts.SourceGeneration);
  Append(hash, facts.SourceWayCount);
  Append(hash, static_cast<uint64_t>(facts.Groups.size()));
  Append(hash, static_cast<uint64_t>(facts.Members.size()));
  Append(hash, static_cast<uint64_t>(facts.Ways.size()));
  hash.Append(facts.OmissionsDigest.data(), facts.OmissionsDigest.size());
  Append(hash, static_cast<uint64_t>(facts.MissingWays.size()));
  for (const auto &group : facts.Groups) {
    Append(hash, group.FirstMember);
    Append(hash, group.MemberCount);
    Append(hash, group.OriginalEndpointCount);
    Append(hash, group.FilteredEndpointCount);
    Append(hash, static_cast<uint8_t>(group.Complete));
    Append(hash, static_cast<uint8_t>(group.Frontier));
  }
  for (const auto &member : facts.Members) {
    Append(hash, member.Edge.PathId);
    Append(hash, member.Edge.SegmentOrdinal);
    Append(hash, static_cast<uint8_t>(member.Edge.Direction));
    Append(hash, member.DomainNode);
    Append(hash, member.NodeGroup);
    Append(hash, member.StreetWay);
    Append(hash, member.SourceStep);
    Append(hash, member.End);
    Append(hash, static_cast<uint8_t>(member.SourceTerminal));
  }
  for (const auto &way : facts.Ways) { AppendWay(hash, way); }
  for (const auto &missing : facts.MissingWays) {
    Append(hash, missing.Omission);
    AppendWay(hash, missing.Way);
    Append(hash, missing.Layer);
    Append(hash, static_cast<uint8_t>(missing.Bridge));
    Append(hash, static_cast<uint8_t>(missing.Tunnel));
    Append(hash, static_cast<uint64_t>(missing.Points.size()));
    for (const auto &point : missing.Points) {
      Append(hash, point.LongitudeDeg);
      Append(hash, point.LatitudeDeg);
    }
    Append(hash, static_cast<uint8_t>(missing.OriginalWayState));
    Append(hash, static_cast<uint64_t>(missing.OriginalPoints.size()));
    for (const auto &point : missing.OriginalPoints) {
      Append(hash, static_cast<uint8_t>(point.State));
      Append(hash, point.Group);
    }
  }
}
}

std::array<uint8_t, 32> SourceFactDigest(const MvtTransportSourceFacts &facts) {
  Sha256Stream hash;
  WriteFacts(hash, facts);
  if (const auto digest = hash.Finish()) {
    return *digest;
  }
  Sha256Prefix fallback(nullptr, 0);
  WriteFacts(fallback, facts);
  return fallback.Digest(nullptr, 0);
}

void BindSourceFacts(MvtTransportSourceFacts &facts) { facts.Digest = SourceFactDigest(facts); }

std::array<uint8_t, 32> GroundInputOmissionDigest(std::span<const MvtTransportOmission> omissions) {
  constexpr std::string_view kVersion = "outshine/mvt-ground-omissions/1";
  Sha256Prefix hash(kVersion.data(), kVersion.size());
  Append(hash, static_cast<uint64_t>(omissions.size()));
  for (const auto &omission : omissions) {
    Append(hash, omission.Source);
    hash.Append(omission.Feature.data(), omission.Feature.size());
    Append(hash, static_cast<uint8_t>(omission.Reason));
  }
  return hash.Digest(nullptr, 0);
}
}
