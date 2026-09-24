#include "RoadSurfaceBuilder.h"
#include "RoadTerrainContact.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "Digest.h"
#include "math/RenderFrame.h"
#include "math/Vec3.h"

namespace outshine::Generators {
namespace {

constexpr size_t kSurfaceCount = 6;
constexpr double kMinimumHorizontalTangent = 0.1;
constexpr double kMinimumTriangleAreaM2 = 0.0001;
constexpr double kMinimumRoadRunSquaredM2 = 1e-6;
constexpr double kMaximumStationStepM = 20.0;
constexpr double kMaximumSurfaceLiftM = 0.2;
constexpr float kConcreteRoughness = 0.82f;
constexpr float kOtherRoadRoughness = 0.92f;
constexpr double kRoadApronM = 6.0;

uint64_t CorridorKeyOf(const RoadAlignment &alignment) {
  uint64_t digest = kDigestBasis;
  const auto foldWord = [&digest](uint64_t word) {
    for (unsigned byte = 0; byte < 8; ++byte) {
      digest = DigestFolded(digest, static_cast<uint8_t>(word >> (byte * 8u)));
    }
  };
  for (const char byte : alignment.SourceIdentity().DatasetId) {
    digest = DigestFolded(digest, static_cast<uint8_t>(byte));
  }
  digest = DigestFolded(digest, 0u);
  for (const char byte : alignment.SourceIdentity().Revision) {
    digest = DigestFolded(digest, static_cast<uint8_t>(byte));
  }
  digest = DigestFolded(digest, 0u);
  for (const RoadAlignmentEdge &edge : alignment.Edges()) {
    foldWord(edge.SourceEdge.WayId);
    foldWord(edge.SourceEdge.SegmentOrdinal);
    foldWord(static_cast<uint64_t>(edge.SourceEdge.Direction));
  }
  return digest;
}

struct SurfaceBucket {
  std::vector<float> PositionsM;
  std::vector<float> Normals;
  std::vector<uint32_t> Triangles;
};

struct SurfaceSample {
  Vec3 LeftM;
  Vec3 RightM;
  Vec3 Normal;
};

struct SurfaceVertex {
  Vec3 PositionM;
  Vec3 Normal;
};

struct RoadFrameTransform {
  const TangentFrame &Alignment;
  const TangentFrame &Render;
};

[[nodiscard]] RoadSurfaceError
Error(RoadSurfaceErrorCode code, World::TransportEdgeId edge = {}, double stationM = 0.0) {
  return {.Code = code, .SourceEdge = edge, .StationM = stationM};
}

[[nodiscard]] std::optional<size_t> SurfaceIndex(World::TransportSurface surface) {
  switch (surface) {
    case World::TransportSurface::Unknown: return 0;
    case World::TransportSurface::Asphalt: return 1;
    case World::TransportSurface::Concrete: return 2;
    case World::TransportSurface::Paved: return 3;
    case World::TransportSurface::Gravel: return 4;
    case World::TransportSurface::Earth: return 5;
    case World::TransportSurface::Rail:
    case World::TransportSurface::Water: return std::nullopt;
  }
  return std::nullopt;
}

[[nodiscard]] Material MaterialFor(size_t surface) {
  constexpr std::array<std::array<float, 3>, kSurfaceCount> kColour{{
      {0.18f, 0.18f, 0.17f},
      {0.11f, 0.11f, 0.10f},
      {0.40f, 0.39f, 0.36f},
      {0.25f, 0.24f, 0.22f},
      {0.22f, 0.20f, 0.16f},
      {0.16f, 0.11f, 0.075f},
  }};
  Material material;
  material.BaseColour = {{kColour[surface][0], kColour[surface][1], kColour[surface][2], 1.0f}};
  material.Metalness = 0.0f;
  material.Roughness = surface == 2 ? kConcreteRoughness : kOtherRoadRoughness;
  return material;
}

[[nodiscard]] Vec3 RenderPosition(const RoadFrameTransform &frames, EastNorthUp position) {
  const Vec3 ecef = frames.Alignment.OriginEcef() + frames.Alignment.EastEcef() * position.EastM +
                    frames.Alignment.NorthEcef() * position.NorthM +
                    frames.Alignment.UpEcef() * position.UpM;
  return {RenderFrame::Of(frames.Render.ToLocalPosition(ecef))};
}

[[nodiscard]] Vec3 RenderDirection(const RoadFrameTransform &frames, Vec3 direction) {
  const Vec3 ecef = frames.Alignment.EastEcef() * direction[0] +
                    frames.Alignment.NorthEcef() * direction[1] +
                    frames.Alignment.UpEcef() * direction[2];
  return {RenderFrame::Of(frames.Render.ToLocalDirection(ecef))};
}

[[nodiscard]] std::optional<SurfaceSample>
Sample(const RoadAlignmentPose &pose, const RoadFrameTransform &frames, double liftM) {
  const Vec3 tangent = pose.TangentEnu;
  const double horizontal = std::hypot(tangent[0], tangent[1]);
  if (!std::isfinite(horizontal) || horizontal < kMinimumHorizontalTangent ||
      !std::isfinite(pose.WidthM) || pose.WidthM <= 0.0) {
    return std::nullopt;
  }
  const double halfWidthM = pose.WidthM * 0.5;
  const double sideEast = -tangent[1] / horizontal * halfWidthM;
  const double sideNorth = tangent[0] / horizontal * halfWidthM;
  const EastNorthUp left{.EastM = pose.PositionM.EastM + sideEast,
                         .NorthM = pose.PositionM.NorthM + sideNorth,
                         .UpM = pose.PositionM.UpM};
  const EastNorthUp right{.EastM = pose.PositionM.EastM - sideEast,
                          .NorthM = pose.PositionM.NorthM - sideNorth,
                          .UpM = pose.PositionM.UpM};
  Vec3 normal =
      Cross(RenderDirection(frames, {{-sideEast / halfWidthM, -sideNorth / halfWidthM, 0.0}}),
            RenderDirection(frames, tangent));
  if (!Normalise(normal)) { return std::nullopt; }
  Vec3 leftM = RenderPosition(frames, left);
  Vec3 rightM = RenderPosition(frames, right);
  leftM[1] += liftM;
  rightM[1] += liftM;
  return SurfaceSample{.LeftM = leftM, .RightM = rightM, .Normal = normal};
}

[[nodiscard]] Vec3 Quantized(Vec3 position) {
  for (size_t axis = 0; axis < 3; ++axis) {
    position[axis] = static_cast<double>(static_cast<float>(position[axis]));
  }
  return position;
}

[[nodiscard]] bool ValidTriangle(const std::array<Vec3, 3> &vertices, const Vec3 &normal) {
  const Vec3 face = Cross(Quantized(vertices[1]) - Quantized(vertices[0]),
                          Quantized(vertices[2]) - Quantized(vertices[0]));
  return std::isfinite(Length(face)) && Length(face) >= 2.0 * kMinimumTriangleAreaM2 &&
         Dot(face, normal) > 0.0;
}

void AppendVertex(SurfaceBucket &bucket, const SurfaceVertex &vertex) {
  for (size_t axis = 0; axis < 3; ++axis) {
    bucket.PositionsM.push_back(static_cast<float>(vertex.PositionM[axis]));
    bucket.Normals.push_back(static_cast<float>(vertex.Normal[axis]));
  }
}

[[nodiscard]] bool
AppendSegment(SurfaceBucket &bucket, const SurfaceSample &begin, const SurfaceSample &end) {
  if (!ValidTriangle({begin.LeftM, begin.RightM, end.LeftM}, begin.Normal) ||
      !ValidTriangle({end.LeftM, begin.RightM, end.RightM}, end.Normal)) {
    return false;
  }
  const auto first = static_cast<uint32_t>(bucket.PositionsM.size() / 3);
  AppendVertex(bucket, {.PositionM = begin.LeftM, .Normal = begin.Normal});
  AppendVertex(bucket, {.PositionM = begin.RightM, .Normal = begin.Normal});
  AppendVertex(bucket, {.PositionM = end.LeftM, .Normal = end.Normal});
  AppendVertex(bucket, {.PositionM = end.RightM, .Normal = end.Normal});
  bucket.Triangles.insert(bucket.Triangles.end(),
                          {first, first + 1u, first + 2u, first + 2u, first + 1u, first + 3u});
  return true;
}

[[nodiscard]] std::optional<EarthworkStamp> EarthworkFor(const SurfaceSample &begin,
                                                         const SurfaceSample &end,
                                                         const RoadAlignmentPose &from,
                                                         const RoadAlignmentPose &to,
                                                         const RoadFrameTransform &frames,
                                                         uint64_t corridorKey,
                                                         double liftM) {
  const Vec3 beginCenter = (begin.LeftM + begin.RightM) * 0.5;
  const Vec3 endCenter = (end.LeftM + end.RightM) * 0.5;
  const double runE = endCenter[0] - beginCenter[0];
  const double runN = beginCenter[2] - endCenter[2];
  const double runSquaredM = runE * runE + runN * runN;
  const double widthM =
      std::hypot(begin.LeftM[0] - begin.RightM[0], begin.LeftM[2] - begin.RightM[2]);
  if (runSquaredM <= kMinimumRoadRunSquaredM2 || widthM <= 0.0) { return std::nullopt; }
  const auto extend = [](Vec3 left, Vec3 right, double factor) {
    const Vec3 across = (left - right) * factor;
    return std::array<Vec3, 2>{left + across, right - across};
  };
  const auto outerBegin = extend(begin.LeftM, begin.RightM, RoadTerrainContact::VergeM / widthM);
  const double endWidthM = std::hypot(end.LeftM[0] - end.RightM[0], end.LeftM[2] - end.RightM[2]);
  const double stationLengthM = to.StationM - from.StationM;
  if (endWidthM <= 0.0 || stationLengthM <= 0.0) { return std::nullopt; }
  const Vec3 fromDerivative = RenderDirection(frames, from.TangentEnu) * stationLengthM;
  const Vec3 toDerivative = RenderDirection(frames, to.TangentEnu) * stationLengthM;
  const auto outerEnd = extend(end.LeftM, end.RightM, RoadTerrainContact::VergeM / endWidthM);
  const auto ring =
      [](const Vec3 &leftStart, const Vec3 &leftEnd, const Vec3 &rightEnd, const Vec3 &rightStart) {
        return std::vector<double>{leftStart[0],
                                   -leftStart[2],
                                   leftEnd[0],
                                   -leftEnd[2],
                                   rightEnd[0],
                                   -rightEnd[2],
                                   rightStart[0],
                                   -rightStart[2]};
      };
  EarthworkStamp stamp;
  stamp.RingEastNorthM = ring(outerBegin[0], outerEnd[0], outerEnd[1], outerBegin[1]);
  stamp.SeamEastNorthM = ring(begin.LeftM, end.LeftM, end.RightM, begin.RightM);
  stamp.LowE = stamp.HighE = stamp.RingEastNorthM[0];
  stamp.LowN = stamp.HighN = stamp.RingEastNorthM[1];
  for (size_t index = 2; index < stamp.RingEastNorthM.size(); index += 2) {
    stamp.LowE = std::min(stamp.LowE, stamp.RingEastNorthM[index]);
    stamp.HighE = std::max(stamp.HighE, stamp.RingEastNorthM[index]);
    stamp.LowN = std::min(stamp.LowN, stamp.RingEastNorthM[index + 1]);
    stamp.HighN = std::max(stamp.HighN, stamp.RingEastNorthM[index + 1]);
  }
  stamp.AtE = beginCenter[0];
  stamp.AtN = -beginCenter[2];
  stamp.PlateauM = beginCenter[1] - liftM;
  const double rise = endCenter[1] - beginCenter[1];
  stamp.SlopeE = rise * runE / runSquaredM;
  stamp.SlopeN = rise * runN / runSquaredM;
  stamp.Profile =
      ProfiledCorridorSpan{.CorridorKey = corridorKey,
                           .BeginM = {.EastM = beginCenter[0], .NorthM = -beginCenter[2]},
                           .EndM = {.EastM = endCenter[0], .NorthM = -endCenter[2]},
                           .BeginDerivativeM = {.EastM = fromDerivative[0],
                                                .NorthM = -fromDerivative[2],
                                                .UpM = fromDerivative[1]},
                           .EndDerivativeM = {.EastM = toDerivative[0],
                                              .NorthM = -toDerivative[2],
                                              .UpM = toDerivative[1]},
                           .StationLengthM = stationLengthM,
                           .BeginBedM = beginCenter[1] - liftM,
                           .EndBedM = endCenter[1] - liftM,
                           .BeginPavementHalfWidthM = widthM * 0.5,
                           .EndPavementHalfWidthM = endWidthM * 0.5,
                           .BeginHalfWidthM = widthM * 0.5 + RoadTerrainContact::VergeM,
                           .EndHalfWidthM = endWidthM * 0.5 + RoadTerrainContact::VergeM};
  stamp.ApronM = kRoadApronM;
  stamp.Fills = true;
  stamp.Kind = EarthworkKind::Corridor;
  return stamp;
}

[[nodiscard]] bool ValidOptions(const RoadSurfaceBuildOptions &options) {
  return std::isfinite(options.MaximumStationStepM) && options.MaximumStationStepM > 0.0 &&
         options.MaximumStationStepM <= kMaximumStationStepM &&
         std::isfinite(options.SurfaceLiftM) && options.SurfaceLiftM >= 0.0 &&
         options.SurfaceLiftM <= kMaximumSurfaceLiftM && options.MaximumSegments > 0 &&
         options.MaximumSegments <= std::numeric_limits<uint32_t>::max() / 4u;
}

[[nodiscard]] std::expected<void, RoadSurfaceError>
AppendEdge(const RoadAlignment &alignment,
           const RoadAlignmentEdge &edge,
           const RoadFrameTransform &frames,
           const RoadSurfaceBuildOptions &options,
           uint64_t corridorKey,
           std::array<SurfaceBucket, kSurfaceCount> &buckets,
           RoadSurface &result,
           size_t &segmentCount) {
  const auto surfaceIndex = SurfaceIndex(edge.Surface);
  if (!surfaceIndex) {
    return std::unexpected(Error(RoadSurfaceErrorCode::UnsupportedSurface, edge.SourceEdge));
  }
  const double lengthM = edge.EndStationM - edge.StartStationM;
  const double requestedSteps = std::ceil(lengthM / options.MaximumStationStepM);
  if (!std::isfinite(requestedSteps) || requestedSteps < 1.0 ||
      requestedSteps > static_cast<double>(options.MaximumSegments - segmentCount)) {
    return std::unexpected(Error(RoadSurfaceErrorCode::SampleBudgetExceeded, edge.SourceEdge));
  }
  const auto steps = static_cast<size_t>(requestedSteps);
  SurfaceBucket &bucket = buckets[*surfaceIndex];
  for (size_t step = 0; step < steps; ++step) {
    const double fromM = lengthM * static_cast<double>(step) / static_cast<double>(steps);
    const double toM = lengthM * static_cast<double>(step + 1) / static_cast<double>(steps);
    const auto from = alignment.AtEdgeStation(edge.SourceEdge, fromM);
    const auto to = alignment.AtEdgeStation(edge.SourceEdge, toM);
    if (!from || !to) {
      return std::unexpected(
          Error(RoadSurfaceErrorCode::MissingPose, edge.SourceEdge, edge.StartStationM + fromM));
    }
    const auto begin = Sample(*from, frames, options.SurfaceLiftM);
    const auto end = Sample(*to, frames, options.SurfaceLiftM);
    if (!begin || !end || !AppendSegment(bucket, *begin, *end)) {
      return std::unexpected(Error(
          RoadSurfaceErrorCode::DegenerateTriangle, edge.SourceEdge, edge.StartStationM + fromM));
    }
    auto earthwork =
        EarthworkFor(*begin, *end, *from, *to, frames, corridorKey, options.SurfaceLiftM);
    if (!earthwork) {
      return std::unexpected(Error(
          RoadSurfaceErrorCode::DegenerateTriangle, edge.SourceEdge, edge.StartStationM + fromM));
    }
    result.Earthworks.push_back(std::move(*earthwork));
    result.Spans.push_back(
        {.SourceEdge = edge.SourceEdge,
         .StartStationM = edge.StartStationM + fromM,
         .EndStationM = edge.StartStationM + toM,
         .Part = static_cast<int>(*surfaceIndex),
         .FirstTriangle = static_cast<uint32_t>(bucket.Triangles.size() / 3u - 2u)});
    ++segmentCount;
  }
  return {};
}

[[nodiscard]] std::expected<void, RoadSurfaceError>
InstallGeometry(const std::array<SurfaceBucket, kSurfaceCount> &buckets, RoadSurface &result) {
  constexpr std::array<std::string_view, kSurfaceCount> kNames{
      "road-unknown", "road-asphalt", "road-concrete", "road-paved", "road-gravel", "road-earth"};
  std::array<int, kSurfaceCount> parts;
  parts.fill(-1);
  for (size_t surface = 0; surface < kSurfaceCount; ++surface) {
    const SurfaceBucket &bucket = buckets[surface];
    if (bucket.Triangles.empty()) { continue; }
    const auto material = result.SurfaceGeometry.addSurface(kNames[surface], MaterialFor(surface));
    if (!material) { return std::unexpected(Error(RoadSurfaceErrorCode::GeometryRejected)); }
    const auto part = result.SurfaceGeometry.addPart(kNames[surface], *material);
    if (!part || !result.SurfaceGeometry.setPositions(*part, bucket.PositionsM) ||
        !result.SurfaceGeometry.setNormals(*part, bucket.Normals) ||
        !result.SurfaceGeometry.setTriangles(*part, bucket.Triangles)) {
      return std::unexpected(Error(RoadSurfaceErrorCode::GeometryRejected));
    }
    parts[surface] = *part;
  }
  for (RoadSurfaceSpan &span : result.Spans) { span.Part = parts[static_cast<size_t>(span.Part)]; }
  if (!result.SurfaceGeometry.wellFormed()) {
    return std::unexpected(Error(RoadSurfaceErrorCode::GeometryRejected));
  }
  return {};
}

}

std::expected<RoadSurface, RoadSurfaceError>
RoadSurfaceBuilder::Build(const RoadAlignment &alignment,
                          const TangentFrame &renderFrame,
                          RoadSurfaceBuildOptions options) {
  if (!ValidOptions(options)) {
    return std::unexpected(Error(RoadSurfaceErrorCode::InvalidOptions));
  }
  if (alignment.Edges().empty() || !std::isfinite(alignment.LengthM()) ||
      alignment.LengthM() <= 0.0) {
    return std::unexpected(Error(RoadSurfaceErrorCode::InvalidAlignment));
  }
  RoadSurface result;
  result.SourceIdentity = alignment.SourceIdentity();
  result.TerrainDigest = alignment.TerrainDigest();
  result.AlignmentAnchor = alignment.Anchor();
  result.RenderAnchor = renderFrame.Anchor();
  const TangentFrame alignmentFrame = TangentFrame::At(alignment.Anchor());
  const RoadFrameTransform frames{.Alignment = alignmentFrame, .Render = renderFrame};
  std::array<SurfaceBucket, kSurfaceCount> buckets;
  size_t segmentCount = 0;
  const uint64_t corridorKey = CorridorKeyOf(alignment);
  for (const RoadAlignmentEdge &edge : alignment.Edges()) {
    auto appended =
        AppendEdge(alignment, edge, frames, options, corridorKey, buckets, result, segmentCount);
    if (!appended) { return std::unexpected(appended.error()); }
  }
  auto geometry = InstallGeometry(buckets, result);
  if (!geometry) { return std::unexpected(geometry.error()); }
  return result;
}

}
