#include "RoadPortFrame.h"
#include "TangentFrame.h"
#include <cmath>
#include <cstddef>
#include <expected>
#include <stop_token>

namespace outshine::Generators {
namespace {
constexpr double kModelHeightMostM = 1e6;
constexpr double kRayResidualM = 1e-8;
constexpr double kGradeTolerance = 1e-9;
constexpr double kLongitudeMostDeg = 180;
constexpr double kLatitudeMostDeg = 90;
constexpr double kDirectionLeastM = 1e-9;
constexpr double kUpDerivativeLeast = .25;
constexpr size_t kRayIterations = 8;

bool Valid(LongitudeLatitude at) noexcept {
  return std::isfinite(at.LongitudeDeg) && std::isfinite(at.LatitudeDeg) &&
         std::abs(at.LongitudeDeg) <= kLongitudeMostDeg &&
         std::abs(at.LatitudeDeg) <= kLatitudeMostDeg;
}

bool Valid(const RoadPortFrameRequest &request) noexcept {
  return Valid(request.SourceAnchor) && Valid(request.PortAnchor) &&
         Valid(request.OriginalSegment[0]) && Valid(request.OriginalSegment[1]) &&
         std::isfinite(request.InitialModelHeightM) &&
         std::abs(request.InitialModelHeightM) <= kModelHeightMostM &&
         std::isfinite(request.HalfWidthM) && request.HalfWidthM > 0 &&
         request.HalfWidthM <= kModelHeightMostM &&
         std::isfinite(request.MaximumLongitudinalGrade) && request.MaximumLongitudinalGrade > 0 &&
         std::isfinite(request.MaximumCrossGrade) && request.MaximumCrossGrade > 0;
}

std::expected<RoadPortFrameSample, RoadPortQueryError> Finish(const TangentFrame &source,
                                                              const TangentFrame &target,
                                                              const RoadPortFrameRequest &request,
                                                              RoadPortField::Sample field,
                                                              RoadPortFrameSample result) noexcept {
  const auto normal = target.ToLocalDirection(source.ToEcefDirection(
      {.EastM = -field.Gradient.SlopeE, .NorthM = -field.Gradient.SlopeN, .UpM = 1}));
  const auto a = target.ToLocalGroundPosition(request.OriginalSegment[0]);
  const auto b = target.ToLocalGroundPosition(request.OriginalSegment[1]);
  const double length = std::hypot(b.EastM - a.EastM, b.NorthM - a.NorthM);
  if (!std::isfinite(length) || length <= kDirectionLeastM || !std::isfinite(normal.UpM) ||
      normal.UpM <= kUpDerivativeLeast) {
    return std::unexpected(RoadPortQueryError::UnqualifiedFrame);
  }
  auto &port = result.Port;
  port.Gate.OutE = (b.EastM - a.EastM) / length;
  port.Gate.OutN = (b.NorthM - a.NorthM) / length;
  port.Gate.HalfWidthM = request.HalfWidthM;
  port.Plane = {.SlopeE = -normal.EastM / normal.UpM, .SlopeN = -normal.NorthM / normal.UpM};
  result.LongitudinalGrade =
      port.Plane.SlopeE * port.Gate.OutE + port.Plane.SlopeN * port.Gate.OutN;
  const double crossGrade =
      -port.Plane.SlopeE * port.Gate.OutN + port.Plane.SlopeN * port.Gate.OutE;
  result.CrossfallRad = -std::atan(crossGrade);
  if (!std::isfinite(result.LongitudinalGrade) || !std::isfinite(crossGrade) ||
      std::abs(result.LongitudinalGrade) > request.MaximumLongitudinalGrade + kGradeTolerance ||
      std::abs(crossGrade) > request.MaximumCrossGrade + kGradeTolerance) {
    return std::unexpected(RoadPortQueryError::GradeExceeded);
  }
  return result;
}
}

std::expected<RoadPortFrameSample, RoadPortQueryError>
SampleRoadPortSectionFrame(RoadPortFieldQuery &query,
                           const RoadPortFrameRequest &request,
                           EastNorth centre,
                           const std::stop_token &stop) noexcept {
  if (stop.stop_requested()) { return std::unexpected(RoadPortQueryError::Canceled); }
  if (!Valid(request)) { return std::unexpected(RoadPortQueryError::InvalidInput); }
  const auto sampled = query.At(centre);
  if (!sampled) { return std::unexpected(sampled.error()); }
  const auto source = TangentFrame::At(request.SourceAnchor);
  const auto target = TangentFrame::At(request.PortAnchor);
  const auto point = target.ToLocalPosition(source.ToEcefPosition(
      {.EastM = centre.EastM, .NorthM = centre.NorthM, .UpM = sampled->HeightM}));
  if (!std::isfinite(point.EastM) || !std::isfinite(point.NorthM) || !std::isfinite(point.UpM) ||
      std::abs(point.UpM) > kModelHeightMostM) {
    return std::unexpected(RoadPortQueryError::UnqualifiedFrame);
  }
  RoadPortFrameSample result;
  result.Port.Gate.EastM = point.EastM;
  result.Port.Gate.NorthM = point.NorthM;
  result.Port.Gate.GradeM = point.UpM;
  return Finish(source, target, request, *sampled, result);
}

std::expected<RoadPortFrameSample, RoadPortQueryError>
SampleRoadPortFrame(RoadPortFieldQuery &query,
                    const RoadPortFrameRequest &request,
                    const std::stop_token &stop) noexcept {
  if (stop.stop_requested()) { return std::unexpected(RoadPortQueryError::Canceled); }
  if (!Valid(request)) { return std::unexpected(RoadPortQueryError::InvalidInput); }
  const auto source = TangentFrame::At(request.SourceAnchor);
  const auto target = TangentFrame::At(request.PortAnchor);
  const auto at = source.ToLocalPosition(target.OriginEcef());
  const auto up = source.ToLocalDirection(target.UpEcef());
  RoadPortFrameSample result;
  result.Port.Gate.GradeM = request.InitialModelHeightM;
  for (size_t iteration = 0; iteration < kRayIterations; ++iteration) {
    if (stop.stop_requested()) { return std::unexpected(RoadPortQueryError::Canceled); }
    const double height = result.Port.Gate.GradeM;
    const auto sample =
        query.At({.EastM = at.EastM + height * up.EastM, .NorthM = at.NorthM + height * up.NorthM});
    if (!sample) { return std::unexpected(sample.error()); }
    result.RayIterations = iteration + 1;
    result.RayResidualM = at.UpM + height * up.UpM - sample->HeightM;
    const double derivative =
        up.UpM - sample->Gradient.SlopeE * up.EastM - sample->Gradient.SlopeN * up.NorthM;
    if (!std::isfinite(result.RayResidualM) || !std::isfinite(derivative) ||
        derivative <= kUpDerivativeLeast) {
      return std::unexpected(RoadPortQueryError::UnqualifiedFrame);
    }
    if (std::abs(result.RayResidualM) <= kRayResidualM) {
      return Finish(source, target, request, *sample, result);
    }
    result.Port.Gate.GradeM -= result.RayResidualM / derivative;
    if (!std::isfinite(result.Port.Gate.GradeM) ||
        std::abs(result.Port.Gate.GradeM) > kModelHeightMostM) {
      return std::unexpected(RoadPortQueryError::UnqualifiedFrame);
    }
  }
  return std::unexpected(RoadPortQueryError::NoConvergence);
}
}
