#include "SurfaceHeightProgramInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <stop_token>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>

namespace outshine::Generators::HeightAssembly {
System::System(size_t variables, SurfaceHeightProgramBudget budget, const std::stop_token &stop)
    : Budget(budget), Stop(stop) {
  Program.Variables.resize(variables);
  Program.AdjustmentVariable = static_cast<uint32_t>(variables - 1);
}

bool System::Check() {
  if (Stop.stop_requested()) { return Fail(Error::Canceled); }
  return !Failure;
}

bool System::Fail(Error error) {
  if (!Failure) { Failure = error; }
  return false;
}

bool System::Count(size_t &count, size_t maximum, size_t added) {
  if (!Check()) { return false; }
  if (count > maximum || added > maximum - count) { return Fail(Error::CapacityExceeded); }
  count += added;
  return true;
}

void System::Add(std::span<const uint32_t> columns,
                 std::span<const double> values,
                 double upper,
                 double sign) {
  if (!Check()) { return; }
  if (columns.size() != values.size() || !std::isfinite(upper)) {
    Fail(Error::InvalidInput);
    return;
  }
  if (Program.Upper.size() >= Budget.Conditions ||
      values.size() > Budget.Nonzeros - Entries.size()) {
    Fail(Error::CapacityExceeded);
    return;
  }
  const int row = static_cast<int>(Program.Upper.size());
  for (size_t at = 0; at < columns.size(); ++at) {
    if (columns[at] >= Program.Variables.size() || !std::isfinite(values[at])) {
      Fail(Error::InvalidInput);
      return;
    }
    Entries.push_back(
        {.Row = row, .Column = static_cast<int>(columns[at]), .Value = values[at] * sign});
  }
  Program.Upper.push_back(upper * sign);
}

void System::Equal(std::span<const uint32_t> columns,
                   std::span<const double> values,
                   double upper) {
  Add(columns, values, upper);
  Add(columns, values, upper, -1);
}

PlanarPoint Difference(PlanarPoint a, PlanarPoint b) {
  return {a[0] - b[0], a[1] - b[1]};
}

namespace {
double Dot(PlanarPoint a, PlanarPoint b) {
  return a[0] * b[0] + a[1] * b[1];
}

double Project(PlanarPoint point, PlanarPoint a, PlanarPoint b) {
  const auto d = Difference(b, a);
  return std::clamp(Dot(Difference(point, a), d) / Dot(d, d), 0., 1.);
}

}

double SegmentDistance(PlanarPoint point, PlanarPoint a, PlanarPoint b) {
  const auto d = Difference(b, a);
  const double t = Project(point, a, b);
  return std::hypot(point[0] - a[0] - t * d[0], point[1] - a[1] - t * d[1]);
}

double HeightAt(std::span<const XYZ> line, PlanarPoint point, System &system) {
  double best = std::numeric_limits<double>::infinity();
  double height = 0;
  for (size_t at = 1; at < line.size(); ++at) {
    if (!system.Count(system.Work.PriorTests, system.Budget.PriorTests)) { return 0; }
    const auto &a = line[at - 1];
    const auto &b = line[at];
    const double t = Project(point, {a[0], a[1]}, {b[0], b[1]});
    const double d =
        std::hypot(point[0] - a[0] - t * (b[0] - a[0]), point[1] - a[1] - t * (b[1] - a[1]));
    if (d < best) {
      best = d;
      height = a[2] + t * (b[2] - a[2]);
    }
  }
  return height;
}

std::array<Coefficients, 2>
Gradients(std::span<const PlanarPoint> points, PlanarFace face, bool quantized) {
  auto a = points[face[0]];
  auto b = points[face[1]];
  auto c = points[face[2]];
  if (quantized) {
    for (auto *p : {&a, &b, &c}) {
      for (auto &value : *p) { value = static_cast<float>(value); }
    }
  }
  const auto u = Difference(b, a);
  const auto v = Difference(c, a);
  const double determinant = u[0] * v[1] - u[1] * v[0];
  return {{{(u[1] - v[1]) / determinant, v[1] / determinant, -u[1] / determinant},
           {(v[0] - u[0]) / determinant, -v[0] / determinant, u[0] / determinant}}};
}

Coefficients Along(const std::array<Coefficients, 2> &gradient, PlanarPoint direction) {
  Coefficients values{};
  for (size_t at = 0; at < 3; ++at) {
    values[at] = direction[0] * gradient[0][at] + direction[1] * gradient[1][at];
  }
  return values;
}
}

namespace outshine::Generators {
std::expected<SurfaceHeightProgram, SurfaceHeightProgramError>
BuildSurfaceHeightProgram(const SurfaceHeightRequest &request,
                          SurfaceHeightProgramBudget budget,
                          const std::stop_token &stop) {
  using namespace HeightAssembly;
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  const auto points = request.Topology.Points.size();
  const auto maximum = static_cast<size_t>(std::numeric_limits<int>::max());
  if (points >= maximum || request.Ports.size() > (maximum - points - 1) / 3 ||
      points + request.Ports.size() * 3 + 1 > budget.Variables || budget.Conditions > maximum ||
      budget.Nonzeros > maximum) {
    return std::unexpected(Error::CapacityExceeded);
  }
  System system(points + request.Ports.size() * 3 + 1, budget, stop);
  auto input = PrepareInput(request, system);
  if (!input) { return std::unexpected(input.error()); }
  Workspace geometry;
  bool valid = PrepareGeometry(*input, geometry, system) && Priors(*input, system) &&
               Contacts(*input, geometry, system);
  for (size_t source = 0; valid && source < input->Sources.size(); ++source) {
    valid = system.Check() && Facets(*input, geometry, source, system);
  }
  valid = valid && Planes(*input, system) && Flights(*input, geometry, system) &&
          Overlaps(*input, system);
  if (!valid) { return std::unexpected(system.Failure.value_or(Error::InvalidInput)); }
  system.Program.Conditions =
      SparseMatrix<int>::Assemble(static_cast<int>(system.Program.Upper.size()),
                                  static_cast<int>(system.Program.Variables.size()),
                                  system.Entries);
  if (!Project(*input, system) || !system.Check()) {
    return std::unexpected(system.Failure.value_or(Error::InvalidInput));
  }
  return SurfaceHeightProgram{.Program = std::move(system.Program),
                              .Projection = std::move(system.Projection),
                              .DatumM = system.Datum,
                              .PointCount = points,
                              .Work = system.Work};
}
}
