#ifndef OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADPORTFRAME_H
#define OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADPORTFRAME_H

#include "RoadPortFieldQuery.h"
#include "TangentFrame.h"
#include <array>
#include <cstddef>
#include <expected>
#include <stop_token>

namespace outshine::Generators {

struct RoadPortFrameRequest {
  LongitudeLatitude SourceAnchor;
  LongitudeLatitude PortAnchor;
  std::array<LongitudeLatitude, 2> OriginalSegment;
  double HalfWidthM = 0;
  double InitialModelHeightM = 0;
  double MaximumLongitudinalGrade = 0;
  double MaximumCrossGrade = kCrossfall;
};

struct RoadPortFrameSample {
  RoadPort Port;
  double LongitudinalGrade = 0;
  double CrossfallRad = 0;
  size_t RayIterations = 0;
  double RayResidualM = 0;
};

[[nodiscard]] std::expected<RoadPortFrameSample, RoadPortQueryError>
SampleRoadPortFrame(RoadPortFieldQuery &query,
                    const RoadPortFrameRequest &request,
                    const std::stop_token &stop = {}) noexcept;

// The caller supplies the centre of an admitted source cross-section, in SourceAnchor's frame.
[[nodiscard]] std::expected<RoadPortFrameSample, RoadPortQueryError>
SampleRoadPortSectionFrame(RoadPortFieldQuery &query,
                           const RoadPortFrameRequest &request,
                           EastNorth centre,
                           const std::stop_token &stop = {}) noexcept;

[[nodiscard]] std::expected<LongitudeLatitude, RoadPortQueryError>
RoadPortGroundPosition(const TangentFrame &source, EastNorth at) noexcept;

}
#endif
