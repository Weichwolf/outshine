#include "TransportPlanFormat.h"
#include <span>
#include <cstddef>
#include <cstdint>

namespace outshine::Generators::TransportFormat {
namespace {
constexpr size_t kMemberBytes = sizeof(uint64_t) * 2 + sizeof(uint32_t) * 5 + sizeof(uint8_t) * 5;

template <class Archive, class Member> bool MemberFields(Archive &archive, Member &member) {
  return archive(member.Edge.PathId,
                 member.Edge.SegmentOrdinal,
                 member.Edge.Direction,
                 member.Node,
                 member.SourceStep,
                 member.PointCount,
                 member.End,
                 member.Layer,
                 member.Bridge,
                 member.Tunnel,
                 member.Terminal,
                 member.OwnsEnd);
}

template <class Archive, class End> bool ProfileFields(Archive &archive, End &end) {
  return archive(end.Contact.Plane.SlopeE,
                 end.Contact.Plane.SlopeN,
                 end.Profile,
                 end.CrossfallRad,
                 end.Layer,
                 end.Use);
}
}

bool SourceEnd(BinaryValueWriter &out, const TransportSourceEnd &end) {
  return Input(out, end.Input) && out.Text(end.SourceId) &&
         out(end.ScopeBinding, end.Inputs, end.Topology.Kind) &&
         MemberFields(out, end.Topology.Ground) &&
         List(out,
              std::span(end.Topology.Relations),
              [&](const auto &member) { return MemberFields(out, member); }) &&
         out(end.Anchor.LongitudeDeg, end.Anchor.LatitudeDeg) && Gate(out, end.Contact.Gate) &&
         ProfileFields(out, end);
}

bool SourceEnd(Reader &in, TransportSourceEnd &end) {
  return Input(in, end.Input) && in.Text(end.SourceId) &&
         in(end.ScopeBinding, end.Inputs, end.Topology.Kind) &&
         MemberFields(in, end.Topology.Ground) &&
         in.List(end.Topology.Relations,
                 kMemberBytes,
                 [&](auto &member) { return MemberFields(in, member); }) &&
         in(end.Anchor.LongitudeDeg, end.Anchor.LatitudeDeg) && Gate(in, end.Contact.Gate) &&
         ProfileFields(in, end);
}
}
