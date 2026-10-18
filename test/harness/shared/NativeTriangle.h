#ifndef OUTSHINE_TEST_NATIVETRIANGLE_H
#define OUTSHINE_TEST_NATIVETRIANGLE_H

#include <Outshine.h>
#include <array>
#include <cstdint>
#include <string_view>

namespace outshine::Test {

class NativeTriangle final : public Generators::Generator {
public:
  [[nodiscard]] std::string_view kind() const override { return "camera-triangle"; }

  [[nodiscard]] Generator::Product make(const Generators::Request &) const override {
    Geometry geometry;
    Material material;
    material.BaseColour = {{1, 0.25F, 0.05F, 1}};
    material.Unlit = true;
    const auto surface = geometry.addSurface("triangle", material);
    if (!surface) { return std::unexpected("native fixture material failed"); }
    const auto part = geometry.addPart(kind(), *surface);
    if (!part ||
        !geometry.setPositions(*part, std::array<float, 9>{-1, -1, -2, 1, -1, -2, 0, 1, -2}) ||
        !geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2})) {
      return std::unexpected("native fixture geometry failed");
    }
    return geometry;
  }
};

}
#endif
