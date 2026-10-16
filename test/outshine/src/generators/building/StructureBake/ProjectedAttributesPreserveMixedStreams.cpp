#include "StructurePlanSelection.h"
#include "Check.h"
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kSources = 16;
constexpr size_t kTriangleVertices = 3;
constexpr float kPrefixValue = 0.25f, kAddedValue = 0.5f;

class Scratch final : public MeshScratch {};

class AttributeMesher final : public StructureMesher {
public:
  explicit AttributeMesher(bool supplied) : Supplied_(supplied) {}

  std::unique_ptr<MeshScratch> Scratch() const override { return std::make_unique<::Scratch>(); }

  bool HasSurfaceProjection() const noexcept override { return true; }

  std::optional<Box> SourceEnvelopeBounds(const StructurePlan &,
                                          MeshScratch &) const noexcept override {
    return Box{.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}};
  }

  std::optional<double> ShellSurfaceErrorM(const StructurePlan &,
                                           MeshScratch &) const noexcept override {
    return 0;
  }

  std::expected<void, StructureMeshError>
  Mesh(const StructurePlan &, MeshScratch &, Raised &) const noexcept override {
    return std::unexpected(StructureMeshError::BuildFailed);
  }

  std::expected<ProjectedStructureMesh, StructureMeshError>
  Project(std::span<const StructurePlan> plans,
          const Vec3 &,
          ProjectedErrorBudget,
          MeshScratch &) const override {
    CHECK(plans.size() == kSources, "the public planner selected the complete projected group");
    ProjectedStructureMesh result;
    result.Mesh.WallCorners.assign(kTriangleVertices, StoredVertex{});
    result.Mesh.WallRun = {0, 1, 2};
    if (Supplied_) {
      result.Mesh.WallColours.assign(kTriangleVertices * 4, kAddedValue);
      result.Mesh.WallMaterialM.assign(kTriangleVertices * 2, kAddedValue);
    }
    return result;
  }

private:
  bool Supplied_;
};

void CheckAppend(bool prefixSupplied, bool addedSupplied) {
  const AttributeMesher mesher(addedSupplied);
  auto scratch = mesher.Scratch();
  RawTile raw;
  raw.EyeEcef = Vec3{{0, 0, 10}};
  raw.Projection = {.FocalPx = 1, .AllowedErrorPx = 1};
  StructurePlanSelection selection;
  BakedTile tile;
  tile.FootprintDetails.resize(kSources);
  tile.Built.WallCorners.assign(kTriangleVertices, StoredVertex{});
  tile.Built.WallRun = {0, 1, 2};
  if (prefixSupplied) {
    tile.Built.WallColours.assign(kTriangleVertices * 4, kPrefixValue);
    tile.Built.WallMaterialM.assign(kTriangleVertices * 2, kPrefixValue);
  }
  for (size_t at = 0; at < kSources; ++at) {
    CHECK(selection.Add({}, {}, at, mesher, *scratch).has_value(),
          "the controlled source has a known projection error");
  }
  CHECK(selection.Select(raw, mesher, *scratch, tile, nullptr), "the public selection succeeds");
  const auto complete = selection.Emit(raw, mesher, *scratch, tile, 1, nullptr);
  CHECK(complete && *complete, "one public emission appends the projected product");
  CHECK(tile.Built.WallCorners.size() == 2 * kTriangleVertices &&
            tile.Built.WallRun == std::vector<uint32_t>({0, 1, 2, 3, 4, 5}),
        "existing and appended geometry retain their indices");
  for (size_t components : {size_t{2}, size_t{4}}) {
    const auto &values = components == 2 ? tile.Built.WallMaterialM : tile.Built.WallColours;
    if (!prefixSupplied && !addedSupplied) {
      CHECK(values.empty(), "absent optional streams allocate no neutral array");
      continue;
    }
    CHECK(values.size() == 2 * kTriangleVertices * components,
          "both ranges have complete independent attribute elements");
    if (values.size() != 2 * kTriangleVertices * components) { continue; }
    const float missing = components == 2 ? 0.0f : 1.0f;
    const std::span stream(values);
    CHECK(std::ranges::all_of(
              stream.first(kTriangleVertices * components),
              [&](float value) { return value == (prefixSupplied ? kPrefixValue : missing); }),
          "the prefix is retained or filled with the attribute's neutral value");
    CHECK(std::ranges::all_of(
              stream.last(kTriangleVertices * components),
              [&](float value) { return value == (addedSupplied ? kAddedValue : missing); }),
          "the appended stream is retained or filled without reusing prefix data");
  }
}
}

int main() {
  for (const bool prefix : {false, true}) {
    for (const bool added : {false, true}) { CheckAppend(prefix, added); }
  }
  return outshine::Test::Report();
}
