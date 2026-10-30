#include "TransportPlanFixture.h"
#include "Check.h"
#include <array>
#include <cstdint>
#include <variant>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto source = TransportPlanFixture();
  CHECK(source && ValidateTransportPlan(*source), "the native fixture is a qualified data product");
  if (!source) { return Report(); }
  const auto encoded = EncodeTransportPlan(*source, kTransportFixtureLimits);
  CHECK(encoded.has_value(), "profiles, inputs, junctions, body and contacts encode together");
  if (!encoded) { return Report(); }
  const auto decoded = DecodeTransportPlan(*encoded, kTransportFixtureLimits);
  CHECK(decoded.has_value(), "the independent decoder accepts the bounded native product");
  if (!decoded) { return Report(); }
  CHECK(EncodeTransportPlan(*decoded, kTransportFixtureLimits) == encoded,
        "every persisted native field reencodes to identical bytes");
  CHECK(decoded->InputBinding == source->InputBinding && decoded->Inputs.size() == 1 &&
            decoded->Inputs[0].Content == source->Inputs[0].Content,
        "the actual consumed source manifest and its binding survive decoding");
  CHECK(decoded->TerrainContacts == source->TerrainContacts &&
            decoded->TerrainContacts[0].Profile && decoded->TerrainContacts[0].PlanarSupportM,
        "holes, seams, analytic profiles and support values remain exact");
  CHECK(decoded->Bodies.size() == 1 && decoded->Bodies[0].Points == source->Bodies[0].Points &&
            decoded->Bodies[0].Faces == source->Bodies[0].Faces &&
            decoded->Bodies[0].WallParents == source->Bodies[0].WallParents &&
            decoded->Bodies[0].WallKinds == source->Bodies[0].WallKinds,
        "the actual native closed body retains all indexed shape data");
  CHECK(source->Bodies[0].Work.NodeQueries > 0 && decoded->Bodies[0].Work.NodeQueries == 0,
        "transient construction telemetry is excluded from the immutable product");
  auto interface = *source;
  interface.Bodies[0].WallKinds[0] = HeightSolidWallKind::Interface;
  const auto interfaceBytes = EncodeTransportPlan(interface, kTransportFixtureLimits);
  CHECK(interfaceBytes.has_value(),
        "explicit internal closures preserve the existing native body wire layout");
  if (interfaceBytes) {
    const auto restored = DecodeTransportPlan(*interfaceBytes, kTransportFixtureLimits);
    CHECK(restored && restored->Bodies[0].WallKinds[0] == HeightSolidWallKind::Interface &&
              !HeightSolidExposesFace(restored->Bodies[0], 2 * restored->Bodies[0].SurfaceFaces),
          "stored functional bodies retain the distinction between outer surfaces and internal "
          "closures");
  }
  interface.Bodies[0].WallKinds[0] = static_cast<HeightSolidWallKind>(255);
  CHECK(!EncodeTransportPlan(interface, kTransportFixtureLimits),
        "unknown wall semantics are refused rather than reinterpreted as exposed surfaces");
  const auto *gate = std::get_if<TransportJunctionGateReference>(decoded->Paths[0].Ends.data());
  const auto *port = std::get_if<TransportBoundaryPortReference>(&decoded->Paths[0].Ends[1]);
  CHECK(gate && port && gate->Junction == 0 && gate->Gate == 0 && port->Port == 0,
        "inner junction gates and outer boundary ports retain distinct exact references");
  const TransportPlanAddress first{.Definition = source->Definition,
                                   .Inputs = TransportInputScopeId(std::array<uint8_t, 32>{})};
  auto second = first;
  second.Inputs.Bytes[0] = 1;
  CHECK(TransportPlanRequestKey(first) != TransportPlanRequestKey(second),
        "independently selected logical source bindings have different request aliases");
  CHECK(TransportPlanContentKey(first.Definition, source->InputBinding) ==
            TransportPlanContentKey(second.Definition, decoded->InputBinding),
        "the same resolved inputs share their product before cooking regardless of request alias");
  auto changed = source->InputBinding;
  changed[0] ^= uint8_t{1};
  CHECK(TransportPlanContentKey(source->Definition, changed) !=
            TransportPlanContentKey(source->Definition, source->InputBinding),
        "a consumed input change invalidates the resolved product identity");
  auto partial = *source;
  partial.Coverage = TransportCoverage::Partial;
  partial.Omissions.push_back(
      {.SourceId = "fixture/road/43", .Reason = TransportOmissionReason::MissingBoundary});
  const auto partialBytes = EncodeTransportPlan(partial, kTransportFixtureLimits);
  CHECK(partialBytes.has_value(),
        "explicit partial data can be persisted without claiming coverage");
  if (partialBytes) {
    const auto restored = DecodeTransportPlan(*partialBytes, kTransportFixtureLimits);
    CHECK(restored && restored->Coverage == TransportCoverage::Partial &&
              restored->Omissions.size() == 1 &&
              restored->Omissions[0].Reason == TransportOmissionReason::MissingBoundary,
          "decoding a partial plan retains its unfulfilled boundary condition");
  }
  return Report();
}
