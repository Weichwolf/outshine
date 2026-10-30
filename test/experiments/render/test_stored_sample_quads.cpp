#include "test/harness/render/StoredSampleQuads.h"
#include "ImpostorAtlas.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

std::optional<ImpostorAtlas> Source() {
  std::vector<Material> materials(2);
  ImpostorAtlas::View view;
  view.TowardEye = {{0, 0, 1}};
  view.Texels.resize(9);
  view.Materials.resize(9);
  view.Texels[0] = {.Normal = {{.6f, .8f, 0}}, .Depth = .12345679f, .Surface = 1};
  view.Materials[0] = {.BaseColour = {{.12345679f, .23456789f, .3456789f}},
                       .Roughness = .456789f,
                       .Metalness = .567891f};
  view.Texels[5] = {.Normal = {{0, 1, 0}}, .Depth = .72345679f, .Surface = 2};
  view.Materials[5] = {.BaseColour = {{.81234567f, .72345678f, .63456789f}},
                       .Roughness = .5456789f,
                       .Metalness = .3567891f};
  std::string error;
  return ImpostorAtlas::Create(
      3, {{10, 20, 30}}, 6, std::move(materials), {std::move(view)}, error);
}

std::array<float, 4> Pixel(ImageView image, size_t pixel) {
  std::array<float, 4> values{};
  std::memcpy(values.data(), image.Rgba.data() + pixel * sizeof(values), sizeof(values));
  return values;
}
}

int main() {
  const auto atlas = Source();
  CHECK(atlas.has_value(), "independent coupled Float32 samples form a valid source atlas");
  if (!atlas) { return Report(); }
  constexpr size_t imageBytes = size_t{9} * 2 * 4 * sizeof(float);
  constexpr size_t meshBytes =
      size_t{2} * (size_t{4} * (3 + 3 + 2) * sizeof(float) + 6 * sizeof(uint32_t));
  constexpr size_t bytes = imageBytes + meshBytes;
  CHECK(!StoredSampleQuads::Prepare(*atlas, {.View = 0, .SourceBytesMost = bytes - 1}) &&
            !StoredSampleQuads::Prepare(*atlas, {.View = 1, .SourceBytesMost = bytes}),
        "source payload admission and view identity are checked before construction");
  const auto input = StoredSampleQuads::Prepare(*atlas, {.View = 0, .SourceBytesMost = bytes});
  CHECK(input && input->SourceBytes == bytes, "exact logical source payload admission succeeds");
  if (!input) { return Report(); }
  const auto &geometry = input->Source;
  CHECK(geometry.parts() == 2 && geometry.images() == 2,
        "source samples batch by material and share two floating images");
  for (const size_t pixel : {size_t{0}, size_t{5}}) {
    const auto &sample = atlas->Views()[0].Materials[pixel];
    const auto &texel = atlas->Views()[0].Texels[pixel];
    const auto colour = Pixel(geometry.imageAt(0), pixel);
    const auto parameters = Pixel(geometry.imageAt(1), pixel);
    const std::array<float, 4> expectedColour{
        sample.BaseColour[0], sample.BaseColour[1], sample.BaseColour[2], 1};
    const std::array<float, 4> expectedParameters{1, sample.Roughness, sample.Metalness, 1};
    CHECK(colour == expectedColour && parameters == expectedParameters,
          "PBR values retain every stored Float32 bit before lighting");
    const int part = static_cast<int>(texel.Surface) - 1;
    const auto positions = geometry.positionsOf(part);
    const auto normals = geometry.normalsOf(part);
    const auto depth = static_cast<float>(30 + (4.0 * texel.Depth - 2) * 6);
    CHECK(geometry.trianglesOf(part).size() == 6 && positions.size() == 12 && normals.size() == 12,
          "each covered original point owns one exact source pixel quad");
    for (size_t corner = 0; corner < 4; ++corner) {
      CHECK(positions[corner * 3 + 2] == depth &&
                std::ranges::equal(normals.subspan(corner * 3, 3), texel.Normal),
            "source depth and normals are not fitted, byte-packed or interpolated across samples");
    }
    const auto &material = geometry.surfaceAt(MaterialInstance(part));
    CHECK(!material.NormalMap.bound() && material.BaseColourMap.Sampler.Mip == MipFilter::None &&
              material.BaseColourMap.Sampler.Minify == Filter::Nearest &&
              material.MetalRoughMap.Sampler.Magnify == Filter::Nearest,
          "source relighting uses vertex normals and unfiltered native material samples");
  }
  CHECK(geometry.imageAt(0).Format == ImagePixelFormat::Rgba32Float &&
            geometry.imageAt(1).Format == ImagePixelFormat::Rgba32Float &&
            input->Capture.Kind == CameraKind::Orthographic && input->Capture.XMagM == 6,
        "floating image semantics and the original orthographic capture remain explicit");
  return Report();
}
