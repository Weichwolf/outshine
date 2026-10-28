#include "RenderScene.h"
#include "Check.h"
#include "GeometryRenderer.h"
#include "Geometry.h"
#include "Material.h"
#include "MaterialRegistration.h"
#include "Gpu.h"
#include "SceneResources.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOfflineDevice.h"
#include "test/harness/render/NativeUploadFaultDevice.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

void Configure(GeometryRenderer &draw, Native::Device &device) {
  Render::Gpu gpu;
  gpu.Device = &device;
  gpu.HdrFormat = Native::Format::Rgba16Float;
  gpu.SurfaceFormat = Native::Format::Rgba8Unorm;
  gpu.Width = gpu.Height = 32;
  CHECK(gpu.SceneColours.Add(Resource::SceneLinear), "the fixture has one HDR attachment");
  Require(draw.Configure(gpu));
}

std::array<MaterialRegistration, 4> RegisterMaterials(SceneResources &resources,
                                                      GeometryRenderer &draw) {
  std::array<Material, 4> materials{};
  materials[1].Alpha = AlphaMode::Masked;
  materials[2].Alpha = AlphaMode::Blended;
  materials[3].Unlit = true;
  std::array<MaterialRegistration, 4> registrations{};
  for (size_t at = 0; at < materials.size(); ++at) {
    Geometry source;
    const auto added = source.addSurface("prism material", materials[at]);
    CHECK(added, "the actual prism material source has one surface");
    if (!added) { Failed(); }
    auto registered = resources.RegisterPieceMaterials(draw, nullptr, std::move(source));
    CHECK(registered, "the actual prism material source registers");
    if (!registered) { Failed(); }
    registrations[at] = std::move(*registered);
  }
  return registrations;
}

std::shared_ptr<const PrismGrid> Source() {
  return std::make_shared<const PrismGrid>(
      PrismGrid{.Samples = std::make_shared<const std::vector<PrismSpan>>(
                    std::vector<PrismSpan>{{.Lower = -2, .Upper = 6}, {.Lower = 0, .Upper = 0}}),
                .Patches = {{.Columns = 2, .Rows = 1, .PitchX = 2, .PitchZ = 3}}});
}

void RejectedReplacement(SceneResources &resources,
                         GeometryRenderer &draw,
                         PrismGridHandle handle,
                         const std::shared_ptr<const PrismGrid> &source,
                         UploadFailure &failure,
                         std::span<const PrismInstance> rows,
                         const MaterialRegistration &material) {
  const auto generation = draw.Generation();
  const auto old = draw.Prisms().Products()[0].Id;
  const auto invalid = std::make_shared<PrismGrid>(*source);
  invalid->Patches[0].Columns = 0;
  CHECK(!resources.ReplacePrismGrid(draw, handle, invalid, rows, material),
        "an invalid source cannot replace the current logical or GPU resource");
  const auto replacement = std::make_shared<const PrismGrid>(*source);
  failure.Armed = true;
  const auto rejected = resources.ReplacePrismGrid(draw, handle, replacement, rows, material);
  failure.Armed = false;
  CHECK(!rejected && rejected.error().Kind == Native::ErrorKind::Driver,
        "a rejected native upload preserves its actual error");
  CHECK(resources.HasPrismGrid(handle) && draw.Prisms().Products().size() == 1 &&
            draw.Prisms().Products()[0].Id == old && draw.Generation() == generation,
        "failed replacements keep the original identity, source and shadow revision");
}

