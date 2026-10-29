#include "GroundPortPlanAssembly.h"
#include "GroundPortCore.h"
#include "GroundPortPlanNetwork.h"
#include "TransportNetworkSnapshot.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <limits>
#include <numeric>
#include <span>
#include <stop_token>
#include <utility>
#include <variant>

namespace outshine::Generators {
namespace {
using namespace GroundPlan;
using InputError = GroundPortPlanInputError;

Checked<void> SetOmissions(TransportPlan &plan,
                           std::span<const TransportOmission> omissions,
                           const TransportPlanNetwork &network,
                           GroundPortPlanBudget budget) {
  if (omissions.empty()) { return std::unexpected(InputError::InvalidOmissions); }
  if (omissions.size() > budget.RecipePaths ||
      network.OmittedRoutes.size() > budget.RecipePaths - omissions.size()) {
    return std::unexpected(InputError::CapacityExceeded);
  }
  plan.Omissions.reserve(omissions.size() + network.OmittedRoutes.size());
  size_t text = budget.BindingBytes;
  for (const auto &omission : omissions) {
    if (omission.SourceId.empty() || omission.Reason > TransportOmissionReason::ResourceDeferred) {
      return std::unexpected(InputError::InvalidOmissions);
    }
    if (omission.SourceId.size() > text) { return std::unexpected(InputError::CapacityExceeded); }
    text -= omission.SourceId.size();
    plan.Omissions.push_back(omission);
  }
  for (const auto &route : network.OmittedRoutes) {
    plan.Omissions.push_back(
        {.SourceId = std::format("native-route/{}/{}", route.Id, route.SourceRouteId),
         .Reason = TransportOmissionReason::MissingBoundary});
  }
  std::ranges::sort(plan.Omissions, {}, &TransportOmission::SourceId);
  size_t kept = 0;
  for (auto &omission : plan.Omissions) {
    if (kept != 0 && plan.Omissions[kept - 1].SourceId == omission.SourceId) {
      if (plan.Omissions[kept - 1].Reason != omission.Reason) {
        return std::unexpected(InputError::InvalidOmissions);
      }
      continue;
    }
    if (&omission != &plan.Omissions[kept]) { plan.Omissions[kept] = std::move(omission); }
    ++kept;
  }
  plan.Omissions.resize(kept);
  plan.Coverage = TransportCoverage::Partial;
  return {};
}

void MoveBodies(TransportPlan &plan, GroundPortBodies &&bodies, GroundPortPlanWork &work) {
  size_t contacts = 0;
  for (const auto &body : bodies.Bodies) { contacts += body.TerrainContacts.size(); }
  plan.Bodies.reserve(bodies.Bodies.size());
  plan.TerrainContacts.reserve(contacts);
  for (auto &body : bodies.Bodies) {
    work.MovedVertices += body.Geometry.Points.size();
    work.MovedContacts += body.TerrainContacts.size();
    plan.Bodies.push_back(std::move(body.Geometry));
    for (auto &contact : body.TerrainContacts) {
      plan.TerrainContacts.push_back(std::move(contact));
    }
  }
}
}

std::expected<GroundPortPlanProduct, GroundPortPlanError>
BuildGroundPortPartialPlan(const GroundPortField &field,
                           const GroundPortBodies &wholeBodies,
                           GeoCellId core,
                           std::span<const GroundPortPlanPathRecipe> recipes,
                           std::span<const TransportOmission> omissions,
                           GroundPortPlanBudget budget,
                           const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(InputError::Canceled); }
  if (budget.Paths > std::numeric_limits<uint32_t>::max() / 2 ||
      budget.Ports > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(InputError::CapacityExceeded);
  }
  if (omissions.empty()) { return std::unexpected(InputError::InvalidOmissions); }
  const auto &graph = field.Source().Source->Topology();
  const auto selection =
      SelectGroundPortCore(field,
                           core,
                           {.SourceNodesMost = graph.Nodes().size(),
                            .SourceEdgesMost = graph.Edges().size(),
                            .SegmentsMost = budget.Paths,
                            .ContainerCapacityMost = budget.Bodies.ContainerBytes},
                           stop);
  if (!selection) { return std::unexpected(selection.error()); }
  if (selection->Segments.empty()) { return std::unexpected(InputError::EmptyCore); }
  auto sliced = SliceGroundPortBodies(field, wholeBodies, *selection, budget.Bodies, stop);
  if (!sliced) {
    return std::unexpected(
        std::visit([](auto error) -> GroundPortPlanError { return error; }, sliced.error()));
  }
  const auto bounds = core.Bounds();
  if (!bounds) { return std::unexpected(TransportCoreError::InvalidCore); }
  const LongitudeLatitude anchor{.LongitudeDeg = std::midpoint(bounds->WestDeg, bounds->EastDeg),
                                 .LatitudeDeg = std::midpoint(bounds->SouthDeg, bounds->NorthDeg)};
  auto reframed = ReframeGroundPortBodies(std::move(*sliced), anchor, budget.Bodies, stop);
  if (!reframed) {
    return std::unexpected(
        std::visit([](auto error) -> GroundPortPlanError { return error; }, reframed.error()));
  }
  GroundPortPlanProduct product{.Plan = TransportPlan({.Core = core}),
                                .BoundsEcef = GroundPortBodyBoundsEcef(*reframed),
                                .Work = {}};
  product.Plan.Anchor = anchor;
  product.Work.Core = selection->Work;
  product.Work.Bodies = reframed->Work;
  auto network = ProjectGroundPortFieldNetwork(field, budget.Network, stop);
  if (!network) { return std::unexpected(network.error()); }
  product.Plan.Network = network->Network;
  product.Work.Network = network->Work;
  if (auto set = SetOmissions(product.Plan, omissions, *network, budget); !set) {
    return std::unexpected(set.error());
  }
  auto parts = PrepareParts(
      field, wholeBodies, *reframed, *selection, recipes, product.Work.Samples, budget, stop);
  if (!parts) { return std::unexpected(parts.error()); }
  if (auto appended = AppendProfiles(product.Plan, field, *parts, wholeBodies.Recipe, budget);
      !appended) {
    return std::unexpected(appended.error());
  }
  if (auto bound = BindInputs(product.Plan, *reframed, *parts, budget); !bound) {
    return std::unexpected(bound.error());
  }
  MoveBodies(product.Plan, std::move(*reframed), product.Work);
  product.Work.Paths = product.Plan.Paths.size();
  product.Work.Ports = product.Plan.Ports.size();
  product.Work.OutputCapacityBytes = TransportPlanCapacityBytes(product.Plan);
  if (product.Work.OutputCapacityBytes > budget.OutputCapacityBytes) {
    return std::unexpected(InputError::CapacityExceeded);
  }
  if (stop.stop_requested()) { return std::unexpected(InputError::Canceled); }
  const auto valid = ValidateTransportPlan(product.Plan);
  if (!valid) { return std::unexpected(valid.error()); }
  return product;
}
}
