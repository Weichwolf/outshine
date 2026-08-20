#include <cmath>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

#include "PreparedRoot.h"

#include "Document.h"
#include "Json.h"
#include "Subject.h"

using outshine::Json;
using outshine::Gltf::Document;
using outshine::Gltf::Placement;
using outshine::Gltf::Transform;
using outshine::Gltf::Viewport;

namespace {

const std::string kCase = outshine::Test::PreparedRoot() + "/test-render-khronos-glTF-Triangle/";

std::string Slurp(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

}

int main() {
  using namespace outshine::Test;

  const std::string manifestText = Slurp(std::string(kCase) + "manifest.json");
  Json manifest;
  CHECK(!manifestText.empty() && manifest.Parse(manifestText.c_str(), manifestText.size()),
        "the case's manifest is present and parses");
  if (manifestText.empty()) { return Report(); }

  const Json::Ref declared = manifest.Root()["scene"]["camera"];
  const Json::Ref recipe = manifest.Root()["renders"]["default"];
  double eye[3] = {0, 0, 0};
  double aim[3] = {0, 0, 0};
  for (int axis = 0; axis < 3; ++axis) {
    eye[axis] = declared["positionM"][static_cast<size_t>(axis)].Num(0.0);
    aim[axis] = declared["lookAtM"][static_cast<size_t>(axis)].Num(0.0);
  }
  const Viewport viewport{recipe["resolutionX"].Num(0.0), recipe["resolutionY"].Num(0.0)};
  CHECK(viewport.WidthPx == 1280.0 && viewport.HeightPx == 720.0,
        "the manifest's render recipe is the 1280x720 frame the oracle was rendered at");

  const std::string subjectPath = std::string(kCase) + "scene.gltf";
  if (Slurp(subjectPath).empty()) {
    Unprepared(subjectPath.c_str());
    return Report();
  }

  Document document;
  const bool read = document.ReadFile(subjectPath);
  CHECK(read, "the Khronos Triangle reads as a .gltf with its buffer beside it");
  if (!read) {
    std::printf("       %s\n", document.Error().c_str());
    return Report();
  }
  CHECK(document.Meshes().size() == 1 && document.Meshes()[0].Primitives.size() == 1,
        "the subject is one mesh of one primitive");
  const auto &primitive = document.Meshes()[0].Primitives[0];

  const std::string missing =
      outshine::Gltf::MissingSemantics(primitive, {"POSITION", "NORMAL"});
  CHECK(missing == "NORMAL",
        "asked for POSITION and NORMAL, the reader names NORMAL as missing and derives nothing");

  std::vector<double> positions;
  CHECK(document.ReadElements(primitive.Find("POSITION"), positions), "the positions decode");
  std::vector<uint32_t> indices;
  CHECK(document.ReadIndices(primitive.Indices, indices), "the u16 indices decode");
  CHECK(positions.size() == 9 && indices.size() == 3,
        "three vertices and three indices, which is what the file declares");
  if (positions.size() != 9 || indices.size() != 3) { return Report(); }

  Placement place;
  CHECK(Placement::LookAt(eye, aim, declared["rollRad"].Num(0.0), place),
        "the declared camera placement resolves to a camera basis");
  place.YfovRad = declared["yfovRad"].Num(0.0);
  place.ZNearM = declared["clipStartM"].Num(0.0);
  place.ZFarM = declared["clipEndM"].Num(0.0);
  Transform clip;
  CHECK(place.Clip(viewport.Aspect(), clip), "the declared lens and placement yield a projection");

  double raster[3][2] = {{0, 0}, {0, 0}, {0, 0}};
  double closestEdgePx = 1e30;
  for (size_t corner = 0; corner < 3; ++corner) {
    const size_t vertex = indices[corner];
    const double world[3] = {positions[vertex * 3], positions[vertex * 3 + 1],
                             positions[vertex * 3 + 2]};
    double ndc[3] = {0, 0, 0};
    clip.Point(world, ndc);
    viewport.Raster(ndc, raster[corner]);
    const double margins[4] = {raster[corner][0] + 0.5, viewport.WidthPx - 0.5 - raster[corner][0],
                               raster[corner][1] + 0.5, viewport.HeightPx - 0.5 - raster[corner][1]};
    for (double margin : margins) { closestEdgePx = (margin < closestEdgePx) ? margin : closestEdgePx; }
  }
  CHECK(closestEdgePx > 0.0, "every vertex of the subject falls inside the frame");
  Note("closest vertex to a frame edge", closestEdgePx, "px");

  const double areaPx = std::fabs((raster[1][0] - raster[0][0]) * (raster[2][1] - raster[0][1]) -
                                  (raster[2][0] - raster[0][0]) * (raster[1][1] - raster[0][1])) /
                        2.0;
  const double fraction = areaPx / (viewport.WidthPx * viewport.HeightPx);
  const Json::Ref accepted = manifest.Root()["expected"]["subjectFrameFraction"];
  const double expected = accepted["value"].Num(0.0);
  CHECK(expected > 0.0, "the manifest states the projected frame fraction it derived");

  CHECK_NEAR(fraction, expected, 5e-7, "dimensionless",
             "the reader's projection of the fetched vertices reproduces the oracle's analytic "
             "projected area fraction");
  Note("projected frame fraction", fraction, "dimensionless");
  Note("projected area", areaPx, "px^2");

  const double normals[3][2] = {{1, 2}, {1, -3}, {2, -1}};
  for (int edge = 0; edge < 3; ++edge) {
    const double *from = raster[edge];
    const double *to = raster[(edge + 1) % 3];
    const double along[2] = {to[0] - from[0], to[1] - from[1]};
    const double length = std::sqrt(along[0] * along[0] + along[1] * along[1]);
    const double scale = std::sqrt(normals[edge][0] * normals[edge][0] +
                                   normals[edge][1] * normals[edge][1]);
    CHECK_NEAR((normals[edge][0] * along[0] + normals[edge][1] * along[1]) / (length * scale), 0.0,
               1e-12, "cosine",
               "the edge lies on the line of its declared integer normal, which no mirrored axis, no "
               "flipped raster row order and no roll of the wrong sign survives");
    const double constant = normals[edge][0] * from[0] + normals[edge][1] * from[1];
    const double offLattice = std::fabs(constant - std::floor(constant + 0.5));
    CHECK_NEAR(offLattice, 0.5, 1e-9, "lattice steps",
               "the edge sits half a lattice step from every pixel centre, which is where the "
               "distance to the nearest one is as large as a rational slope allows");
    Note("edge margin from every pixel centre", offLattice / scale, "px");
  }

  Covers("I.26 the reader: buffers, bufferViews, accessors, meshes, nodes and the camera, checked "
         "against Blender's analytic answer for test/render/khronos/glTF/Triangle/");
  return Report();
}
