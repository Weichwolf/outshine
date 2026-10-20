#include "WaterHeightProgram.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Error = WaterHeightError;
using Entry = Eigen::Triplet<double, OSQPInt>;
constexpr double kNormalMargin = 1e-12;
constexpr double kContactWeightTolerance = 64 * std::numeric_limits<double>::epsilon();
constexpr double kPositiveNormalFloor = .5;
constexpr double kTriangleMassDivisor = 12;
constexpr size_t kTriangleMassEntries = 9;

void Assemble(WaterSparseMatrix &matrix, std::vector<Entry> &entries) {
  std::ranges::sort(entries, [](const Entry &left, const Entry &right) {
    if (left.col() != right.col()) { return left.col() < right.col(); }
    return left.row() < right.row();
  });
  matrix.setFromSortedTriplets(entries.data(), entries.data() + entries.size());
}

std::expected<void, Error> Validate(const WaterHeightProblem &problem,
                                    const WaterHeightBudget &budget) {
  const size_t nv = problem.VerticesXY.size();
  const size_t nf = problem.Faces.size();
  if (nv == 0 || nf == 0 || problem.TargetUpM.size() != nv || problem.PrimaryGrades.size() != nf ||
      !std::isfinite(budget.SolveSeconds) || budget.SolveSeconds < 0) {
    return std::unexpected(Error::InvalidInput);
  }
  if (nv > budget.Vertices || nf > budget.Faces || problem.Grades.size() > budget.Rows ||
      problem.Contacts.size() > budget.Contacts || budget.Iterations == 0 ||
      budget.SolveSeconds == 0) {
    return std::unexpected(Error::CapacityExceeded);
  }
  const uint64_t rows =
      2 * static_cast<uint64_t>(problem.Grades.size()) + nf + 2 * nv + problem.Contacts.size();
  if (rows > budget.Rows || rows * 3 > static_cast<uint64_t>(std::numeric_limits<OSQPInt>::max()) ||
      nv > static_cast<uint64_t>(std::numeric_limits<OSQPInt>::max()) ||
      !std::in_range<OSQPInt>(budget.Iterations)) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (std::ranges::any_of(problem.TargetUpM, [](double value) { return !std::isfinite(value); })) {
    return std::unexpected(Error::InvalidInput);
  }
  return {};
}

std::expected<WaterFitFacet, Error> Facet(const WaterHeightProblem &problem, size_t index) {
  const auto vertices = problem.Faces[index].Vertices;
  std::array<std::array<double, 2>, 3> triangle{};
  for (size_t at = 0; at < vertices.size(); ++at) {
    if (vertices[at] >= problem.VerticesXY.size()) { return std::unexpected(Error::InvalidInput); }
    triangle[at] = problem.VerticesXY[vertices[at]];
  }
  const auto cap = BoundWaterFacetNormals(problem.Frame, triangle, problem.UpBoundsM);
  if (!cap) {
    return std::unexpected(cap.error() == WaterFrameError::InvalidInput
                               ? Error::InvalidInput
                               : Error::OutsidePhysicalDomain);
  }
  const double ux = triangle[1][0] - triangle[0][0];
  const double uy = triangle[1][1] - triangle[0][1];
  const double vx = triangle[2][0] - triangle[0][0];
  const double vy = triangle[2][1] - triangle[0][1];
  const double det = ux * vy - uy * vx;
  if (!std::isfinite(det) || det == 0) { return std::unexpected(Error::InvalidInput); }
  WaterFitFacet result{.Gradients = {{{uy - vy, vx - ux}, {vy, -vx}, {-uy, ux}}},
                       .Cap = *cap,
                       .AreaM2 = std::abs(det) / 2};
  for (auto &gradient : result.Gradients) {
    gradient[0] /= det;
    gradient[1] /= det;
    if (!std::isfinite(gradient[0]) || !std::isfinite(gradient[1])) {
      return std::unexpected(Error::InvalidInput);
    }
  }
  return result;
}

std::expected<Vec3, Error> Tangent(const WaterGradeConstraint &grade,
                                   std::span<const WaterFitFacet> facets) {
  if (grade.Face >= facets.size() || !std::isfinite(grade.Lower) || !std::isfinite(grade.Upper) ||
      grade.Lower > 0 || grade.Upper < 0) {
    return std::unexpected(Error::InvalidInput);
  }
  const auto up = facets[grade.Face].Cap.Up;
  const auto direction = grade.DirectionEnu - up * Dot(grade.DirectionEnu, up);
  const double length = Length(direction);
  if (!std::isfinite(length) || length <= kNormalMargin) {
    return std::unexpected(Error::InvalidInput);
  }
  return direction * (1 / length);
}

