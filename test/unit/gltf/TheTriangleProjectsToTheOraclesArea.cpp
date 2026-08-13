/* THE READER AGAINST A NUMBER THIS TREE DID NOT PRODUCE. `test/render/coverage/fetched-triangle/`
 * was prepared by the corpus round: the vertices come from Khronos, the camera was placed in Blender
 * and the projected area fraction was derived analytically there. Nothing below re-derives it -- the
 * expected value is read out of the case's own manifest -- so what is being checked is the chain
 * *file bytes -> accessor -> view -> projection -> raster*, end to end, against an outside answer.
 *
 * WHY THE AREA IS EXACT HERE AND NOT AN APPROXIMATION: the subject is a plane perpendicular to the
 * optical axis, so its perspective image is a similarity and the shoelace area is closed form.
 *
 * THE EDGES ARE THE HANDEDNESS CHECK AND THE AREA IS NOT. Area survives a mirrored x axis, a flipped
 * raster row order and a roll of the wrong sign; the three edge normals below survive none of them.
 *
 * AND THE QUANTITY THEY ARE CHECKED ON IS THE ONE THE COMPARISON TURNS ON. What stood here was the
 * angle each edge makes with the nearest raster axis, maximised at 22.5 degrees -- a proxy, and the
 * wrong one: a rasteriser and a path tracer disagree about a pixel when its CENTRE is close to an
 * EDGE, which is a distance and not an angle. At an irrational slope those distances equidistribute
 * and the smallest of them falls as about 0.5/L over L boundary pixels, so a bigger subject is a
 * worse one. At a slope p/q in lowest terms the distance from every pixel centre in the plane to
 * p*X - q*Y = c is |c - round(c)| / sqrt(p^2 + q^2), which depends on neither L nor position, and at
 * c on a half lattice step it is the largest it can be. This test pins exactly that: the three
 * normals, and half a lattice step on each.
 *
 * THE SUBJECT CARRIES NO NORMAL AND THAT IS NOT REPAIRED (board:0073): the reader is
 * asked for one below and must name what is missing. */
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

#include "Document.h"
#include "Json.h"
#include "Subject.h"

using outshine::Json;
using outshine::Gltf::Document;
using outshine::Gltf::Placement;
using outshine::Gltf::Transform;
using outshine::Gltf::Viewport;

namespace {

const char *const kCase = "test/render/coverage/fetched-triangle/";

std::string Slurp(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
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

  /* THE SUBJECT IS FETCHED, NOT TRACKED (board:0083: a case directory's only
   * tracked file is its manifest), so on a fresh clone it is absent and that is a statement about
   * the tree rather than about the reader. It is RED and it is not a skip -- a tier that skipped
   * here could not be told from one that passed having read nothing. */
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

  /* The placement convention -- POSITIVE ROLL TURNS THE CAMERA'S RIGHT VECTOR TOWARDS ITS UP VECTOR,
   * so the image of the world rotates by the opposite sign -- is stated once, in src/gltf, and this
   * test reads it from there rather than restating it. A second copy is how the two come to disagree
   * on the day one of them is corrected. */
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
  /* The manifest carries nine decimals, which is what the runner recomputes against; the tolerance
   * is far above float noise and far below the 1.65x framing error this number exists to pin. */
  CHECK_NEAR(fraction, expected, 5e-7, "dimensionless",
             "the reader's projection of the fetched vertices reproduces the oracle's analytic "
             "projected area fraction");
  Note("projected frame fraction", fraction, "dimensionless");
  Note("projected area", areaPx, "px^2");

  /* The three edges of the sample under the declared roll of -arctan(1/2), each as the primitive
   * integer normal of the line it lies on in raster pixels. Coprime, so `p*i - q*j` over integer
   * pixel centres runs over every integer and the nearest one to `c` is at distance
   * `|c - round(c)|`. */
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
         "against Blender's analytic answer for test/render/coverage/fetched-triangle/");
  return Report();
}
