#include "Check.h"
#include "GeometryHierarchyAdmission.h"
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Test;

Native::Capabilities Limits() {
  return {.Name = "hierarchy-admission-test",
          .MaxStorageBufferBytes = std::numeric_limits<uint32_t>::max(),
          .MaxWorkgroupSize = {1024, 1024, 64},
          .MaxWorkgroupCount = {65535, 65535, 65535},
          .MaxWorkgroupInvocations = 1024};
}

struct Fixture {
  std::array<GeometryRoot, 1> Roots{GeometryRoot{.TemplateCount = 1, .IndexCapacity = 3}};
  std::array<GeometryClusterTemplate, 1> Templates{GeometryClusterTemplate{.IndexCount = 3}};

  [[nodiscard]] GeometryHierarchyInputs Inputs() const {
    return {.Roots = Roots,
            .Templates = Templates,
            .SphereCount = 1,
            .PlacementCount = 1,
            .SourceIndexCount = 3,
            .BatchCount = 1};
  }
};

void RejectSourceErrors() {
  const Fixture valid;
  const auto accepted = AdmitGeometryHierarchy(valid.Inputs(), Limits());
  const std::vector expectedPrefix{0u, 1u};
  CHECK(accepted && accepted->IndexWords == 3 && accepted->View.ClusterCapacity == 1 &&
            accepted->View.WorstChunks == 1 && accepted->ChunkPrefix == expectedPrefix,
        "valid resident ranges establish complete bounded queue and index capacities");
  for (uint32_t which = 0; which < 10; ++which) {
    auto broken = valid;
    switch (which) {
      case 0: broken.Roots[0].TemplateFirst = std::numeric_limits<uint32_t>::max(); break;
      case 1: broken.Roots[0].TemplateCount = std::numeric_limits<uint32_t>::max(); break;
      case 2: broken.Roots[0].LogicalFirst = 1; break;
      case 3: broken.Roots[0].BatchRow = 1; break;
      case 4: broken.Roots[0].ModelSlot = 1; break;
      case 5: broken.Roots[0].IndexBase = std::numeric_limits<uint32_t>::max(); break;
      case 6: broken.Roots[0].IndexCapacity = 2; break;
      case 7: broken.Templates[0].SphereIndex = 1; break;
      case 8: broken.Templates[0].RelativeFirstIndex = std::numeric_limits<uint32_t>::max(); break;
      case 9: broken.Templates[0].IndexCount = 0; break;
      default: CHECK(false, "the explicit mutation index is in range"); break;
    }
    CHECK(!AdmitGeometryHierarchy(broken.Inputs(), Limits()),
          "invalid source intervals and references fail before any GPU allocation or work");
  }
  for (uint32_t which = 0; which < 6; ++which) {
    auto inputs = valid.Inputs();
    switch (which) {
      case 0: inputs.Roots = {}; break;
      case 1: inputs.Templates = {}; break;
      case 2: inputs.SphereCount = 0; break;
      case 3: inputs.PlacementCount = 0; break;
      case 4: inputs.SourceIndexCount = 0; break;
      case 5: inputs.BatchCount = 0; break;
      default: CHECK(false, "the explicit mutation index is in range"); break;
    }
    CHECK(!AdmitGeometryHierarchy(inputs, Limits()), "missing backing ranges are rejected");
  }
}

void RejectDeviceErrors() {
  const Fixture valid;
  for (uint32_t which = 0; which < 7; ++which) {
    auto limits = Limits();
    switch (which) {
      case 0: limits.MaxStorageBufferBytes = 79; break;
      case 1: limits.MaxWorkgroupSize[0] = 63; break;
      case 2: limits.MaxWorkgroupInvocations = 63; break;
      case 3: limits.MaxWorkgroupCount[0] = 0; break;
      case 4: limits.MaxWorkgroupCount[1] = 0; break;
      case 5: limits.MaxWorkgroupCount[2] = 0; break;
      case 6: limits.MaxStorageBufferBytes = 143; break;
      default: CHECK(false, "the explicit mutation index is in range"); break;
    }
    CHECK(!AdmitGeometryHierarchy(valid.Inputs(), limits),
          "device storage and all dispatch axes are admitted before encoding");
  }
}

void RejectAggregateOverflow() {
  Fixture fixture;
  constexpr uint32_t indices = 1048578;
  fixture.Templates[0].IndexCount = indices;
  std::vector<GeometryRoot> roots(1024, fixture.Roots[0]);
  for (uint32_t at = 0; at < roots.size(); ++at) {
    roots[at].LogicalFirst = at;
    roots[at].IndexCapacity = indices;
  }
  auto inputs = fixture.Inputs();
  inputs.Roots = roots;
  inputs.SourceIndexCount = indices;
  CHECK(!AdmitGeometryHierarchy(inputs, Limits()),
        "1024 instances of 1048578 index words cannot wrap 4 GiB into an 8192-byte destination");
  for (auto &root : roots) { root.IndexCapacity = 3; }
  fixture.Templates[0].IndexCount = 3;
  auto narrow = Limits();
  narrow.MaxWorkgroupCount = {7, 1, 1};
  CHECK(!AdmitGeometryHierarchy(inputs, narrow),
        "complete fallback and scan grids must fit the admitted Y axis");
  roots[1].BatchRow = 1;
  inputs.BatchCount = 2;
  CHECK(!AdmitGeometryHierarchy(inputs, Limits()),
        "roots of one draw batch are contiguous and ordered for race-free stable scanning");
}
}

int main() {
  RejectSourceErrors();
  RejectDeviceErrors();
  RejectAggregateOverflow();
  return Report();
}
