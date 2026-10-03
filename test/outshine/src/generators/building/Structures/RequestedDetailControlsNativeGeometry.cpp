#include "Structures.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace {
size_t Vertices(const outshine::Geometry &geometry) {
  size_t count = 0;
  for (int part = 0; part < geometry.parts(); ++part) {
    count += geometry.positionsOf(part).size() / 3;
  }
  return count;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Generators::Structures producer;
  const std::array<Generators::Parameter, 1> parameters{{{.Name = "widthM", .Value = "24"}}};
  Generators::Request request;
  request.Parameters = parameters;
  request.Seed = 17;
  const auto fine = producer.make(request);
  CHECK(fine.has_value(), "default produces the simple building envelope");
  if (!fine) { return Report(); }
  request.Coarseness = LevelOfDetail::Fine;
  const auto explicitFine = producer.make(request);
  CHECK(explicitFine && explicitFine->parts() == fine->parts(), "default detail is Fine");
  for (int part = 0; explicitFine && part < fine->parts() && part < explicitFine->parts(); ++part) {
    CHECK(std::ranges::equal(fine->positionsOf(part), explicitFine->positionsOf(part)),
          "explicit Fine retains default geometry");
  }
  size_t previous = Vertices(*fine);
  for (const auto detail : {LevelOfDetail::Shell, LevelOfDetail::Massed, LevelOfDetail::Skyline}) {
    request.Coarseness = detail;
    const auto product = producer.make(request);
    CHECK(product && product->parts() > 0, "each supported detail retains the building");
    if (!product) { continue; }
    const size_t vertices = Vertices(*product);
    CHECK(vertices > 0 && vertices <= Vertices(*fine),
          "coarse requests never introduce secondary geometry");
    CHECK(vertices <= previous, "coarser detail does not increase geometric work");
    previous = vertices;
  }
  request.Coarseness = static_cast<LevelOfDetail>(uint8_t{255});
  CHECK(!producer.make(request), "unknown detail classes are refused");
  return Report();
}
