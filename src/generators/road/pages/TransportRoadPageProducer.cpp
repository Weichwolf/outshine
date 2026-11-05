#include "TransportRoadPageProducer.h"
#include "ProfiledRoadMesher.h"
#include "RoadPageAppearance.h"
#include "TransportPlanCodec.h"
#include "TransportRoadPageMeshing.h"
#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace outshine::Generators {
std::expected<TransportRoadPageProducer, std::string>
TransportRoadPageProducer::Bind(TransportPlanLease plan,
                                const RoadPageAddress &address,
                                std::vector<RoadPagePartAppearance> parts) {
  const auto *appearance = std::get_if<RoadPageAppearanceV2>(&address.Appearance);
  if (!plan || plan->Coverage != TransportCoverage::Complete || appearance == nullptr ||
      address.Level != 0 || address.MaterialIndex != 0 ||
      address.Anchor.LongitudeDeg != plan->Anchor.LongitudeDeg ||
      address.Anchor.LatitudeDeg != plan->Anchor.LatitudeDeg ||
      !std::ranges::equal(address.Parts, parts, {}, {}, &RoadPagePartAppearance::Part) ||
      appearance->Key != RoadPageAppearanceKey(parts)) {
    return std::unexpected("road page bindings do not match the complete transport plan");
  }
  const auto planKey = TransportPlanContentKey(plan->Definition, plan->InputBinding);
  if (!planKey || *planKey != address.PlanKey) {
    return std::unexpected("road page functional plan key does not match its lease");
  }
  const auto key = RoadPageKey(address);
  if (key.empty()) { return std::unexpected("invalid transport road page address"); }
  return TransportRoadPageProducer(std::move(plan), key, std::move(parts));
}

TransportRoadPageProducer::TransportRoadPageProducer(TransportPlanLease plan,
                                                     std::string key,
                                                     std::vector<RoadPagePartAppearance> parts)
    : Plan_(std::move(plan)),
      ProductKey_(std::move(key)),
      Parts_(std::move(parts)),
      InputBytes_(sizeof(*this) + ProductKey_.capacity() +
                  Parts_.capacity() * sizeof(RoadPagePartAppearance) +
                  TransportPlanCapacityBytes(*Plan_)) {}

std::expected<RoadPageGeometry, std::string>
TransportRoadPageProducer::Produce(const RoadPageRequest &request, std::stop_token stop) const {
  if (RoadPageKey(request.Address) != ProductKey_) {
    return std::unexpected("transport road page producer does not own the requested product");
  }
  std::optional<RoadDisplayRequest> display;
  const auto &error = request.Address.UpperModelErrorM;
  if (error) {
    const auto &limits = request.SweepLimits;
    if (!limits) {
      return std::unexpected("Upper display generation needs an explicit sampling budget");
    }
    for (const auto &part : Parts_) {
      if (part.Part.Kind != RoadPagePartKind::Interval) { continue; }
      if (part.Part.Index >= Plan_->Paths.size()) {
        return std::unexpected("unknown Upper display path");
      }
      const auto &path = Plan_->Paths[part.Part.Index];
      if (request.Address.DisplayPolicy == RoadDisplayPolicy::UpperOnly &&
          (path.Bridge || path.Body)) {
        return std::unexpected("Upper display generation cannot replace closed or step bodies");
      }
    }
    display = RoadDisplayRequest{
        .ErrorM = *error, .Limits = *limits, .Policy = request.Address.DisplayPolicy};
  }
  const ProfiledRoadMesher mesher = display ? ProfiledRoadMesher(*display) : ProfiledRoadMesher{};
  auto product = MeshTransportRoadPage(mesher, *Plan_, Parts_, stop);
  if (!product) { return std::unexpected(std::string(product.error())); }
  if (display && (product->Swept.Refused != 0 || !product->Diagnostics.empty())) {
    return std::unexpected("Upper display generation cannot publish incomplete source coverage");
  }
  return std::move(*product);
}
}
