#ifndef OUTSHINE_GENERATORS_ROAD_ROADSURFACEBUILDER_H
#define OUTSHINE_GENERATORS_ROAD_ROADSURFACEBUILDER_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

#include "RoadAlignment.h"
#include "GroundMesher.h"
#include "TangentFrame.h"
#include "scene/Geometry.h"

namespace outshine::Generators {

struct RoadSurfaceSpan {
  World::TransportEdgeId SourceEdge;
  double StartStationM = 0.0;
  double EndStationM = 0.0;
  int Part = -1;
  uint32_t FirstTriangle = 0;
};

struct RoadSurface {
  Data::OsmSourceIdentity SourceIdentity;
  uint64_t TerrainDigest = 0;
  LongitudeLatitude AlignmentAnchor;
  LongitudeLatitude RenderAnchor;
  Geometry SurfaceGeometry;
  std::vector<RoadSurfaceSpan> Spans;
  std::vector<EarthworkStamp> Earthworks;

  [[nodiscard]] size_t OwnedHeapBytes() const noexcept {
    size_t bytes = SurfaceGeometry.storageBytes() + Spans.capacity() * sizeof(RoadSurfaceSpan) +
                   Earthworks.capacity() * sizeof(EarthworkStamp);
    for (const EarthworkStamp &stamp : Earthworks) { bytes += stamp.HeapBytes(); }
    return bytes;
  }
};

enum class RoadSurfaceErrorCode : uint8_t {
  InvalidOptions,
  InvalidAlignment,
  UnsupportedSurface,
  SampleBudgetExceeded,
  MissingPose,
  DegenerateTriangle,
  GeometryRejected
};

struct RoadSurfaceError {
  RoadSurfaceErrorCode Code = RoadSurfaceErrorCode::InvalidOptions;
  World::TransportEdgeId SourceEdge;
  double StationM = 0.0;
};

struct RoadSurfaceBuildOptions {
  double MaximumStationStepM = 2.0;
  static constexpr double kDefaultSurfaceLiftM = 0.06;
  double SurfaceLiftM = kDefaultSurfaceLiftM;
  size_t MaximumSegments = 32768;
};

class RoadSurfaceBuilder {
public:
  [[nodiscard]] static std::expected<RoadSurface, RoadSurfaceError>
  Build(const RoadAlignment &alignment,
        const TangentFrame &renderFrame,
        RoadSurfaceBuildOptions options = {});
};

}

#endif
