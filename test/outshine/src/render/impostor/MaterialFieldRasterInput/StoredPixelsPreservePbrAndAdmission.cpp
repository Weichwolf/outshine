#include "MaterialFieldRasterInput.h"
#include "ImpostorAtlas.h"
#include "math/Octahedral.h"
#include "Check.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

std::optional<ImpostorAtlas> Source(bool covered, bool sampled, Material material = {}) {
  ImpostorAtlas::View view;
  view.TowardEye = {{0, 0, 1}};
  view.Texels.resize(9);
  if (covered) { view.Texels[5] = {.Normal = {{.6f, .8f, 0}}, .Depth = .72345679f, .Surface = 1}; }
  if (sampled) {
    view.Materials.resize(9);
    view.Materials[5] = {.BaseColour = {{.12345679f, .23456789f, .3456789f}},
                         .Roughness = .456789f,
                         .Metalness = .567891f};
  }
  std::string error;
  return ImpostorAtlas::Create(3, {{10, 20, 30}}, 6, {material}, {std::move(view)}, error);
}

void Exercise(bool sampled) {
  const auto atlas = Source(true, sampled);
  CHECK(atlas, "independent source forms a valid atlas");
  if (!atlas) { return; }
  constexpr size_t bytes = 9 * 32 + 16;
  CHECK(!MaterialFieldRasterInput::Prepare(*atlas, {.View = 0, .BytesMost = bytes - 1}) &&
            !MaterialFieldRasterInput::Prepare(*atlas, {.View = 1, .BytesMost = bytes}),
        "payload capacity and view identity refuse before raster construction");
  const auto input = MaterialFieldRasterInput::Prepare(*atlas, {.View = 0, .BytesMost = bytes});
  CHECK(input && input->Words.size() * sizeof(input->Words[0]) == bytes,
        "exact admitted storage contains pixels and one material without mesh data");
  if (!input) { return; }
  const auto &first = input->Words[10].Values;
  const auto &second = input->Words[11].Values;
  const auto expected =
      sampled ? atlas->Views()[0].Materials[5]
              : ImpostorAtlas::SurfaceSample{.BaseColour = {{.5f, .5f, .5f}}, .Roughness = 1};
  CHECK(first[3] == 1 && std::bit_cast<float>(first[1]) == .72345679f &&
            std::bit_cast<float>(first[2]) == expected.Metalness,
        "stored identity, depth and metalness retain original values");
  CHECK((second == std::array{std::bit_cast<uint32_t>(expected.BaseColour[0]),
                              std::bit_cast<uint32_t>(expected.BaseColour[1]),
                              std::bit_cast<uint32_t>(expected.BaseColour[2]),
                              std::bit_cast<uint32_t>(expected.Roughness)}),
        "sampled and unsampled PBR values retain all Float32 bits");
  const auto normal = OctUnfolded(UnpackedPair(first[0]));
  CHECK(std::abs(normal[0] - .6f) < .0001f && std::abs(normal[1] - .8f) < .0001f &&
            std::abs(normal[2]) < .0001f,
        "normal retains the existing GPU octahedral precision");
  for (size_t at = 0; at < 9; ++at) {
    if (at != 5) {
      CHECK((input->Words[2 * at].Values == std::array<uint32_t, 4>{} &&
             input->Words[2 * at + 1].Values == std::array<uint32_t, 4>{}),
            "misses remain empty rather than becoming lit surfaces");
    }
  }
  CHECK(input->Capture.EyeM == (Vec3{{10, 20, 48}}) &&
            input->Capture.Kind == CameraKind::Orthographic && input->Capture.XMagM == 6 &&
            input->Capture.ZNearM == 6 && input->Capture.ZFarM == 30,
        "source rays retain their independently derived orthographic capture volume");
}
}

int main() {
  Exercise(false);
  Exercise(true);
  const auto empty = Source(false, false);
  CHECK(empty && !MaterialFieldRasterInput::Prepare(*empty, {.View = 0, .BytesMost = 1024}),
        "a fully empty capture cannot publish a material field");
  Material unsupported;
  unsupported.Emission = {{1, 0, 0}};
  const auto emissive = Source(true, false, unsupported);
  CHECK(emissive && !MaterialFieldRasterInput::Prepare(*emissive, {.View = 0, .BytesMost = 1024}),
        "unsupported source emission is rejected rather than dropped");
  unsupported = {};
  unsupported.Scattering = ScatteringModel::AreaMixture;
  CHECK(!Source(true, false, unsupported), "mixture moments are not a valid source atlas material");
  return Report();
}
