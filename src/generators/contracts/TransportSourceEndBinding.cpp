#include "TransportPlanCodec.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include <array>
#include <cstdint>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace outshine::Generators {
namespace {
template <class T> void Word(Sha256Prefix &hash, T value) {
  if constexpr (std::is_floating_point_v<T>) {
    if (value == 0) { value = 0; }
  }
  const auto bytes = LittleEndian(value);
  hash.Append(&bytes, sizeof(bytes));
}

void Text(Sha256Prefix &hash, std::string_view value) {
  Word(hash, static_cast<uint64_t>(value.size()));
  hash.Append(value.data(), value.size());
}

void Member(Sha256Prefix &hash, const TransportSourceMember &member) {
  const auto [path, segment, physical] = TransportSourceMemberKey(member);
  Word(hash, path);
  Word(hash, segment);
  Word(hash, physical);
  Word(hash, member.Node);
  Word(hash, member.SourceStep);
  Word(hash, member.PointCount);
  Word(hash, member.Layer);
  for (const bool flag : {member.Bridge, member.Tunnel, member.Terminal, member.OwnsEnd}) {
    Word(hash, static_cast<uint8_t>(flag));
  }
}
}

std::array<uint8_t, 32> TransportSourceEndContentBinding(const TransportSourceEnd &end) {
  constexpr std::string_view version = "outshine/transport-source-end/1";
  Sha256Prefix hash(version.data(), version.size());
  Text(hash, end.Input.DatasetId);
  Text(hash, end.Input.Version);
  Text(hash, end.Input.Revision);
  Text(hash, end.Input.ItemId);
  Text(hash, end.SourceId);
  hash.Append(end.ScopeBinding.data(), end.ScopeBinding.size());
  hash.Append(end.Inputs.data(), end.Inputs.size());
  Word(hash, static_cast<uint8_t>(end.Topology.Kind));
  Member(hash, end.Topology.Ground);
  Word(hash, static_cast<uint64_t>(end.Topology.Relations.size()));
  for (const auto &member : end.Topology.Relations) { Member(hash, member); }
  const auto &gate = end.Contact.Gate;
  for (const double value : {end.Anchor.LongitudeDeg,
                             end.Anchor.LatitudeDeg,
                             gate.EastM,
                             gate.NorthM,
                             gate.GradeM,
                             gate.OutE,
                             gate.OutN,
                             gate.HalfWidthM,
                             gate.ShoulderM,
                             gate.ThicknessM,
                             end.Contact.Plane.SlopeE,
                             end.Contact.Plane.SlopeN,
                             end.CrossfallRad}) {
    Word(hash, value);
  }
  Word(hash, static_cast<uint8_t>(end.Profile));
  Word(hash, end.Layer);
  Word(hash, static_cast<uint8_t>(end.Use));
  return hash.Digest(nullptr, 0);
}
}
