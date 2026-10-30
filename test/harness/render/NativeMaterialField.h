#ifndef OUTSHINE_TEST_RENDER_NATIVEMATERIALFIELD_H
#define OUTSHINE_TEST_RENDER_NATIVEMATERIALFIELD_H
#include "MaterialFieldInput.h"
#include "ImpostorAtlas.h"
#include "NativeGpuCheck.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace outshine::Test::Gpu {
inline Geometry MaterialFieldQuad(std::array<float, 4> colour, bool unlit = false) {
  Geometry geometry;
  Material row;
  row.BaseColour = {{colour[0], colour[1], colour[2], colour[3]}};
  row.Unlit = unlit;
  row.DoubleSided = true;
  row.Roughness = .75f;
  auto material = geometry.addSurface("field", row);
  CHECK(material, "the field owns its physical material");
  if (!material) { Failed(); }
  auto part = geometry.addPart("quad", *material);
  CHECK(part, "the field owns one part");
  if (!part) { Failed(); }
  CHECK(geometry.setPositions(
            *part, std::array<float, 12>{-.5f, -.5f, 0, .5f, -.5f, 0, .5f, .5f, 0, -.5f, .5f, 0}) &&
            geometry.setNormals(*part, std::array<float, 12>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}) &&
            geometry.setTriangles(*part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}),
        "the material field has complete native geometry");
  return geometry;
}

inline std::shared_ptr<const Render::MaterialFieldInput> PreparedStoredMaterialField() {
  ImpostorAtlas::View view;
  view.TowardEye = {{0, 0, 1}};
  view.Texels.resize(size_t{16} * 16, {.Normal = {{0, 0, 1}}, .Depth = .5f, .Surface = 1});
  std::string error;
  auto atlas = ImpostorAtlas::Create(16, {}, 1, {Material{}}, {std::move(view)}, error);
  CHECK(atlas, "stored fixture has valid source pixels");
  if (!atlas) { Failed(); }
  auto input = Render::MaterialFieldInput::Prepare(*atlas, 0, 16384);
  CHECK(input, "stored fixture prepares without source geometry");
  if (!input) { Failed(); }
  return *input;
}

inline std::shared_ptr<const Render::MaterialFieldInput>
PreparedMaterialField(std::array<float, 4> colour, bool unlit = false) {
  auto input = Render::MaterialFieldInput::Prepare(MaterialFieldQuad(colour, unlit),
                                                   {.WidthPx = 32, .HeightPx = 32});
  CHECK(input, "worker-side field preparation succeeds");
  if (!input) { Failed(); }
  return *input;
}
}
#endif
