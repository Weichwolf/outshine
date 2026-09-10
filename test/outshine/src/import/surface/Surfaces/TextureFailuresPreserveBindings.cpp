#include "Surfaces.h"
#include "Check.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto root =
      std::filesystem::temp_directory_path() / ("outshine-texture-" + std::to_string(getpid()));
  std::filesystem::create_directories(root);
  const std::array<uint8_t, 4> pixel{31, 67, 127, 255};
  std::vector<uint8_t> png;
  CHECK(Core::EncodePng(pixel.data(), 1, 1, png), "fixture PNG encoded");
  {
    std::ofstream output(root / "pixel.png", std::ios::binary);
    output.write(reinterpret_cast<const char *>(png.data()),
                 static_cast<std::streamsize>(png.size()));
  }
  {
    std::ofstream output(root / "textured.gltf");
    output << R"({"asset":{"version":"2.0"},"images":[{"uri":"pixel.png"}],
      "samplers":[{"magFilter":9728,"minFilter":9984,"wrapS":33071,"wrapT":33648}],
      "textures":[{"source":0,"sampler":0}],"materials":[{
      "pbrMetallicRoughness":{"baseColorTexture":{"index":0}},
      "normalTexture":{"index":0,"scale":0.5}}]})";
  }
  {
    std::ofstream output(root / "plain.gltf");
    output << R"({"asset":{"version":"2.0"},"materials":[{}]})";
  }
  Gltf::Document textured, plain;
  CHECK(textured.ReadFile((root / "textured.gltf").string()), textured.Error().c_str());
  CHECK(plain.ReadFile((root / "plain.gltf").string()), plain.Error().c_str());
  Geometry geometry;
  const int part = geometry.addPart("triangle", {});
  CHECK(geometry.setPositions(part, std::array{0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f}) &&
            geometry.setTriangles(part, std::array{0u, 1u, 2u}) &&
            geometry.setTexture(part, std::array{0.f, 0.f, 1.f, 0.f, 0.f, 1.f}),
        "independent native geometry prepared");
  Gltf::Subject subject;
  CHECK(subject.Assemble(geometry), "native geometry assembled for UV contract");
  Render::SurfaceTable table;
  table.Slots.resize(1);
  table.Material = {0};
  table.PartSlot = {0};
  std::string error;
  CHECK(
      Gltf::ResolveFileSurface(
          textured, subject, Render::ColourFrom::Row, Render::ColourCarrier::Texture, table, error),
      error.c_str());
  if (table.Decoded.empty() || !table.Decoded[0].Colour.Holds()) { return Report(); }
  CHECK(table.Slots[0].Colour.Rgba == table.Decoded[0].Colour.Rgba.data() &&
            table.Slots[0].Normal.Rgba == table.Decoded[0].Normal.Rgba.data(),
        "published bindings point into their owned rasters");
  CHECK(table.Slots[0].Colour.WrapU == Render::SubjectWrap::ClampToEdge &&
            table.Slots[0].Colour.WrapV == Render::SubjectWrap::MirroredRepeat &&
            table.Slots[0].Colour.Mip == Render::SubjectMip::Nearest &&
            table.Slots[0].Colour.Minify == Render::SubjectFilter::Nearest &&
            table.Slots[0].NormalScale == 0.5f,
        "sampler and material properties preserved");
  table.Decoded[0].Colour.Rgba[0] = 77;
  const auto *previous = table.Decoded[0].Colour.Rgba.data();
  CHECK(
      !Gltf::ResolveFileSurface(
          textured, subject, Render::ColourFrom::Row, Render::ColourCarrier::Factor, table, error),
      "late carrier conflict rejected");
  CHECK(table.Decoded[0].Colour.Rgba.data() == previous && table.Decoded[0].Colour.Rgba[0] == 77 &&
            table.Slots[0].Colour.Rgba == previous,
        "late failure preserves owned pixels and borrowed binding");
  CHECK(Gltf::ResolveFileSurface(
            plain, subject, Render::ColourFrom::Row, Render::ColourCarrier::Factor, table, error),
        error.c_str());
  CHECK(!table.Slots[0].ReadsAnyImage() && table.Slots[0].NormalScale == 1,
        "successful untextured replacement clears previous image bindings and normal scale");
  for (const bool invalidPart : {false, true}) {
    auto invalid = table;
    if (invalidPart) {
      invalid.PartSlot = {1};
    } else {
      invalid.Material.clear();
    }
    const auto *slots = invalid.Slots.data();
    CHECK(
        !Gltf::ResolveFileSurface(
            plain, subject, Render::ColourFrom::Row, Render::ColourCarrier::Factor, invalid, error),
        "inconsistent table indices rejected before access");
    CHECK(invalid.Slots.data() == slots, "invalid table preserves previous storage");
  }
  std::filesystem::remove_all(root);
  return Report();
}
