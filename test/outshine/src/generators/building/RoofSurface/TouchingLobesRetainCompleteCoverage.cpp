#include "Check.h"
#include "RoofSurface.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const std::array<EastNorth, 12> ring{{{-20.820334080010639, -7.1490440268733551},
                                        {-14.720107616514307, -7.1490440268733551},
                                        {-17.107152094867978, -4.3688591710510707},
                                        {-13.92442612372975, -1.9858446620407335},
                                        {-12.730903884552914, -3.5745208905680954},
                                        {-13.128744630945192, -3.9716900165960567},
                                        {-10.343859406199243, -7.1490440268733551},
                                        {13.92442612372975, -7.1490440268733551},
                                        {17.504992841260258, -3.5745208905680954},
                                        {7.9568149278455715, 7.1490350407661367},
                                        {0, 0},
                                        {-4.6275193252101401, 4.9750636884403416}}};
  BuildingScratch scratch;
  std::vector<EastNorth> triangles;
  CHECK(RoofSurface::Fill(ring, scratch, triangles), "touching real lobes retain a complete roof");
  double area = 0;
  for (size_t at = 0; at < triangles.size(); at += 3) {
    const auto a = triangles[at];
    const auto b = triangles[at + 1];
    const auto c = triangles[at + 2];
    const double cross =
        (b.EastM - a.EastM) * (c.NorthM - a.NorthM) - (b.NorthM - a.NorthM) * (c.EastM - a.EastM);
    CHECK(cross > 0, "every repaired roof triangle has positive finite area");
    area += cross / 2;
  }
  CHECK_NEAR(area,
             289.3884089112871,
             1e-6,
             "square metres",
             "validated decomposition preserves both lobes and their source area");
  const auto before = triangles;
  auto backwards = ring;
  std::ranges::reverse(backwards);
  CHECK(!RoofSurface::Fill(backwards, scratch, triangles),
        "repair does not weaken the declared winding contract");
  CHECK(triangles == before, "refused input preserves the existing mesh");
  return Report();
}