std::expected<double, Error>
NormalBound(const WaterHeightProblem &problem, const WaterHeightProgram &program, size_t face) {
  const auto selected = problem.PrimaryGrades[face];
  for (const auto index : selected) {
    if (index >= problem.Grades.size() || problem.Grades[index].Face != face) {
      return std::unexpected(Error::InvalidInput);
    }
  }
  const double eigenvalue =
      1 - std::abs(Dot(program.Tangents[selected[0]], program.Tangents[selected[1]])) -
      kNormalMargin;
  if (eigenvalue <= 0) { return std::unexpected(Error::OutsidePhysicalDomain); }
  const auto magnitude = [&](uint32_t index) {
    return std::max(std::abs(problem.Grades[index].Lower), std::abs(problem.Grades[index].Upper));
  };
  const double grade =
      std::hypot(magnitude(selected[0]), magnitude(selected[1])) / std::sqrt(eigenvalue);
  const auto up = program.Facets[face].Cap.Up;
  const double horizontal = Length(Vec3{{0, 0, 1}} - up * up[2]) + kNormalMargin;
  const double denominator = up[2] - grade * horizontal - kNormalMargin;
  if (denominator <= 0) { return std::unexpected(Error::OutsidePhysicalDomain); }
  const double bound = std::sqrt(1 + grade * grade) / denominator * (1 + kNormalMargin);
  if (!std::isfinite(bound)) { return std::unexpected(Error::OutsidePhysicalDomain); }
  return bound;
}

std::expected<void, Error> Mass(const WaterHeightProblem &problem, WaterHeightProgram &program) {
  std::vector<Entry> entries;
  entries.reserve(problem.Faces.size() * kTriangleMassEntries);
  for (size_t face = 0; face < problem.Faces.size(); ++face) {
    const auto vertices = problem.Faces[face].Vertices;
    const double weight = program.Facets[face].AreaM2 / kTriangleMassDivisor;
    for (size_t row = 0; row < vertices.size(); ++row) {
      for (size_t column = 0; column < vertices.size(); ++column) {
        entries.emplace_back(vertices[row], vertices[column], weight * (row == column ? 2 : 1));
      }
    }
  }
  if (entries.empty()) { return std::unexpected(Error::InvalidInput); }
  const auto count = static_cast<uint32_t>(problem.VerticesXY.size());
  if (count == 0) { return std::unexpected(Error::InvalidInput); }
  WaterSparseMatrix mass(count, count);
  Assemble(mass, entries);
  mass /= mass.sum();
  program.Mass = std::move(mass);
  program.Hessian = program.Mass.triangularView<Eigen::Upper>();
  Eigen::VectorXd target(count);
  for (uint32_t at = 0; at < count; ++at) { target[at] = problem.TargetUpM[at] - program.DatumM; }
  program.Linear = -(program.Mass * target);
  return {};
}

struct Rows {
  std::vector<Entry> Entries;
  std::vector<double> Upper;

  void Normal(const WaterHeightProblem &problem,
              const WaterHeightProgram &program,
              uint32_t face,
              const Vec3 &direction,
              double auxiliary) {
    const auto &facet = program.Facets[face];
    for (size_t at = 0; at < facet.Gradients.size(); ++at) {
      const auto &gradient = facet.Gradients[at];
      Entries.emplace_back(Upper.size(),
                           problem.Faces[face].Vertices[at],
                           -(direction[0] * gradient[0] + direction[1] * gradient[1]));
    }
    Upper.push_back(-direction[2] - auxiliary * facet.NormalBound);
  }
};

void GradeRows(const WaterHeightProblem &problem, const WaterHeightProgram &program, Rows &rows) {
  for (const double sign : {1., -1.}) {
    for (size_t at = 0; at < problem.Grades.size(); ++at) {
      const auto &grade = problem.Grades[at];
      const auto &facet = program.Facets[grade.Face];
      const double limit = sign > 0 ? -grade.Lower : grade.Upper;
      const double k = std::sqrt(1 - facet.Cap.Delta * facet.Cap.Delta);
      const auto direction = program.Tangents[at] * sign - facet.Cap.Up * (limit * k);
      rows.Normal(problem, program, grade.Face, direction, facet.Cap.Delta * (1 + limit * k));
    }
  }
  for (uint32_t face = 0; face < program.Facets.size(); ++face) {
    const auto &facet = program.Facets[face];
    rows.Normal(problem, program, face, facet.Cap.Up * -1., facet.Cap.Delta + kPositiveNormalFloor);
  }
}

