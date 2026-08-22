#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Carriageway.h"
#include "Ribbon.h"

using outshine::Curve;
using outshine::Placed;
using outshine::ReferenceLine;
using outshine::Ribbon;
using outshine::Section;
using outshine::Segment;
using outshine::Sweep;

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  ReferenceLine along;
  std::string error;
  const std::vector<Segment> laid = {{Curve::Straight, 1000.0, 0.0, 0.0}};
  const bool lay = along.Lay(Placed{}, laid, error) &&
                   along.Rise({{0.0, 0.0, 0.0}, {1000.0, 0.0, 0.0}}, error) &&
                   along.Bank({{0.0, 0.0, 0.0}, {1000.0, 0.0, 0.0}}, error);
  if (!lay) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(lay, "a straight kilometre of reference line lays");

  Section section;
  section.HalfWidthM = 5.0;
  section.ShoulderM = 2.5;
  section.ThicknessM = 0.35;
  const Ribbon swept = Sweep(along, section, 100.0, 500.0, 2.0);
  CHECK(swept.Woven, "and sweeps into a solid");
  if (!swept.Woven) {
    std::printf("REFUSED %s\n", swept.Error.c_str());
    return Report();
  }

  Note("triangles", (double)swept.Triangles, "triangles");
  Note("vertices", (double)swept.Vertices, "vertices");

  double leastX = 1.0e30, mostX = -1.0e30;
  for (size_t at = 0; at + 2 < swept.PositionM.size(); at += 3) {
    leastX = std::fmin(leastX, (double)swept.PositionM[at]);
    mostX = std::fmax(mostX, (double)swept.PositionM[at]);
  }

  size_t startCap = 0, endCap = 0;
  size_t agreeing = 0, disagreeing = 0;
  for (size_t tri = 0; tri + 2 < swept.Index.size(); tri += 3) {
    const auto vertex = [&](size_t corner, int axis) {
      return (double)swept.PositionM[(size_t)swept.Index[tri + corner] * 3u + (size_t)axis];
    };
    double edgeA[3], edgeB[3];
    for (int axis = 0; axis < 3; ++axis) {
      edgeA[axis] = vertex(1, axis) - vertex(0, axis);
      edgeB[axis] = vertex(2, axis) - vertex(0, axis);
    }
    const double faceE = edgeA[1] * edgeB[2] - edgeA[2] * edgeB[1];
    const double faceUp = edgeA[2] * edgeB[0] - edgeA[0] * edgeB[2];
    const double faceZ = edgeA[0] * edgeB[1] - edgeA[1] * edgeB[0];
    const double area2 =
        std::sqrt(faceE * faceE + faceUp * faceUp + faceZ * faceZ);
    if (!(area2 > 0.0)) { continue; }

    const bool flat = std::fabs(vertex(0, 0) - vertex(1, 0)) < 1.0e-9 &&
                      std::fabs(vertex(0, 0) - vertex(2, 0)) < 1.0e-9;
    if (!flat) { continue; }
    const bool atStart = std::fabs(vertex(0, 0) - leastX) < 1.0e-9;
    const bool atEnd = std::fabs(vertex(0, 0) - mostX) < 1.0e-9;
    if (!atStart && !atEnd) { continue; }
    if (atStart) { ++startCap; }
    if (atEnd) { ++endCap; }

    const double vertexNormalE = (double)swept.NormalM[(size_t)swept.Index[tri] * 3u];
    if (faceE * vertexNormalE > 0.0) {
      ++agreeing;
    } else {
      ++disagreeing;
    }
  }

  Note("triangles standing in the start cap's plane", (double)startCap, "triangles");
  Note("triangles standing in the end cap's plane", (double)endCap, "triangles");
  CHECK(startCap >= 6 && endCap >= 6,
        "**A SWEPT CARRIAGEWAY IS A SOLID, AND A SOLID HAS ENDS.** Both cross-sections are closed "
        "-- six triangles each over the four across-stations and their soffit row. An open end "
        "shows its unlit inside edge-on as the dark dash the reviewer found floating at the "
        "vanishing point of four stations (board:1565)");
  Note("cap triangles whose winding agrees with their vertex normal", (double)agreeing,
       "triangles");
  Note("and disagrees", (double)disagreeing, "triangles");
  CHECK(disagreeing == 0 && agreeing == startCap + endCap,
        "**AND EVERY CAP TRIANGLE WINDS WITH ITS OWN NORMAL.** The face normal from the winding "
        "and the declared vertex normal point the same way on every cap triangle, so a back-face "
        "cull keeps the outside and drops the inside -- the glTF convention, checked by "
        "determinant rather than by eye");

  Covers("I.4.7 the carriageway ribbon is a closed solid: top, soffit, both flanks and both end "
         "caps, each cap wound outward");
  return Report();
}
