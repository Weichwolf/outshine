#include "Check.h"
#include "PlantBladeSurface.h"
#include <cstddef>
#include <limits>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr double kLengthM = 0.1;
  constexpr double kWidthM = 0.05;
  constexpr double kWidest = 0.6;
  constexpr double kBendRad = 0.2;
  constexpr double kTwistRad = 0.4;
  constexpr double kPetioleM = 0.01;
  constexpr double kBaseFill = 0.1;
  constexpr double kFold = 0.1;
  constexpr size_t kVertices = 18;
  constexpr size_t kTriangles = 10;
  PlantBlade blade{.LengthM = kLengthM,
                   .WidthM = kWidthM,
                   .Widest = kWidest,
                   .BaseFill = kBaseFill,
                   .BendRad = kBendRad,
                   .Fold = kFold,
                   .FoldPower = 1,
                   .TwistRad = kTwistRad,
                   .PetioleM = kPetioleM};
  PlantBladeSurfaceDemand demand{
      .Segments = 2, .Across = 1, .MaximumVertices = kVertices, .MaximumTriangles = kTriangles};
  const auto mesh = BuildPlantBladeSurface(blade, demand);
  CHECK(mesh.has_value(), "a folded blade and its petiole form an oriented open sheet");
  if (!mesh) { return Report(); }
  CHECK(mesh->Positions.size() == kVertices * 3 && mesh->Normals.size() == kVertices * 3 &&
            mesh->Texture.size() == kVertices * 2 && mesh->Triangles.size() == kTriangles * 3,
        "creases share positions while retaining separate normals within the exact budget");
  CHECK(mesh->Texture[1] < 0, "petiole retains its signed longitudinal coordinate");
  bool crease = false;
  for (size_t a = 0; a < kVertices; ++a) {
    for (size_t b = a + 1; b < kVertices; ++b) {
      bool samePosition = true;
      bool sameNormal = true;
      for (size_t component = 0; component < 3; ++component) {
        samePosition = samePosition &&
                       mesh->Positions[a * 3 + component] == mesh->Positions[b * 3 + component];
        sameNormal =
            sameNormal && mesh->Normals[a * 3 + component] == mesh->Normals[b * 3 + component];
      }
      crease = crease || (samePosition && !sameNormal);
    }
  }
  CHECK(crease, "hard fold or petiole junction normals are not averaged away");
  --demand.MaximumVertices;
  CHECK(!BuildPlantBladeSurface(blade, demand), "insufficient vertex budget rejects detail");
  ++demand.MaximumVertices;
  --demand.MaximumTriangles;
  CHECK(!BuildPlantBladeSurface(blade, demand), "insufficient triangle budget rejects detail");
  ++demand.MaximumTriangles;
  blade.Widest = 0;
  CHECK(!BuildPlantBladeSurface(blade, demand), "singular width warp is rejected");
  blade.Widest = kWidest;
  blade.TwistRad = std::numeric_limits<double>::infinity();
  CHECK(!BuildPlantBladeSurface(blade, demand), "nonfinite blade parameters are rejected");
  return Report();
}
