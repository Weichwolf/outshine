#include "GroundPortPlanAssembly.h"
#include "TransportNetworkSnapshot.h"
#include "TangentFrame.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <span>
#include <stop_token>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace outshine::Generators::GroundPlan {
namespace {
using InputError = GroundPortPlanInputError;

struct Endpoint {
  uint32_t Edge = 0;
  TransportCoreEnd End;
};

auto Key(const Endpoint &endpoint) {
  return std::tuple{endpoint.Edge, endpoint.End.Fraction};
}

bool Allows(const World::TransportEdge &edge, TransportUse use) {
  using World::TransportMode;
  if (use == TransportUse::Road) {
    return (edge.Modes & static_cast<uint8_t>(TransportMode::Motor)) != 0;
  }
  if (use == TransportUse::Rail) {
    return (edge.Modes & static_cast<uint8_t>(TransportMode::Rail)) != 0;
  }
  return use == TransportUse::Path &&
         (edge.Modes & (static_cast<uint8_t>(TransportMode::Walk) |
                        static_cast<uint8_t>(TransportMode::Cycle))) != 0;
}

Checked<Part> MakePart(const GroundPortField &field,
                       const GroundPortBodies &whole,
                       const GroundPortBody &body,
                       std::span<const Endpoint> endpoints,
                       std::span<const GroundPortPlanPathRecipe> recipes,
                       size_t index,
                       GroundPortPlanBudget budget) {
  const auto &graph = field.Source().Source->Topology();
  const auto *edge = graph.FindEdge(body.Source);
  if (edge == nullptr) { return std::unexpected(GroundPortSourceError::InvalidSource); }
  const auto recipe =
      std::ranges::lower_bound(recipes, edge->Id.PathId, {}, &GroundPortPlanPathRecipe::PathId);
  if (recipe == recipes.end() || recipe->PathId != edge->Id.PathId || recipe->Material.empty() ||
      recipe->Material.size() > budget.BindingBytes || !std::isfinite(recipe->SpeedMps) ||
      recipe->SpeedMps < 0 || !Allows(*edge, recipe->Use)) {
    return std::unexpected(InputError::InvalidRecipe);
  }
  Part result;
  result.Body = index;
  result.SourceNodes = {edge->FromNodeId, edge->ToNodeId};
  const auto sourceIndex = static_cast<uint32_t>(edge - graph.Edges().data());
  const std::array fractions{body.SourceBegin, body.SourceEnd};
  for (size_t end = 0; end < 2; ++end) {
    const auto found =
        std::ranges::lower_bound(endpoints, std::tuple{sourceIndex, fractions[end]}, {}, Key);
    if (found == endpoints.end() || Key(*found) != std::tuple{sourceIndex, fractions[end]}) {
      return std::unexpected(GroundPortSourceError::InvalidSource);
    }
    result.Ends[end] = found->End;
  }
  auto &path = result.Path;
  path.SourceId = std::format("native/{}/{}", edge->Id.PathId, edge->Id.SegmentOrdinal);
  path.PartId = std::format("{}/{:a}-{:a}", path.SourceId, body.SourceBegin, body.SourceEnd);
  path.Material = recipe->Material;
  path.HalfWidthM = field.Source().WidthM / 2;
  path.MaximumGradient = field.Source().MaximumGradient;
  path.SpeedMps = recipe->SpeedMps;
  path.Profile = RoadProfile::Simple;
  path.Use = recipe->Use;
  path.Layer = edge->Layer;
  path.Lanes = edge->LaneCount;
  path.Priority = recipe->Priority;
  auto opposite = edge->Id;
  opposite.Direction = opposite.Direction == World::EdgeDirection::Forward
                           ? World::EdgeDirection::Reverse
                           : World::EdgeDirection::Forward;
  path.Oneway = graph.FindEdge(opposite) == nullptr;
  path.Bridge = edge->Bridge;
  path.Body = static_cast<uint32_t>(index);
  result.Input = PathInput(whole, *recipe, path.SourceId);
  return result;
}

struct PendingPort {
  TransportPort Port;
  size_t Part = 0;
  size_t End = 0;
  bool Primary = false;
};

auto PortKey(const PendingPort &pending) {
  const auto &port = pending.Port;
  return std::tuple{port.Boundary.Level,
                    port.Boundary.Axis,
                    port.Boundary.Line,
                    port.Boundary.Segment,
                    port.SourceId,
                    port.Anchor.LongitudeDeg,
                    port.Anchor.LatitudeDeg};
}
}

