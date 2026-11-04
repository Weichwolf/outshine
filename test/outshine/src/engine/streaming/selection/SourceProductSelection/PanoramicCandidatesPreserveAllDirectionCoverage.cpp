#include "SourceProductSelection.h"
#include "Viewing.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using Selection = SourceProductSelection;

std::optional<Selection> Sources(bool known = true) {
  const std::array bounds{Box{.Min = {{-10, -2, 0}}, .Max = {{-8, 2, 4}}},
                          Box{.Min = {{8, -2, 0}}, .Max = {{10, 2, 4}}}};
  auto hierarchy = PlanHierarchy::Build(bounds);
  if (!hierarchy) { return std::nullopt; }
  const auto error = known ? std::optional<double>(0.25) : std::nullopt;
  const std::array traits{Selection::Source{.Cohort = 1000, .Faces = 8, .ShellErrorM = error},
                          Selection::Source{.Cohort = 2000, .Faces = 8, .ShellErrorM = error}};
  auto selected = Selection::Of(
      std::make_shared<const PlanHierarchy>(std::move(*hierarchy)), traits, "panoramic");
  return selected ? std::optional<Selection>(std::move(*selected)) : std::nullopt;
}

void CheckRotationCoverage() {
  const auto sources = Sources();
  CHECK(sources, "declared shell errors form a complete native source hierarchy");
  if (!sources) { return; }
  const Vec3 eye{{0, 0, 1000}};
  const ProjectedErrorBudget budget{.FocalPx = 720, .AllowedErrorPx = 1};
  const auto selected = sources->SelectGeometry(eye, {{1280, 720}}, budget);
  CHECK(selected && selected->Products.size() == 1 &&
            selected->Products[0].Representation == Selection::Kind::Shell &&
            selected->Products[0].ProjectedErrorPx <= 1,
        "all rotations retain a shared qualified shell instead of originals behind the camera");
  if (selected && !selected->Products.empty()) {
    const auto members = sources->Members(*selected, selected->Products[0]);
    CHECK(members && members->size() == 2 && (*members)[0] != (*members)[1],
          "the panoramic group preserves both original ordinals");
  }
  const Render::CameraBasis back{.EyeM = eye, .Forward = {{0, 0, 1}}, .Right = {{-1, 0, 0}}};
  const auto directional = sources->SelectGeometry(back, budget);
  CHECK(directional && directional->Products.size() == 2 &&
            directional->Products[0].Representation == Selection::Kind::Geometry,
        "a single directional interval cannot qualify the same sources behind its near plane");
  const auto cohorts = sources->SelectGeometry(eye, {{1280, 720}}, budget, 0, true);
  CHECK(cohorts && cohorts->Products.size() == 2,
        "optional producer cohorts remain independent of the panoramic projection domain");
  const auto hull = sources->SelectGeometry(Vec3{{0, 0, 100000}}, {{1280, 720}}, budget);
  CHECK(hull && hull->Products.size() == 1 &&
            hull->Products[0].Representation == Selection::Kind::Hull,
        "far bounding proxies consolidate the complete source group");
}

void CheckProjectionLimits() {
  const auto known = Sources();
  const auto unknown = Sources(false);
  if (!known || !unknown) {
    CHECK(false, "geometry fixtures prepare");
    return;
  }
  const Vec3 eye{{0, 0, 1000}};
  const ProjectedErrorBudget budget{.FocalPx = 720, .AllowedErrorPx = 1};
  const auto near = known->SelectGeometry(eye, {{1280, 720}}, budget, 1000);
  const auto noError = unknown->SelectGeometry(eye, {{1280, 720}}, budget);
  const auto wide = known->SelectGeometry(eye, {{10000, 10000}}, budget);
  for (const auto *cut : {&near, &noError, &wide}) {
    CHECK(*cut && (*cut)->Products.size() == 2 &&
              std::ranges::all_of(
                  (*cut)->Products,
                  [](const auto &p) { return p.Representation == Selection::Kind::Geometry; }),
          "approach, unknown error and wide viewports cannot silently qualify coarse geometry");
  }
  CHECK(!known->SelectGeometry(eye, {{0, 720}}, budget), "zero viewport dimensions are invalid");
  CHECK(!known->SelectGeometry(eye, {{1280, 720}}, budget, -1),
        "negative translation domains are invalid");
  CHECK(!known->SelectGeometry(eye, {{1280, std::numeric_limits<double>::infinity()}}, budget),
        "nonfinite viewport dimensions are invalid");
}
}

int main() {
  CheckRotationCoverage();
  CheckProjectionLimits();
  return outshine::Test::Report();
}
