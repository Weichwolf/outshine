#include <cstdio>
#include <cstdlib>
#include <string>

#include "Check.h"

namespace {

size_t Counted(const std::string &command) {
  const std::string said = outshine::Test::Ask(command + " 2>/dev/null | wc -l | tr -d ' '");
  return said.empty() ? 0u : static_cast<size_t>(std::strtoul(said.c_str(), nullptr, 10));
}

constexpr size_t kRotationsInThePlane = 3;

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const size_t south =
      Counted("grep -rnE 'SouthM|EastSouth' src include test --include='*.h' --include='*.cpp'");
  const size_t negated = Counted(
      "grep -rnE -- '-[[:space:]]*[A-Za-z_.]*NorthM' src include --include='*.h' --include='*.cpp' "
      "| grep -v 'src/base/math/RenderFrame.h'");
  std::printf("  SouthM or EastSouth anywhere      %zu\n", south);
  std::printf("  a negated north outside RenderFrame %zu (%zu are rotations in the plane)\n",
              negated,
              kRotationsInThePlane);

  CHECK(south == 0,
        "**ONE LOCAL FRAME, AND IT IS EAST-NORTH-UP**: Cesium's, and the one every geographic "
        "struct in this tree speaks. A South-positive axis is how a drape was sampled mirrored "
        "across the east axis and a junction's normals summed the wrong way (board:2147); the "
        "word does not come back");

  CHECK(negated <= kRotationsInThePlane,
        "**NORTH IS NEGATED IN ONE PLACE**: RenderFrame::ZOfNorth in src/base/math/RenderFrame.h "
        "is the conversion from East-North-Up to the renderer's right-handed Y-up frame, and its "
        "static assertions state the axes. The three negations allowed outside it are the "
        "building generator's rotations of a footprint axis by a quarter turn IN THE PLANE "
        "(BuildingShape.h AxisV, BuildingMesh.cpp, BuildingShape.cpp), which convert nothing; a "
        "fourth is a hand-written frame conversion and the count may only fall");

  Covers("board:2147 -- one local frame, and one named conversion to the renderer's");
  return Report();
}
