#ifndef OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEHEIGHTPROGRAMINTERNAL_H
#define OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEHEIGHTPROGRAMINTERNAL_H
#include "SurfaceHeightProgram.h"
#include "GeosGeometry.h"
#include "PlanHierarchy.h"
#include <geos_c.h>
#include <optional>

namespace outshine::Generators::HeightAssembly {
using Error = SurfaceHeightProgramError;
using XYZ = SurfaceHeightPoint;
using Coefficients = std::array<double, 3>;

struct Source {
  double Width = 0, Grade = 0;
  bool Steps = false;
  std::span<const PlanarPoint> Axis;
  std::span<const XYZ> Prior;
  std::span<const PlanarFace> Faces;
  std::span<const uint32_t> FaceIds;
  std::vector<uint32_t> Vertices;
  std::span<const std::array<PlanarPoint, 2>> Sections;
  const PlanarDomain *Domain = nullptr;
};

struct Input {
  const std::vector<PlanarPoint> &Points;
  std::vector<Source> Sources;
  std::span<const SurfaceHeightJunction> Ports;
  std::span<const SurfaceHeightFlight> Flights;
  std::span<const SurfaceHeightContact> Contacts;
  std::vector<int> Orders;
  double Adjustment = 0, Crossfall = 0;
};

struct System {
  RoadProfiles::HeightProgram Program;
  SparseMatrix<int> Projection;
  std::vector<SparseEntry<int>> Entries;
  double Datum = 0, HeightBound = 0;
  SurfaceHeightProgramWork Work;
  SurfaceHeightProgramBudget Budget;
  const std::stop_token &Stop;
  std::optional<Error> Failure;

  System(size_t variables, SurfaceHeightProgramBudget budget, const std::stop_token &stop);
  [[nodiscard]] bool Check();
  bool Fail(Error error);
  bool Count(size_t &count, size_t maximum, size_t added = 1);
  void Add(std::span<const uint32_t> columns,
           std::span<const double> values,
           double upper,
           double sign = 1);
  void Equal(std::span<const uint32_t> columns, std::span<const double> values, double upper);
};

using Geometry = Spatial::GeosGeometry;

struct Context {
  GEOSContextHandle_t Handle = GEOS_init_r();
  ~Context();
  Context() = default;
  Context(const Context &) = delete;
  Context &operator=(const Context &) = delete;
};

struct SourceGeometry {
  Geometry Domain;
  std::vector<Geometry> Faces;
  PlanHierarchy Index;
};

struct Workspace {
  Context Geos;
  std::vector<SourceGeometry> Sources;
};

[[nodiscard]] std::expected<Input, Error> PrepareInput(const SurfaceHeightRequest &request,
                                                       System &system);
PlanarPoint Difference(PlanarPoint a, PlanarPoint b);
double SegmentDistance(PlanarPoint point, PlanarPoint a, PlanarPoint b);
double HeightAt(std::span<const XYZ> line, PlanarPoint point, System &system);
std::array<Coefficients, 2>
Gradients(std::span<const PlanarPoint> points, PlanarFace face, bool quantized);
Coefficients Along(const std::array<Coefficients, 2> &gradient, PlanarPoint direction);
Geometry Line(GEOSContextHandle_t context, std::span<const PlanarPoint> points);
Box Bounds(std::span<const PlanarPoint> points);
bool PrepareGeometry(const Input &input, Workspace &workspace, System &system);
bool Influence(const Input &input,
               Workspace &workspace,
               size_t owner,
               PlanarPoint a,
               PlanarPoint b,
               double threshold,
               std::vector<uint32_t> &faces,
               System &system);
bool Contacts(const Input &input, Workspace &workspace, System &system);
bool Priors(const Input &input, System &system);
bool Facets(const Input &input, Workspace &workspace, size_t owner, System &system);
bool Planes(const Input &input, System &system);
bool Flights(const Input &input, Workspace &workspace, System &system);
bool Overlaps(const Input &input, System &system);
bool Project(const Input &input, System &system);
}
#endif
