#include "SourceProductSelection.h"
#include "PlanHierarchy.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <span>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::array<Box, 4> bounds;
  for (size_t at = 0; at < bounds.size(); ++at) {
    bounds[at] = {.Min = {{static_cast<double>(at) * 10, 0, 0}},
                  .Max = {{static_cast<double>(at) * 10 + 2, 2, 2}}};
  }
  auto built = PlanHierarchy::Build(bounds);
  CHECK(built, "four sources have a valid immutable hierarchy");
  if (!built) { return Report(); }
  const auto hierarchy = std::make_shared<const PlanHierarchy>(std::move(*built));
  std::array<SourceProductSelection::Source, 4> sources;
  sources.fill({.Cohort = 1000, .FieldSupported = true});
  auto selected = SourceProductSelection::Of(hierarchy, sources, "source-revision");
  CHECK(selected, "opaque cohort tags need no World building-cell limit");
  if (!selected) { return Report(); }
  const ProjectedErrorBudget projection{.FocalPx = 720, .AllowedErrorPx = 1};
  auto near = selected->Select({{15, 1, 1}}, projection);
  auto far = selected->Select({{15, 1, 100000}}, projection);
  CHECK(near && near->Products.size() == 4 && far && far->Products.size() == 1,
        "near sources remain geometry while the distant root is one field candidate");
  if (!near || !far) { return Report(); }
  CHECK(std::ranges::all_of(near->Products,
                            [](const auto &product) {
                              return product.Representation ==
                                     SourceProductSelection::Kind::Geometry;
                            }) &&
            far->Products[0].Representation == SourceProductSelection::Kind::MaterialField,
        "candidate representations follow the declared centred projection budget");
  const auto members = selected->Members(*far, far->Products.front());
  CHECK(members && members->size() == 4, "one root retains all four original source ordinals");
  far->Revision = "foreign";
  CHECK(!selected->Members(*far, far->Products.front()),
        "another source revision grants no member access");
  sources[2].Cohort = sources[3].Cohort = 2000;
  auto cohorts = SourceProductSelection::Of(hierarchy, sources, "cohorts");
  const auto split = cohorts->Select({{15, 1, 100000}}, projection);
  CHECK(split && split->Products.size() == 2, "independent producer cohorts stay separate");
  sources[1].FieldSupported = false;
  auto mixed = SourceProductSelection::Of(hierarchy, sources, "unknown");
  const auto retained = mixed->Select({{15, 1, 100000}}, projection);
  CHECK(retained && std::ranges::count_if(retained->Products,
                                          [](const auto &product) {
                                            return product.Representation ==
                                                   SourceProductSelection::Kind::Unresolved;
                                          }) == 1,
        "unresolved sources remain explicit beside supported field candidates");
  const auto exact = selected->Select({{15, 1, 100000}}, {.FocalPx = 720, .AllowedErrorPx = 0});
  CHECK(exact && std::ranges::all_of(exact->Products,
                                     [](const auto &product) {
                                       return product.Representation ==
                                              SourceProductSelection::Kind::Geometry;
                                     }),
        "zero allowed error never admits an approximate field");
  CHECK(!selected->Select({}, {.FocalPx = 0, .AllowedErrorPx = 1}) &&
            !SourceProductSelection::Of(hierarchy, std::span(sources).first(3), "missing"),
        "invalid projection and incomplete source traits are rejected");
  return Report();
}
