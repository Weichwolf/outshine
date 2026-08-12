/* THE READER AGAINST A NUMBER THIS TREE DID NOT PRODUCE. `test/render/coverage/triangle/` was
 * prepared by the corpus round: the vertices come from Khronos, the camera was placed in Blender and
 * the projected area fraction was derived analytically there. Nothing below re-derives it -- the
 * expected value is read out of the case's own manifest -- so what is being checked is the chain
 * *file bytes -> accessor -> view -> projection -> raster*, end to end, against an outside answer.
 *
 * WHY THE AREA IS EXACT HERE AND NOT AN APPROXIMATION: the subject is a plane perpendicular to the
 * optical axis, so its perspective image is a similarity and the shoelace area is closed form.
 *
 * THE EDGE ANGLES ARE THE HANDEDNESS CHECK AND THE AREA IS NOT. Area survives a mirrored x axis, a
 * flipped raster row order and a roll of the wrong sign; 12, 57 and 102 degrees survive none of them.
 * The three values are the manifest's own, stated there in prose as the reason the camera admits no
 * axis-aligned edge, and made checkable here.
 *
 * THE SUBJECT CARRIES NO NORMAL AND THAT IS NOT REPAIRED (doc/requirements.md I.26): the reader is
 * asked for one below and must name what is missing. */
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

#include "Document.h"
#include "Json.h"

using outshine::Json;
using outshine::Gltf::Document;
using outshine::Gltf::Transform;
using outshine::Gltf::Viewport;

namespace {

const char *const kCase = "test/render/coverage/triangle/";

/* The manifest declares the camera as a position, an aim and a roll -- Blender's placement, not
 * glTF's, because this subject's file carries no camera at all. Building it needs one convention
 * this side has to state: POSITIVE ROLL TURNS THE CAMERA'S RIGHT VECTOR TOWARDS ITS UP VECTOR. It is
 * pinned by the oracle's own camera matrix, whose right vector is (cos 12, sin 12, 0) in glTF axes. */
Transform CameraWorld(const double eye[3], const double aim[3], double rollRad) {
  double forward[3] = {aim[0] - eye[0], aim[1] - eye[1], aim[2] - eye[2]};
  const double reach =
      std::sqrt(forward[0] * forward[0] + forward[1] * forward[1] + forward[2] * forward[2]);
  for (double &component : forward) { component /= reach; }
  const double worldUp[3] = {0.0, 1.0, 0.0};
  double right[3] = {forward[1] * worldUp[2] - forward[2] * worldUp[1],
                     forward[2] * worldUp[0] - forward[0] * worldUp[2],
                     forward[0] * worldUp[1] - forward[1] * worldUp[0]};
  const double span = std::sqrt(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
  for (double &component : right) { component /= span; }
  const double up[3] = {right[1] * forward[2] - right[2] * forward[1],
                        right[2] * forward[0] - right[0] * forward[2],
                        right[0] * forward[1] - right[1] * forward[0]};
  const double turn = std::cos(rollRad);
  const double lean = std::sin(rollRad);
  Transform world;
  for (int axis = 0; axis < 3; ++axis) {
    world.M[axis] = right[axis] * turn + up[axis] * lean;
    world.M[4 + axis] = up[axis] * turn - right[axis] * lean;
    world.M[8 + axis] = -forward[axis];
    world.M[12 + axis] = eye[axis];
  }
  world.M[3] = world.M[7] = world.M[11] = 0.0;
  world.M[15] = 1.0;
  return world;
}

std::string Slurp(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

double AngleDeg(const double from[2], const double to[2]) {
  double degrees = std::atan2(to[1] - from[1], to[0] - from[0]) * 180.0 / 3.14159265358979323846;
  while (degrees < 0.0) { degrees += 180.0; }
  while (degrees >= 180.0) { degrees -= 180.0; }
  return degrees;
}

} // namespace

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

  Document document;
  const bool read = document.ReadFile(std::string(kCase) + "scene.gltf");
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

  outshine::Gltf::Camera lens;
  lens.YfovRad = declared["yfovRad"].Num(0.0);
  lens.ZNearM = declared["clipStartM"].Num(0.0);
  lens.ZFarM = declared["clipEndM"].Num(0.0);

  Transform projection;
  CHECK(lens.Projection(viewport.Aspect(), projection), "the declared lens yields a projection");
  Transform view;
  CHECK(CameraWorld(eye, aim, declared["rollRad"].Num(0.0)).Inverse(view),
        "the declared camera placement inverts to a view transform");
  const Transform clip = projection * view;

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
  const Json::Ref accepted =
      manifest.Root()["acceptance"]["coverage"]["subjectFrameFraction"];
  const double expected = accepted["value"].Num(0.0);
  CHECK(expected > 0.0, "the manifest states the projected frame fraction it derived");
  /* The manifest carries six decimals, which is the tolerance available from it; the ninth digit is
   * in the note below, where the next round can read it without re-deriving anything. */
  CHECK_NEAR(fraction, expected, 5e-7, "dimensionless",
             "the reader's projection of the fetched vertices reproduces the oracle's analytic "
             "projected area fraction");
  Note("projected frame fraction", fraction, "dimensionless");
  Note("projected area", areaPx, "px^2");

  /* 12, 57 and 102 degrees: the manifest's own statement of what the 12-degree roll does to the
   * subject's three edges in raster space. */
  const double expectedAngles[3] = {12.0, 57.0, 102.0};
  for (int edge = 0; edge < 3; ++edge) {
    const double got = AngleDeg(raster[edge], raster[(edge + 1) % 3]);
    CHECK_NEAR(got, expectedAngles[edge], 1e-6, "deg",
               "each edge sits where the declared roll, the aspect and the raster row order put it "
               "-- which no mirrored axis and no roll of the wrong sign survives");
  }

  Covers("I.26 the reader: buffers, bufferViews, accessors, meshes, nodes and the camera, checked "
         "against Blender's analytic answer for test/render/coverage/triangle/");
  return Report();
}
