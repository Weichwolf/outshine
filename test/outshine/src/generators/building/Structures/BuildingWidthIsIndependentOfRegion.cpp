#include "Structures.h"
#include "Check.h"
#include "export/GltfExporter.h"
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string_view>
#include <utility>

namespace {
bool HasNominalEastSides(const outshine::Geometry &geometry, double width) {
  bool west = false;
  bool east = false;
  for (int part = 0; part < geometry.parts(); ++part) {
    const auto positions = geometry.positionsOf(part);
    for (size_t at = 0; at < positions.size(); at += 3) {
      west = west || std::abs(positions[at] + width / 2.0) < 0.01;
      east = east || std::abs(positions[at] - width / 2.0) < 0.01;
    }
  }
  return west && east;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Generators::Structures producer;
  for (const auto [text, width] :
       {std::pair{"12", 12.0}, std::pair{"24", 24.0}, std::pair{"240", 240.0}}) {
    const std::array<Generators::Parameter, 1> parameters{{{.Name = "widthM", .Value = text}}};
    Geometry reference;
    Generators::Request request;
    request.Parameters = parameters;
    CHECK(producer.make(request, reference), "explicit width produces geometry");
    // Native X is east. The metre-to-degree approximation differs from
    // WGS84 by less than a centimetre at these widths on the equator.
    CHECK(HasNominalEastSides(reference, width),
          "nominal wall sides match declared metres, excluding roof overhang");
    for (const double extent : {1.0, 100.0, 10000.0}) {
      request.ExtentM = extent;
      Geometry another;
      CHECK(producer.make(request, another) && another.parts() == reference.parts(),
            "changing region retains building geometry");
      for (int part = 0; part < reference.parts() && part < another.parts(); ++part) {
        CHECK(std::ranges::equal(reference.positionsOf(part), another.positionsOf(part)),
              "region cannot rescale building vertices");
      }
    }
  }
  Geometry held;
  CHECK(producer.make({}, held), "default width still works");
  CHECK(HasNominalEastSides(held, 12.0), "default width is twelve metres");
  if (const char *path = std::getenv("OUTSHINE_TEST_GLB")) {
    const auto glb = exportGlb(held);
    CHECK(glb.has_value(), "public exporter accepts native building");
    if (glb) {
      std::ofstream file(path, std::ios::binary);
      file.write(reinterpret_cast<const char *>(glb->data()),
                 static_cast<std::streamsize>(glb->size()));
      CHECK(file.good(), "building render fixture written completely");
    }
  }
  const int parts = held.parts();
  const int surfaces = held.surfaces();
  for (const std::string_view invalid :
       {"", "0", "-1", "nan", "inf", "1e999", "12x", " 12", "12 "}) {
    const std::array<Generators::Parameter, 1> parameters{{{.Name = "widthM", .Value = invalid}}};
    Generators::Request request;
    request.Parameters = parameters;
    CHECK(!producer.make(request, held) && held.parts() == parts && held.surfaces() == surfaces,
          "invalid width preserves existing output");
  }
  const std::array<Generators::Parameter, 2> duplicates{
      {{.Name = "widthM", .Value = "12"}, {.Name = "widthM", .Value = "24"}}};
  Generators::Request duplicate;
  duplicate.Parameters = duplicates;
  CHECK(!producer.make(duplicate, held) && held.parts() == parts,
        "duplicate width is not silently resolved");
  return Report();
}
