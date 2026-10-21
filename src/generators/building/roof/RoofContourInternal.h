#ifndef OUTSHINE_GENERATORS_BUILDING_ROOFCONTOURINTERNAL_H
#define OUTSHINE_GENERATORS_BUILDING_ROOFCONTOURINTERNAL_H
#include "GeosGeometry.h"
#include "HipRoofPlan.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <expected>
#include <span>
#include <vector>

namespace outshine::Generators::RoofContour {
using Point = std::array<double, 2>;
using Ring = std::vector<Point>;

struct Loop {
  std::vector<size_t> Owners;
  Ring Points;
};

struct Failure {
  HipRoofError Kind = HipRoofError::PartitionFailed;
  const char *Detail;

  explicit Failure(const char *detail) : Detail(detail) {}

  Failure(HipRoofError kind, const char *detail) : Kind(kind), Detail(detail) {}
};
template <class T> using Checked = std::expected<T, Failure>;

struct Input {
  std::span<const Ring> Rings;
  std::span<const Point> Normals;
  std::span<const double> Intercepts;
  HipRoofPlan::Limits Limits;
  double ToleranceM;
  size_t EventLimit = 4096;
  double Seconds = 2;
};

struct Step {
  double NowM, ToleranceM;
};

struct Event {
  std::vector<size_t> Participants;
  Point At;
};

struct CollisionResult {
  double AdvanceM;
  size_t PairCount;
  std::vector<Event> Events;
  Ring Motions;
};

struct Partition {
  Ring Nodes;
  std::vector<double> Distances;
  std::vector<std::vector<size_t>> Faces;
  std::vector<size_t> Ownership;
  double MaximumInterfaceRefinementM = 0;
  double MaximumFinalRefinementM = 0;
  size_t Steps = 0, CollisionPairs = 0, SweptPieces = 0;
  double BuildSeconds = 0, CompleteSeconds = 0;
};

struct Geos {
  GEOSContextHandle_t Handle = GEOS_init_r();
  Geos() = default;
  Geos(const Geos &) = delete;
  Geos &operator=(const Geos &) = delete;

  ~Geos() {
    if (Handle != nullptr) { GEOS_finish_r(Handle); }
  }

  Spatial::GeosGeometry Own(GEOSGeometry *p) const { return Spatial::GeosGeometry(p, {Handle}); }
};

struct Front {
  Point At, Speed, Normal;
  double Intercept;
  size_t Following;
};

struct Contact {
  double Time;
  Point At;
};

struct Segment {
  Point From, To;
  size_t Owner;
};

struct Refined {
  Ring Points;
  double MaximumM = 0;
};

using Vector3 = std::array<double, 3>;

struct LeastSquares {
  Vector3 Solution{};
  size_t Rank = 0;
};

using Geometry = Spatial::GeosGeometry;

struct ContourMeasure {
  double ToleranceM, SignedAreaM2;
};

struct ContourBoundary {
  std::vector<Segment> Segments;
  Ring Coordinates;
  double SignedAreaM2;
};

struct ContourGeometry {
  Geometry Material, Anchors;
  std::vector<Geometry> Sources;
};

struct BoundaryLimits {
  size_t Owners;
  double ToleranceM;
};

Checked<ContourBoundary> CollectBoundary(std::span<const Loop> loops, BoundaryLimits limits);
Checked<ContourGeometry> PrepareContourGeometry(const Geos &g,
                                                std::span<const Loop> loops,
                                                std::span<const Segment> segments,
                                                std::span<const Point> coordinates,
                                                ContourMeasure measure);
inline constexpr size_t kMaximumFrontVertices = 1024;
inline constexpr size_t kMaximumWeldVertices = 16384;
inline constexpr size_t kMaximumWeldPairs = 262144;
inline constexpr size_t kMaximumCandidates = 2000000;
inline constexpr double kParallelNormalTolerance = 1e-12;
inline constexpr double kMinimumClosingRate = 1e-12;
Point Add(Point a, Point b);
Point Subtract(Point a, Point b);
Point Scale(Point p, double factor);
double Dot(Point a, Point b);
Checked<Ring> WeldVertices(std::span<const Point> input, double tolerance, size_t fixed = 0);
Ring Unique(std::span<const Point> input);
Checked<Ring>
Subdivide(std::span<const Point> input, std::span<const Point> anchors, double tolerance);
LeastSquares SolveSmall(std::vector<Vector3> rows, std::span<const double> rhs);

struct RefinementLimits {
  size_t FixedVertices;
  double ToleranceM;
};

Checked<Refined> Refine(Ring points,
                        std::span<const int> owners,
                        std::span<const Point> normals,
                        std::span<const double> intercepts,
                        RefinementLimits limits);
Geometry Line(const Geos &g, std::span<const Point> points, bool ring = false);
Geometry Collection(const Geos &g, std::vector<Geometry> parts, int type = GEOS_GEOMETRYCOLLECTION);
Checked<Ring> Coordinates(const Geos &g, const GEOSGeometry *geometry, bool closed);
Geometry Polygon(const Geos &g, std::span<const Point> points);
Geometry Footprint(const Geos &g, std::span<const Ring> rings);
Checked<CollisionResult> Collision(std::span<const Loop> loops,
                                   std::span<const Point> normals,
                                   std::span<const double> intercepts,
                                   Step step);
Checked<std::vector<Loop>> Stitch(std::span<const Loop> loops, double tolerance);
Checked<std::vector<Loop>> Contours(std::span<const Loop> fronts,
                                    std::span<const Point> normals,
                                    std::span<const double> intercepts,
                                    Step step);
Checked<Partition> Assemble(const Input &input,
                            std::span<const std::vector<std::array<Point, 4>>> patches);
Checked<Partition> Roof(const Input &input);
Checked<double> RefineRidges(Partition &plan, const Input &input);
Checked<void> ValidatePartition(const Geos &g,
                                const GEOSGeometry *footprint,
                                const Partition &plan,
                                const Input &input);
Checked<Partition> Prepare(const Input &input);
}
#endif