Checked<std::vector<Part>> PrepareParts(const GroundPortField &field,
                                        const GroundPortBodies &whole,
                                        const GroundPortBodies &sliced,
                                        const TransportCoreSelection &selection,
                                        std::span<const GroundPortPlanPathRecipe> recipes,
                                        GroundPortFieldQueryWork &sampleWork,
                                        GroundPortPlanBudget budget,
                                        const std::stop_token &stop) {
  if (sliced.Bodies.size() > budget.Paths || sliced.Bodies.size() > budget.Samples.Ports / 2 ||
      recipes.size() > budget.RecipePaths) {
    return std::unexpected(InputError::CapacityExceeded);
  }
  if (std::ranges::adjacent_find(recipes, [](const auto &a, const auto &b) {
        return a.PathId >= b.PathId;
      }) != recipes.end()) {
    return std::unexpected(InputError::InvalidRecipe);
  }
  std::vector<Endpoint> endpoints;
  endpoints.reserve(selection.Segments.size() * 2);
  for (const auto &segment : selection.Segments) {
    for (const auto &end : segment.Ends) {
      endpoints.push_back({.Edge = segment.SourceEdge, .End = end});
    }
  }
  std::ranges::sort(endpoints, {}, Key);
  std::vector<Part> parts;
  parts.reserve(sliced.Bodies.size());
  size_t text = budget.BindingBytes;
  for (size_t index = 0; index < sliced.Bodies.size(); ++index) {
    if (stop.stop_requested()) { return std::unexpected(InputError::Canceled); }
    auto remaining = budget;
    remaining.BindingBytes = text;
    auto part = MakePart(field, whole, sliced.Bodies[index], endpoints, recipes, index, remaining);
    if (!part) { return std::unexpected(part.error()); }
    const size_t used = part->Path.Material.size() + part->Path.PartId.size() +
                        part->Path.SourceId.size() + part->Input.ItemId.size();
    if (used > text) { return std::unexpected(InputError::CapacityExceeded); }
    text -= used;
    parts.push_back(std::move(*part));
  }
  std::ranges::sort(parts, {}, [](const Part &part) { return part.Path.PartId; });
  std::vector<GroundPortFieldRequestPoint> requests;
  requests.reserve(parts.size() * 2);
  for (const auto &part : parts) {
    for (const auto &end : part.Ends) {
      requests.push_back({.Edge = sliced.Bodies[part.Body].Source, .Anchor = end.Geographic});
    }
  }
  auto sampled = field.Sample(requests, budget.Samples, stop);
  if (!sampled) {
    return std::unexpected(
        std::visit([](auto error) -> GroundPortPlanError { return error; }, sampled.error()));
  }
  sampleWork = sampled->Work;
  for (size_t part = 0; part < parts.size(); ++part) {
    for (size_t end = 0; end < 2; ++end) {
      parts[part].Samples[end] = sampled->Ports[2 * part + end];
    }
    parts[part].Path.CrossfallRad = parts[part].Samples[0].CrossfallRad;
  }
  return parts;
}

Checked<void> AppendProfiles(TransportPlan &plan,
                             std::vector<Part> &parts,
                             GroundPortBodyRecipe recipe,
                             GroundPortPlanBudget budget) {
  std::vector<PendingPort> ports;
  size_t count = 0;
  for (const auto &part : parts) {
    for (const auto &end : part.Ends) {
      for (const auto &boundary : end.Boundaries) {
        count += static_cast<size_t>(boundary.has_value());
      }
    }
  }
  if (count > budget.Ports) { return std::unexpected(InputError::CapacityExceeded); }
  ports.reserve(count);
  const auto frame = TangentFrame::At(plan.Anchor);
  plan.Stations.reserve(parts.size() * 2);
  for (size_t at = 0; at < parts.size(); ++at) {
    auto &part = parts[at];
    part.Path.FirstStation = static_cast<uint32_t>(plan.Stations.size());
    part.Path.StationCount = 2;
    for (size_t end = 0; end < 2; ++end) {
      const auto &point = part.Ends[end];
      const auto &sample = part.Samples[end];
      const auto centre = frame.ToLocalPosition({.LongitudeDeg = point.Geographic.LongitudeDeg,
                                                 .LatitudeDeg = point.Geographic.LatitudeDeg,
                                                 .HeightM = sample.Port.Gate.GradeM});
      plan.Stations.push_back(
          {.EastM = centre.EastM,
           .NorthM = centre.NorthM,
           .GradeM = centre.UpM,
           .Node = point.Fraction == static_cast<double>(end) ? part.SourceNodes[end] : 0});
      bool primary = true;
      for (const auto &boundary : point.Boundaries) {
        if (!boundary) { continue; }
        TransportPort port{.Boundary = *boundary,
                           .SourceId = part.Path.SourceId,
                           .Inputs = part.Input.Content,
                           .Anchor = point.Geographic,
                           .Contact = sample.Port,
                           .Profile = part.Path.Profile,
                           .CrossfallRad = sample.CrossfallRad,
                           .Layer = part.Path.Layer,
                           .Use = part.Path.Use};
        port.Contact.Gate.ThicknessM = recipe.ThicknessM;
        ports.push_back({.Port = std::move(port), .Part = at, .End = end, .Primary = primary});
        primary = false;
      }
    }
  }
  std::ranges::sort(ports, {}, PortKey);
  plan.Ports.reserve(ports.size());
  for (auto &pending : ports) {
    if (pending.Primary) {
      parts[pending.Part].Path.Ends[pending.End] =
          TransportBoundaryPortReference{.Port = static_cast<uint32_t>(plan.Ports.size())};
    }
    plan.Ports.push_back(std::move(pending.Port));
  }
  plan.Paths.reserve(parts.size());
  for (auto &part : parts) { plan.Paths.push_back(std::move(part.Path)); }
  return {};
}
}
