#include "MvtTransportSnapshotInternal.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include <cstdint>
#include <string_view>

namespace outshine::Generators::Osm::MvtTransport {
namespace {
template <class T> void Append(Sha256Prefix &hash, T value) {
  const auto word = LittleEndian(value);
  hash.Append(&word, sizeof(word));
}

void AppendWay(Sha256Prefix &hash, const MvtTransportWayFacts &way) {
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
}

void BindSourceFacts(MvtTransportSourceFacts &facts) {
  constexpr std::string_view kVersion = "outshine/mvt-source-facts/3";
  Sha256Prefix hash(kVersion.data(), kVersion.size());
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
  }
  facts.Digest = hash.Digest(nullptr, 0);
}
}