std::expected<void, Error>
Contacts(const WaterHeightProblem &problem, const WaterHeightProgram &program, Rows &rows) {
  for (const auto &contact : problem.Contacts) {
    double sum = 0;
    if (contact.Face >= problem.Faces.size() || !std::isfinite(contact.UpM) ||
        contact.UpM < problem.UpBoundsM[0] || contact.UpM > problem.UpBoundsM[1]) {
      return std::unexpected(Error::InvalidInput);
    }
    for (size_t at = 0; at < contact.Barycentric.size(); ++at) {
      const auto value = contact.Barycentric[at];
      if (!std::isfinite(value) || value < 0) { return std::unexpected(Error::InvalidInput); }
      sum += value;
      rows.Entries.emplace_back(rows.Upper.size(), problem.Faces[contact.Face].Vertices[at], value);
    }
    if (std::abs(sum - 1) > kContactWeightTolerance) {
      return std::unexpected(Error::InvalidInput);
    }
    rows.Upper.push_back(contact.UpM - program.DatumM * sum);
  }
  return {};
}

std::expected<void, Error> Constraints(const WaterHeightProblem &problem,
                                       WaterHeightProgram &program) {
  const size_t maximum = 2 * problem.Grades.size() + problem.Faces.size() +
                         2 * problem.VerticesXY.size() + problem.Contacts.size();
  Rows rows;
  rows.Entries.reserve(maximum * 3);
  rows.Upper.reserve(maximum);
  GradeRows(problem, program, rows);
  for (const double sign : {1., -1.}) {
    for (uint32_t vertex = 0; vertex < problem.VerticesXY.size(); ++vertex) {
      rows.Entries.emplace_back(rows.Upper.size(), vertex, sign);
      rows.Upper.push_back(sign > 0 ? problem.UpBoundsM[1] - program.DatumM
                                    : program.DatumM - problem.UpBoundsM[0]);
    }
  }
  program.Inequalities = rows.Upper.size();
  const auto contacts = Contacts(problem, program, rows);
  if (!contacts) { return contacts; }
  const auto count = static_cast<uint32_t>(rows.Upper.size());
  program.Constraints.resize(count, static_cast<uint32_t>(problem.VerticesXY.size()));
  Assemble(program.Constraints, rows.Entries);
  program.Upper = Eigen::Map<Eigen::VectorXd>(rows.Upper.data(), count);
  program.Lower = Eigen::VectorXd::Constant(count, -OSQP_INFTY);
  for (auto at = static_cast<uint32_t>(program.Inequalities); at < count; ++at) {
    program.Lower[at] = program.Upper[at];
  }
  if (!program.Linear.allFinite() || !program.Upper.allFinite() ||
      !Eigen::Map<const Eigen::VectorXd>(program.Constraints.valuePtr(),
                                         program.Constraints.nonZeros())
           .allFinite() ||
      !Eigen::Map<const Eigen::VectorXd>(program.Mass.valuePtr(), program.Mass.nonZeros())
           .allFinite()) {
    return std::unexpected(Error::OutsidePhysicalDomain);
  }
  return {};
}

double Datum(std::span<const double> heights) {
  std::vector<double> values(heights.begin(), heights.end());
  const auto half = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
  std::nth_element(values.begin(), half, values.end());
  return values.size() % 2 == 0 ? *std::max_element(values.begin(), half) / 2 + *half / 2 : *half;
}
}

std::expected<WaterHeightProgram, WaterHeightError>
BuildWaterHeightProgram(const WaterHeightProblem &problem,
                        const WaterHeightBudget &budget,
                        const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  const auto valid = Validate(problem, budget);
  if (!valid) { return std::unexpected(valid.error()); }
  WaterHeightProgram program;
  program.DatumM = Datum(problem.TargetUpM);
  program.Facets.reserve(problem.Faces.size());
  for (size_t face = 0; face < problem.Faces.size(); ++face) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const auto facet = Facet(problem, face);
    if (!facet) { return std::unexpected(facet.error()); }
    program.Facets.push_back(*facet);
  }
  program.Tangents.reserve(problem.Grades.size());
  for (const auto &grade : problem.Grades) {
    auto tangent = Tangent(grade, program.Facets);
    if (!tangent) { return std::unexpected(tangent.error()); }
    program.Tangents.push_back(*tangent);
  }
  for (size_t face = 0; face < program.Facets.size(); ++face) {
    const auto bound = NormalBound(problem, program, face);
    if (!bound) { return std::unexpected(bound.error()); }
    program.Facets[face].NormalBound = *bound;
  }
  const auto mass = Mass(problem, program);
  if (!mass) { return std::unexpected(mass.error()); }
  const auto rows = Constraints(problem, program);
  if (!rows) { return std::unexpected(rows.error()); }
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  return program;
}
}
