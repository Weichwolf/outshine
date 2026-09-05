#include <array>
#include <cstdio>

#include <scene/Geometry.h>
#include <scene/Material.h>

#include "Check.h"

// THE DOOR HOLDS THE STANDARD FOR A MESH, board:2148. glTF's specification says a normal is unit
// length and a front face is counter-clockwise; Unreal's and RAGE's mesh builds enforce both when
// they cook. Here the door refuses a normal that is not unit length and counts the triangles that
// are wound against their own normals, so a mesher that gets either wrong is told at the door and
// not by a black pixel. The negative control is the flipped triangle: the same three vertices
// wound the other way read as one triangle against its normals.

int main(void) {
  using namespace outshine;
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Geometry made;
  Material flat;
  const MaterialInstance surface = made.addSurface("flat", flat);
  const int part = made.addPart("triangle", surface);
  CHECK(part >= 0, "the geometry stands a part to fill");

  const std::array<float, 9> corners = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f};
  const std::array<float, 9> up = {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  const std::array<float, 9> twice = {0.0f, 2.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 2.0f, 0.0f};
  const std::array<uint32_t, 3> counterClockwise = {0u, 1u, 2u};
  const std::array<uint32_t, 3> clockwise = {0u, 2u, 1u};

  CHECK(made.setPositions(part, corners), "three corners in the renderer's frame are taken");
  CHECK(!made.setNormals(part, twice),
        "**A NORMAL THAT IS NOT UNIT LENGTH IS REFUSED AT THE DOOR**: glTF requires it, the shader "
        "presumes it, and a length of two is how an accumulated normal looked before board:2148");
  CHECK(made.setNormals(part, up), "unit normals are taken");

  CHECK(made.setTriangles(part, counterClockwise), "a counter-clockwise triangle is taken");
  CHECK(made.windingAgainstNormals(part) == 0,
        "**A COUNTER-CLOCKWISE TRIANGLE FACES ALONG ITS NORMALS**: in a right-handed frame with x "
        "east, y up and z south, (0,0,0) -> (1,0,0) -> (0,0,-1) turns counter-clockwise seen from "
        "+y, so its face normal is +y and agrees with the vertices'");

  CHECK(made.setTriangles(part, clockwise), "the same corners the other way round are taken");
  CHECK(made.windingAgainstNormals(part) == 1,
        "**THE NEGATIVE CONTROL: THE FLIPPED TRIANGLE IS COUNTED**: wound clockwise its face "
        "normal is -y against vertex normals of +y, which is the one-triangle case of every dark "
        "junction body this tree drew before its faces were built per face");

  Covers("board:2148 -- the door refuses a normal that is not unit and counts a face wound against "
         "its normals");
  return Report();
}
