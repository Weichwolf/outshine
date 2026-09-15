#include <algorithm>
#include <array>
#include <vector>

#include <SDL3/SDL.h>
#include <Outshine.h>
#include <generation/Generate.h>
#include "Check.h"

namespace {
class Product final : public outshine::Generators::Generator {
public:
  Product(std::string_view named, float centreX, const std::array<float, 3> &colour)
      : Name_(named), CentreX_(centreX), Colour_(colour) {}

  [[nodiscard]] std::string_view kind() const override { return Name_; }

  [[nodiscard]] Generator::Product make(const outshine::Generators::Request &) const override {
    outshine::Geometry geometry;
    outshine::Material material;
    material.BaseColour = {{Colour_[0], Colour_[1], Colour_[2], 1}};
    material.Unlit = true;
    const auto surface = geometry.addSurface(Name_, material);
    if (!surface) { return std::unexpected("could not create product material"); }
    const int part = geometry.addPart(Name_, *surface);
    constexpr float halfWidthM = 0.45F;
    if (!geometry.setPositions(part,
                               std::array<float, 12>{CentreX_ - halfWidthM,
                                                     -0.5F,
                                                     0,
                                                     CentreX_ + halfWidthM,
                                                     -0.5F,
                                                     0,
                                                     CentreX_ + halfWidthM,
                                                     0.5F,
                                                     0,
                                                     CentreX_ - halfWidthM,
                                                     0.5F,
                                                     0}) ||
        !geometry.setNormals(part, std::array<float, 12>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}) ||
        !geometry.setTriangles(part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3})) {
      return std::unexpected("could not create product mesh");
    }
    return geometry;
  }

private:
  std::string Name_;
  float CentreX_;
  std::array<float, 3> Colour_;
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  Product unnamed("", 0.0F, {{0, 0, 0}});
  Product left("left-product", -0.5F, {{1, 0, 0}});
  Product right("right-product", 0.5F, {{0, 1, 0}});
  Engine engine;
  const auto empty = engine.offers(unnamed);
  CHECK(!empty && empty.error() == "generator registration needs a nonempty kind",
        "empty generator kind reports a registration refusal");
  CHECK(engine.offers(left), "left fixture generator registers");
  const auto duplicate = engine.offers(left);
  CHECK(!duplicate && duplicate.error() == "generator kind is already registered",
        "duplicate generator kind reports a registration refusal");
  CHECK(engine.offers(right), "right fixture generator registers");
  Scenario::Document scenario;
  scenario.Render.Declared = true;
  scenario.Render.Frame = {64, 64};
  scenario.Render.Outputs = {"sceneLinear"};
  scenario.Generators = {{.Kind = "left-product"}, {.Kind = "right-product"}};
  Scenario::View view;
  view.Id = "products";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{0, 0, 2}};
  view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = 0.1, .FarM = 10});
  scenario.Views.push_back(view);
  std::vector<float> pixels;
  if (!engine.drawsInto({64, 64}) || !engine.declare(scenario) || !engine.assemble() ||
      !engine.advance() || !engine.renderer().render({}) ||
      !engine.renderer().readPixels(Buffer::Linear, pixels)) {
    Unprepared(engine.error().c_str());
    return Report();
  }
  CHECK(pixels.size() == 64u * 64u * 4u, "generated image dimensions");
  if (pixels.size() != 64u * 64u * 4u) { return Report(); }
  float redMax = 0;
  float greenMax = 0;
  for (size_t at = 0; at < pixels.size(); at += 4) {
    redMax = std::max(redMax, pixels[at]);
    greenMax = std::max(greenMax, pixels[at + 1]);
  }
  CHECK(redMax > 0.9F && greenMax > 0.9F,
        "two products compose with their own remapped native materials");
  return Report();
}
