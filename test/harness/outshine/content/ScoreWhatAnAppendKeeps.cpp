#include <cstdio>
#include <span>
#include <string>
#include <vector>

#include <Geometry.h>

#include "Check.h"
#include "Subject.h"

namespace {

// The oracle is that appending is not merging, and it does not depend on our design: two bodies
// joined into one buffer set are still two bodies, and a part that named its own material before
// the join must name the same material after it. A part index is a NAME, and joining two
// namespaces without shifting one silently renames everything in the second.
//
// This was measured rather than supposed. The ground ring appended to the driver's car declared
// `Material = 0`, which is also the car's own first slot, so the terrain's surface was painted
// onto the roof, the rear window and the boot lid -- visible in the still, and not a shading bug
// but a collision of two namespaces.
constexpr double kUnitM = 1.0;

void Facing(outshine::Geometry &into, const char *named, int material) {
  constexpr float kFace[9] = {0.0f, 0.0f, 0.0f, (float)kUnitM, 0.0f, 0.0f, 0.0f, (float)kUnitM,
                              0.0f};
  constexpr uint32_t kRun[3] = {0, 1, 2};
  const int part = into.Part(named, material);
  (void)into.Positions(part, std::span<const float>(kFace, 9));
  (void)into.Triangles(part, std::span<const uint32_t>(kRun, 3));
}

}

int main(void) {
  using namespace outshine::Test;
  using namespace outshine::Gltf;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  outshine::Geometry host;
  Facing(host, "body", 0);
  Facing(host, "glass", 1);
  Subject standing;
  CHECK(standing.Assemble(host),
        "a host of two parts naming materials 0 and 1 stands");
  if (standing.Parts().size() != 2) { return Report(); }

  outshine::Geometry guest;
  Facing(guest, "ground", 0);
  Subject arriving;
  CHECK(arriving.Assemble(guest),
        "a guest of one part naming material 0 stands");

  const size_t was = standing.Parts().size();
  CHECK(standing.Append(arriving), "the guest appends onto the host");
  CHECK(standing.Parts().size() == was + 1, "and the picture holds one part more than before");
  if (standing.Parts().size() != was + 1) { return Report(); }

  for (size_t at = 0; at < standing.Parts().size(); ++at) {
    std::printf("PART %zu '%s' names material %d\n", at,
                standing.Parts()[at].NodeName.c_str(), standing.Parts()[at].Material);
  }

  CHECK(standing.Parts()[0].Material == 0 && standing.Parts()[1].Material == 1,
        "the host's own parts keep the materials they named -- an append that renamed the HOST "
        "would be worse than one that renamed the guest");
  CHECK(standing.Parts()[2].Material == 2,
        "**AN APPENDED PART NAMES ITS OWN MATERIAL, NOT THE HOST'S**: two bodies joined into one "
        "buffer set are still two bodies, and a part index is a NAME -- joining two namespaces "
        "without shifting one silently renames everything in the second. Measured: the ground "
        "ring appended to the car declared material 0, which is the car's own first slot, and "
        "the terrain was painted onto the roof");

  Covers("gltf: appending one subject onto another shifts the guest's material names clear of "
         "the host's, so a joined buffer set is still two bodies and neither wears the other's "
         "surface");
  return Report();
}
