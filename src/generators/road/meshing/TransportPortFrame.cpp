#include "TransportPortFrame.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <string_view>

namespace outshine::Generators {
std::expected<RoadPort, std::string_view>
TransportPortInFrame(const TransportPort &port, const TangentFrame &frame, EastNorth interior) {
  const auto source = TangentFrame::At(port.Anchor);
  const auto &gate = port.Contact.Gate;
  const auto &plane = port.Contact.Plane;
  if (gate.EastM != 0 || gate.NorthM != 0) {
    return std::unexpected("transport boundary contact is not in its canonical local frame");
  }
  const auto point = frame.ToLocalPosition(source.ToEcefPosition({.UpM = gate.GradeM}));
  const auto normal = frame.ToLocalDirection(
      source.ToEcefDirection({.EastM = -plane.SlopeE, .NorthM = -plane.SlopeN, .UpM = 1}));
  const auto across = frame.ToLocalDirection(
      source.ToEcefDirection({.EastM = -gate.OutN,
                              .NorthM = gate.OutE,
                              .UpM = -plane.SlopeE * gate.OutN + plane.SlopeN * gate.OutE}));
  const double scale = std::hypot(across.EastM, across.NorthM);
  const double direction = (interior.EastM - point.EastM) * across.NorthM -
                           (interior.NorthM - point.NorthM) * across.EastM;
  if (!(scale > 0) || !(normal.UpM > 0) || !std::isfinite(scale) || !std::isfinite(direction) ||
      direction == 0 || !std::isfinite(normal.EastM) || !std::isfinite(normal.NorthM) ||
      !std::isfinite(normal.UpM)) {
    return std::unexpected("transport boundary contact is not representable in the target frame");
  }
  const double sign = direction > 0 ? 1 : -1;
  const RoadPort result = {
      .Gate = {.EastM = point.EastM,
               .NorthM = point.NorthM,
               .GradeM = point.UpM,
               .OutE = sign * across.NorthM / scale,
               .OutN = -sign * across.EastM / scale,
               .HalfWidthM = gate.HalfWidthM * scale,
               .ShoulderM = gate.ShoulderM * scale,
               .ThicknessM = gate.ThicknessM},
      .Plane = {.SlopeE = -normal.EastM / normal.UpM, .SlopeN = -normal.NorthM / normal.UpM}};
  if (!std::ranges::all_of(std::array{result.Gate.EastM,
                                      result.Gate.NorthM,
                                      result.Gate.GradeM,
                                      result.Gate.HalfWidthM,
                                      result.Gate.ShoulderM,
                                      result.Gate.ThicknessM,
                                      result.Plane.SlopeE,
                                      result.Plane.SlopeN},
                           [](double value) { return std::isfinite(value); })) {
    return std::unexpected("transport boundary transformation is not finite");
  }
  return result;
}
}
