#include "InstanceDemand.h"
#include "FrameContext.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <limits>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

FrameContext View(bool turned) {
  FrameContext view;
  view.Mvp.Column.fill(0);
  view.Mvp[5] = 1;
  constexpr float kNearM = .1f;
  view.Mvp[14] = kNearM;
  if (turned) {
    view.Mvp.At(0, 2) = -1;
    view.Mvp.At(3, 0) = 1;
  } else {
    view.Mvp[0] = 1;
    view.Mvp[11] = -1;
  }
  return view;
}

void CheckPoliciesAndUnknown(const std::array<BatchSphere, 4> &bounds,
                             const std::array<double, 4> &errors,
                             const InstanceDemand &demand) {
  auto unknownBounds = bounds;
  unknownBounds[0][3] = -1;
  unknownBounds[1][0] = std::numeric_limits<float>::quiet_NaN();
  auto unknownError = errors;
  unknownError[2] = std::numeric_limits<double>::infinity();
  const auto unknown = PlanInstancePatches(unknownBounds, unknownError, demand);
  CHECK(unknown && unknown->size() == bounds.size(), "unknown metadata is never discarded");
  if (unknown) {
    for (size_t index = 0; index < 3; ++index) {
      CHECK((*unknown)[index].Index == index && (*unknown)[index].Detail == InstanceDetail::Unknown,
            "invalid bounds or unknown error stay explicitly unresolved by the classifier");
    }
    CHECK((*unknown)[0].Visible && (*unknown)[0].Caster,
          "invalid bounds cannot prove exclusion from view or caster domains");
  }
  CHECK(!PlanInstancePatches(bounds, std::span(errors).first(3), demand),
        "missing per-patch error inputs fail before a partial result is returned");
  auto invalidView = demand;
  invalidView.Projection.FocalPx = 0;
  CHECK(!PlanInstancePatches(bounds, errors, invalidView),
        "unspecified projection has no quality authority");
  auto zeroAllowance = demand;
  zeroAllowance.Projection.AllowedErrorPx = 0;
  const auto exact = PlanInstancePatches(bounds, errors, zeroAllowance);
  CHECK(exact, "zero allowance is an explicit valid policy");
  if (exact) {
    for (const auto &need : *exact) {
      CHECK(need.Detail == InstanceDetail::Resolved,
            "positive displacement never satisfies zero allowance");
    }
  }
}
}

int main() {
  const std::array bounds{BatchSphere{0, 0, -10, 1},
                          BatchSphere{0, 0, -1000, 1},
                          BatchSphere{1000, 0, 0, 1},
                          BatchSphere{0, 0, 0, 5}};
  const std::array errors{1., 1., 1., 1.};
  const std::array view{BatchFrustum(View(false), {})};
  const std::array turned{BatchFrustum(View(true), {})};
  const InstanceDemand demand{
      .EyeM = {}, .Projection = {.FocalPx = 100}, .Views = view, .Casters = turned};
  const auto planned = PlanInstancePatches(bounds, errors, demand);
  CHECK(planned && planned->size() == bounds.size(),
        "every supplied patch retains an explicit demand record");
  if (planned) {
    CHECK((*planned)[0].Detail == InstanceDetail::Resolved && (*planned)[0].Visible &&
              !(*planned)[0].Caster,
          "near detail and offscreen caster interest are independent");
    CHECK((*planned)[1].Detail == InstanceDetail::Unresolved && (*planned)[1].Visible,
          "small projected detail still requests unresolved representation, not empty space");
    CHECK((*planned)[2].Detail == InstanceDetail::Unresolved && !(*planned)[2].Visible &&
              (*planned)[2].Caster,
          "a patch outside the view retains its independent caster need");
    CHECK((*planned)[3].Detail == InstanceDetail::Resolved,
          "bounds containing the eye cannot justify distant-detail approximation");
    auto changed = demand;
    changed.Views = turned;
    const auto rotated = PlanInstancePatches(bounds, errors, changed);
    CHECK(rotated, "rotation remains a valid selection query");
    if (rotated) {
      for (size_t index = 0; index < bounds.size(); ++index) {
        CHECK((*rotated)[index].Index == index &&
                  (*rotated)[index].Detail == (*planned)[index].Detail,
              "rotation changes visibility without changing roundabout detail demand or patch "
              "identity");
      }
      CHECK(!(*rotated)[0].Visible && (*rotated)[2].Visible,
            "camera rotation changes actual side-plane visibility");
    }
  }
  CheckPoliciesAndUnknown(bounds, errors, demand);
  const std::array<BatchSphere, 1> boundary{BatchSphere{0, 0, -101, 1}};
  const std::array boundaryError{1.};
  const auto guarded = PlanInstancePatches(boundary, boundaryError, demand);
  CHECK(guarded && guarded->front().Detail == InstanceDetail::Resolved,
        "numeric uncertainty at the exact policy boundary retains detail");
  auto noDomains = demand;
  noDomains.Views = {};
  noDomains.Casters = {};
  const auto round = PlanInstancePatches(bounds, errors, noDomains);
  CHECK(round && round->size() == bounds.size() &&
            round->front().Detail == InstanceDetail::Resolved,
        "absence of draw domains does not erase roundabout detail residency demand");
  auto translatedBounds = bounds;
  auto translatedDemand = noDomains;
  constexpr Vec3 shift{{1000000, -2000000, 3000000}};
  translatedDemand.EyeM = shift;
  for (auto &bound : translatedBounds) {
    for (size_t axis = 0; axis < 3; ++axis) { bound[axis] += static_cast<float>(shift[axis]); }
  }
  const auto translated = PlanInstancePatches(translatedBounds, errors, translatedDemand);
  CHECK(translated && translated->size() == bounds.size(), "translated render frames remain valid");
  if (translated && round) {
    for (size_t index = 0; index < bounds.size(); ++index) {
      CHECK((*translated)[index].Detail == (*round)[index].Detail,
            "a common metric datum shift does not change roundabout need classification");
    }
  }
  const std::array emptyBounds = std::array<BatchSphere, 0>{};
  const std::array emptyErrors = std::array<double, 0>{};
  const auto empty = PlanInstancePatches(emptyBounds, emptyErrors, noDomains);
  CHECK(empty && empty->empty(), "a supplied empty patch list needs no fabricated geometry");
  auto invalidView = demand;
  invalidView.EyeM[1] = std::numeric_limits<double>::infinity();
  CHECK(!PlanInstancePatches(bounds, errors, invalidView),
        "a nonfinite eye cannot justify detail selection");
  return Report();
}
