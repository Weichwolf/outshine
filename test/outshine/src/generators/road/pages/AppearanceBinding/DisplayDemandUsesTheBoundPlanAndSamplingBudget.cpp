#include "TransportRoadPageProducer.h"
#include "TransportPlanCodec.h"
#include "TransportPlanFixture.h"
#include "Check.h"
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  auto fixture = TransportPlanFixture();
  CHECK(fixture.has_value(), "the complete native plan fixture is available");
  if (!fixture) { return Report(); }
  const auto plan = std::make_shared<TransportPlan>(std::move(*fixture));
  const auto key = TransportPlanContentKey(plan->Definition, plan->InputBinding);
  CHECK(key.has_value(), "the immutable functional plan has a stable content key");
  if (!key) { return Report(); }
  const std::vector<RoadPagePartAppearance> parts{
      {.Part = {.Kind = RoadPagePartKind::Interval, .Index = 1},
       .WearsLinear = {{0.2f, 0.3f, 0.4f}}}};
  RoadPageRequest request{
      .Address = {.PlanKey = *key,
                  .MaterialKey = std::string(64, 'b'),
                  .MaterialIndex = 0,
                  .Anchor = plan->Anchor,
                  .Level = 0,
                  .Parts = {parts[0].Part},
                  .Appearance = RoadPageAppearanceV2{RoadPageAppearanceKey(parts)},
                  .UpperModelErrorM = 1},
      .Parent = {},
      .RequestKey = {},
      .NativeBytesMost = size_t{1024} * 1024};
  const auto producer = TransportRoadPageProducer::Bind(plan, request.Address, parts);
  CHECK(producer.has_value(), "a display address binds the same complete functional plan");
  if (!producer) { return Report(); }
  CHECK(!producer->Produce(request, {}), "display misses require explicit work admission");
  request.SweepLimits = {.StationsMost = 8, .PoseQueriesMost = 256};
  const auto display = producer->Produce(request, {});
  CHECK(display && display->Swept.Refused == 0 && display->Diagnostics.empty() &&
            !display->Mesh.Index.empty(),
        "the bound plan produces complete display geometry through the native mesher");
  request.SweepLimits = {.StationsMost = 1, .PoseQueriesMost = 256};
  CHECK(!producer->Produce(request, {}), "budget exhaustion returns no partial native page");
  const auto closed = std::make_shared<TransportPlan>(*plan);
  closed->Paths[1].Bridge = true;
  const auto bridge = TransportRoadPageProducer::Bind(closed, request.Address, parts);
  CHECK(bridge.has_value(), "the bridge variant retains the fixture's source binding");
  request.SweepLimits = {.StationsMost = 8, .PoseQueriesMost = 256};
  CHECK(bridge && !bridge->Produce(request, {}),
        "an Upper-only product cannot silently substitute fine closed bridge geometry");
  request.Address.UpperModelErrorM = 2;
  CHECK(!producer->Produce(request, {}), "another error demand belongs to another product");
  return Report();
}
