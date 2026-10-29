#include "GroundPortPlanAssembly.h"
#include "TransportNetworkSnapshot.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::Generators::GroundPlan {
namespace {
TransportSourceEnd MakeEnd(const GroundPortField &field,
                           const GroundPortFieldScopeInfo &scope,
                           const TransportSourceEndTopology &topology,
                           const Part &part,
                           size_t side,
                           GroundPortBodyRecipe recipe) {
  const auto &identity = field.Source().Source->SourceIdentity();
  const auto physical = static_cast<uint8_t>(topology.Ground.End ^
                                             static_cast<uint8_t>(topology.Ground.Edge.Direction));
  TransportSourceEnd end;
  end.Input = {.DatasetId = identity.DatasetId,
               .Version = "ground-source-end/1",
               .Revision = identity.Revision,
               .ItemId = std::format("native/{}/{}/{}",
                                     topology.Ground.Edge.PathId,
                                     topology.Ground.Edge.SegmentOrdinal,
                                     physical),
               .Content = scope.ContentBinding};
  end.SourceId = part.Path.SourceId;
  end.ScopeBinding = scope.ContentBinding;
  end.Inputs = part.Input.Content;
  end.Topology = topology;
  end.Anchor = part.Ends[side].Geographic;
  end.Contact = part.Samples[side].Port;
  end.Contact.Gate.ThicknessM = recipe.ThicknessM;
  end.Profile = part.Path.Profile;
  end.CrossfallRad = part.Samples[side].CrossfallRad;
  end.Layer = part.Path.Layer;
  end.Use = part.Path.Use;
  if (physical == 0) {
    end.Contact.Gate.OutE = -end.Contact.Gate.OutE;
    end.Contact.Gate.OutN = -end.Contact.Gate.OutN;
    end.CrossfallRad = -end.CrossfallRad;
  }
  end.Input.Content = TransportSourceEndContentBinding(end);
  return end;
}
}

Checked<void> AppendSourceEnds(TransportPlan &plan,
                               const GroundPortField &field,
                               std::vector<Part> &parts,
                               GroundPortBodyRecipe recipe,
                               GroundPortPlanBudget budget) {
  const auto &scope = field.Scope();
  if (!scope) { return {}; }
  const auto &topologies = scope->Ends;
  if (parts.size() > budget.Paths) {
    return std::unexpected(GroundPortPlanInputError::CapacityExceeded);
  }
  plan.SourceEnds.reserve(parts.size() * 2);
  for (auto &part : parts) {
    for (size_t side = 0; side < 2; ++side) {
      if (part.Ends[side].Fraction != static_cast<double>(side)) { continue; }
      const TransportSourceMember key{.Edge = part.Source, .End = static_cast<uint8_t>(side)};
      const auto wanted = TransportSourceMemberKey(key);
      const auto found = std::ranges::lower_bound(topologies, wanted, {}, [](const auto &end) {
        return TransportSourceMemberKey(end.Ground);
      });
      if (found == topologies.end() || TransportSourceMemberKey(found->Ground) != wanted) {
        return std::unexpected(GroundPortSourceError::InvalidSource);
      }
      const TransportSourceEndReference reference{
          .End = static_cast<uint32_t>(plan.SourceEnds.size())};
      plan.SourceEnds.push_back(MakeEnd(field, *scope, *found, part, side, recipe));
      part.Path.Ends[side] = reference;
    }
  }
  return {};
}
}
