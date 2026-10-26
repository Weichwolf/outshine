#include "RoadPortFieldProgram.h"
#include "SparseMatrix.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <numbers>
#include <numeric>
#include <span>
#include <stop_token>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Error = RoadProfiles::HeightFitError;
constexpr double kMargin = .99;
constexpr double kMaximumModelHeightM = 1e6;

struct RowLimit {
  double Upper;
  double Rho = 0;
};

struct PlainFrame {
  uint32_t Plane;
  uint32_t Patch;
  std::array<double, 4> Frame;
};

bool ValidFrame(const std::array<double, 4> &frame) {
  constexpr double maximum = 1e12;
  return std::ranges::all_of(
             frame,
             [](double value) { return std::isfinite(value) && std::abs(value) <= maximum; }) &&
         frame[0] * frame[3] - frame[1] * frame[2] != 0;
}

double Datum(std::span<const RoadPortPrior> priors) {
  std::vector<double> heights;
  heights.reserve(priors.size());
  for (const auto &prior : priors) { heights.push_back(prior.HeightM); }
  const auto middle = heights.begin() + static_cast<std::ptrdiff_t>(heights.size() / 2);
  std::nth_element(heights.begin(), middle, heights.end());
  return heights.size() % 2 != 0
             ? *middle
             : std::midpoint(*std::max_element(heights.begin(), middle), *middle);
}

struct Builder {
  Builder(const RoadPortFieldFitInput &input, RoadProfiles::HeightFitBudget budget)
      : Input(input), Budget(budget) {
    Result.DatumM = Datum(input.Priors);
    const size_t variables = 3 * input.Planes.size() + 1;
    Result.Program.Variables.resize(variables);
    Result.Program.AdjustmentVariable = static_cast<uint32_t>(variables - 1);
    Result.Program.Variables.back() = {.Minimum = 0, .Maximum = input.MaximumPriorChangeM};
    const size_t rows = 2 * input.Priors.size() + kPortFitPolygonSides * 15 * input.Faces.size();
    Result.Program.Upper.reserve(std::min(rows, budget.Conditions));
    Entries.reserve(std::min(7 * rows, budget.Nonzeros));
  }

  bool Row(uint32_t patch, const std::array<double, 6> &values, RowLimit limit) {
    const size_t needed =
        static_cast<size_t>(limit.Rho != 0) +
        static_cast<size_t>(std::ranges::count_if(values, [](double value) { return value != 0; }));
    if (Result.Program.Upper.size() >= Budget.Conditions ||
        needed > Budget.Nonzeros - Entries.size()) {
      return false;
    }
    const auto row = static_cast<int>(Result.Program.Upper.size());
    for (size_t at = 0; at < values.size(); ++at) {
      if (values[at] == 0) { continue; }
      const uint32_t plane = Input.Patches[patch].Parents[at / 3];
      Entries.push_back({.Row = row,
                         .Column = static_cast<int>(3 * static_cast<size_t>(plane) + at % 3),
                         .Value = values[at]});
    }
    if (limit.Rho != 0) {
      Entries.push_back({.Row = row,
                         .Column = static_cast<int>(Result.Program.AdjustmentVariable),
                         .Value = limit.Rho});
    }
    Result.Program.Upper.push_back(limit.Upper);
    return true;
  }

  bool Cone(uint32_t patch,
            const std::array<std::array<double, 6>, 2> &gradient,
            const std::array<double, 4> &frame) {
    const double upper =
        kMargin * std::cos(std::numbers::pi / static_cast<double>(kPortFitPolygonSides));
    for (size_t side = 0; side < kPortFitPolygonSides; ++side) {
      const double angle = (static_cast<double>(side) + .5) * 2 * std::numbers::pi /
                           static_cast<double>(kPortFitPolygonSides);
      const double east = std::cos(angle) * frame[0] + std::sin(angle) * frame[2];
      const double north = std::cos(angle) * frame[1] + std::sin(angle) * frame[3];
      std::array<double, 6> values{};
      for (size_t at = 0; at < values.size(); ++at) {
        values[at] = east * gradient[0][at] + north * gradient[1][at];
      }
      if (!Row(patch, values, {.Upper = upper})) { return false; }
    }
    ++Result.Work.GradientControls;
    return true;
  }

  RoadPortFieldProgram Finish() {
    Result.Program.Conditions =
        SparseMatrix<int>::Assemble(static_cast<int>(Result.Program.Upper.size()),
                                    static_cast<int>(Result.Program.Variables.size()),
                                    std::span(Entries));
    Result.Work.Conditions = Result.Program.Upper.size();
    Result.Work.Nonzeros = Result.Program.Conditions.Values.size();
    return std::move(Result);
  }