void Probe(Native::Device &device, UploadFailure &failure) {
  RenderScene publishedStorage;
  GeometryRenderer published{publishedStorage};
  RenderScene candidateStorage;
  GeometryRenderer candidate{candidateStorage};
  Configure(published, device);
  Configure(candidate, device);
  SceneResources original;
  SceneResources copied;
  const auto materials = RegisterMaterials(original, published);
  const auto &opaque = materials[0];
  const auto source = Source();
  std::array<PrismInstance, 1> rows{};
  for (const uint32_t slot : {1u, 2u, 3u}) {
    const auto rejected = original.PlacePrismGrid(published, source, rows, materials[slot]);
    CHECK(!rejected && rejected.error().Kind == Native::ErrorKind::Unsupported,
          "masked, blended and unlit materials are rejected before native upload");
  }
  CHECK(
      !original.PlacePrismGrid(
          published, source, rows, {.First = std::numeric_limits<uint32_t>::max(), .Source = {}}) &&
          published.Prisms().Products().empty(),
      "unregistered materials cannot allocate logical or native prism products");
  const auto first = Require(original.PlacePrismGrid(published, source, rows, opaque));
  const auto live = Require(original.PlacePrismGrid(published, source, rows, opaque));
  original.ReleasePrismGrid(published, first);
  copied.CopySourcesFrom(original, SceneResources::PieceSources::Copy);
  CHECK(copied.HasPrismGrid(live) && !copied.HasPrismGrid(first),
        "a snapshot preserves live and released logical generations");
  std::string error;
  CHECK(copied.RestorePieceMaterials(candidate, nullptr, error),
        "copied material sources restore before native prism resources");
  size_t next = 0;
  const auto firstSlice = copied.AdvancePieceRestore(candidate, next, 1);
  CHECK(firstSlice && !*firstSlice && next == 1 && candidate.Prisms().Products().empty(),
        "bounded restoration accounts for the copied free slot without uploading it");
  const auto finalSlice = copied.AdvancePieceRestore(candidate, next, 1);
  CHECK(finalSlice && *finalSlice && next == 2,
        "the existing native restore phase also completes prism resources");
  CHECK(candidate.Prisms().Products().size() == 1 &&
            candidate.Prisms().Products()[0].Source == source &&
            candidate.Prisms().Products()[0].Id != published.Prisms().Products()[0].Id,
        "shared CPU data restores behind the same handle despite compacted native identities");
  const auto stable = candidate.Generation();
  Require(copied.SetPrismGridInstances(candidate, live, rows));
  CHECK(candidate.Generation() == stable, "unchanged instances preserve shadow reuse");
  RejectedReplacement(copied, candidate, live, source, failure, rows, opaque);
  const auto replacement = std::make_shared<PrismGrid>(*source);
  replacement->Samples = std::make_shared<const std::vector<PrismSpan>>(
      std::vector<PrismSpan>{{.Lower = -1, .Upper = 8}, {.Lower = 0, .Upper = 0}});
  Require(copied.ReplacePrismGrid(candidate, live, replacement, rows, opaque));
  CHECK(copied.HasPrismGrid(live) && candidate.Prisms().Products().size() == 1 &&
            candidate.Prisms().Products()[0].Source == replacement,
        "a complete replacement preserves the logical handle and publishes its new source");
  const auto beforeMotion = candidate.Generation();
  rows[0].Current[12] = 2;
  Require(copied.SetPrismGridInstances(candidate, live, rows));
  CHECK(candidate.Generation() != beforeMotion, "changed instances invalidate cached casters");
  copied.ReleasePrismGrid(candidate, live);
  const auto reused = Require(copied.PlacePrismGrid(candidate, source, rows, opaque));
  CHECK(reused.Slot == live.Slot && reused.Generation == live.Generation + 1 &&
            !copied.SetPrismGridInstances(candidate, live, rows),
        "a recycled slot cannot be mutated through its stale handle");
  SceneResources omitted;
  omitted.CopySourcesFrom(copied, SceneResources::PieceSources::Omit);
  CHECK(!omitted.HasPrismGrid(reused), "omitting native geometry also omits prism sources");
}
}

int main() {
  auto actual = Require(Native::CreateDevice({.Validation = true}));
  UploadFailure failure;
  NativeUploadFaultDevice failed(std::move(actual), failure);
  NativeOfflineDevice device(failed);
  Probe(device, failure);
  return Report();
}
