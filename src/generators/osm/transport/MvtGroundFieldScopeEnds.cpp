#include "MvtGroundFieldScopeInternal.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>

namespace outshine::Generators::Osm::GroundFieldScopes {
Checked<void> AppendEnds(State &state,
                         std::span<const TransportSourceMember> members,
                         std::span<const uint32_t> grounds) {
  if (grounds.empty() || grounds.size() > 2) { return std::unexpected(Error::InvalidCandidate); }
  for (const auto ground : grounds) {
    if (state.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (ground >= members.size()) { return std::unexpected(Error::InvalidBinding); }
    TransportSourceEndTopology end;
    end.Ground = members[ground];
    end.Kind = TransportSourceEndKind::External;
    if (grounds.size() == 2) {
      end.Kind = TransportSourceEndKind::SourceNode;
    } else if (members.size() == 1) {
      end.Kind = TransportSourceEndKind::Natural;
    }
    if (end.Kind == TransportSourceEndKind::Natural && !state.Facts.MissingWays.empty()) {
      end.Kind = TransportSourceEndKind::Unresolved;
    }
    end.Relations.reserve(members.size() - 1);
    for (size_t at = 0; at < members.size(); ++at) {
      if (at == ground) { continue; }
      if (state.Output.Work.OriginalMemberVisits + state.Output.Work.QualifiedMemberVisits +
              state.Output.Work.EndRelationVisits + state.Output.Work.ContentMemberVisits >=
          state.Limits.MemberVisits) {
        return std::unexpected(Error::CapacityExceeded);
      }
      ++state.Output.Work.EndRelationVisits;
      end.Relations.push_back(members[at]);
    }
    std::ranges::sort(end.Relations, {}, TransportSourceMemberKey);
    state.Output.Field.Info.Ends.push_back(std::move(end));
  }
  return {};
}
}