  RoadPortFieldProgram Result;

  const RoadPortFieldFitInput &Input;
  RoadProfiles::HeightFitBudget Budget;
  std::vector<SparseEntry<int>> Entries;
};

std::expected<void, Error> Observations(Builder &builder,
                                        const RoadPortField &prototype,
                                        std::span<const RoadPortPrior> priors,
                                        const std::stop_token &stop) {
  for (const auto &prior : priors) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const auto basis = prototype.BasisAt(prior.Patch, prior.At);
    if (!basis) { return std::unexpected(Error::InvalidInput); }
    const double shifted = prior.HeightM - builder.Result.DatumM;
    auto coefficients = basis->Height;
    if (!builder.Row(prior.Patch, coefficients, {.Upper = shifted, .Rho = -1})) {
      return std::unexpected(Error::CapacityExceeded);
    }
    for (auto &value : coefficients) { value = -value; }
    if (!builder.Row(prior.Patch, coefficients, {.Upper = -shifted, .Rho = -1})) {
      return std::unexpected(Error::CapacityExceeded);
    }
    ++builder.Result.Work.Observations;
  }
  return {};
}

std::expected<void, Error> Gradients(Builder &builder,
                                     const RoadPortField &prototype,
                                     const RoadPortFieldFitInput &input,
                                     const std::stop_token &stop) {
  std::vector<PlainFrame> plain;
  plain.reserve(input.Faces.size());
  for (const auto &face : input.Faces) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (face.Patch >= input.Patches.size() || !ValidFrame(face.GradeFrame)) {
      return std::unexpected(Error::InvalidInput);
    }
    const auto controls = prototype.Controls(face.Patch, face.Triangle);
    if (!controls) { return std::unexpected(Error::InvalidInput); }
    const auto &parents = input.Patches[face.Patch].Parents;
    if (parents[0] == parents[1]) {
      plain.push_back({.Plane = parents[0], .Patch = face.Patch, .Frame = face.GradeFrame});
    } else {
      for (const auto &control : *controls) {
        if (!builder.Cone(face.Patch, control.Gradient, face.GradeFrame)) {
          return std::unexpected(Error::CapacityExceeded);
        }
      }
    }
    ++builder.Result.Work.Faces;
  }
  const auto key = [](const PlainFrame &value) { return std::tie(value.Plane, value.Frame); };
  std::ranges::sort(plain, {}, key);
  const auto repeated = std::ranges::unique(plain, {}, key);
  plain.erase(repeated.begin(), repeated.end());
  for (const auto &item : plain) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const auto basis = prototype.BasisAt(item.Patch, {});
    if (!basis) { return std::unexpected(Error::InvalidInput); }
    if (!builder.Cone(item.Patch, basis->Gradient, item.Frame)) {
      return std::unexpected(Error::CapacityExceeded);
    }
  }
  return {};
}
}

std::expected<RoadPortFieldProgram, RoadProfiles::HeightFitError>
BuildRoadPortFieldProgram(const RoadPortFieldFitInput &input,
                          const RoadPortField &prototype,
                          RoadProfiles::HeightFitBudget budget,
                          const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  constexpr auto integerMaximum = static_cast<size_t>(std::numeric_limits<int>::max());
  if (budget.Variables > integerMaximum || budget.Conditions > integerMaximum ||
      budget.Nonzeros > integerMaximum || budget.Variables == 0 ||
      input.Planes.size() > (budget.Variables - 1) / 3 ||
      input.Priors.size() > budget.Conditions / 2 ||
      input.Faces.size() > budget.Conditions / kPortFitPolygonSides) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (input.Priors.empty() || input.Faces.empty() || !std::isfinite(input.MaximumPriorChangeM) ||
      input.MaximumPriorChangeM <= 0 ||
      !std::ranges::all_of(input.Priors, [](const RoadPortPrior &prior) {
        return std::isfinite(prior.HeightM) && std::abs(prior.HeightM) <= kMaximumModelHeightM;
      })) {
    return std::unexpected(Error::InvalidInput);
  }
  Builder builder(input, budget);
  const auto observations = Observations(builder, prototype, input.Priors, stop);
  if (!observations) { return std::unexpected(observations.error()); }
  const auto gradients = Gradients(builder, prototype, input, stop);
  if (!gradients) { return std::unexpected(gradients.error()); }
  return builder.Finish();
}
}
