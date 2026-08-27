#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include "Check.h"
#include "ClassField.h"
#include "GroundSnapshot.h"
#include "GroundMaterials.h"
#include "VegetationTemplates.h"

namespace {

// THE GENERATOR TIER'S INPUT IS FOUR VALUES, NOT A WORLD. `Generators::Ground::Snapshot` holds a
// height patch, a class structure, a feature field and a ground table -- and every one of them is
// built from a ground query and an OSM field. Nothing in it needs `Ground::World`.
//
// It looked otherwise for nine review rounds because ONE class owned the builders:
// `src/engine/Sim.h` was the only place `Buildings.h`, `Forest.h`, `GeneratorSet.h`,
// `Infrastructure.h` and `Water.h` were included from, and `grep -rn '"Sim.h"'` found one line --
// its own `.cpp`. Thirty of the tree's thirty-seven stranded sources hung off that. So the
// generators read as unreachable capability when what was unreachable was their DOOR.
//
// THIS CASE IS THE REACHABILITY. It stands in a suite that links no engine source at all -- the
// geo suite's group list holds `src/generators` and nothing from `src/engine` -- and it
// builds a snapshot. That it COMPILES AND LINKS here is half the claim; the numbers below are
// the other half.
//
// The oracle is the ground the case hands in: a block whose every posting stands at one declared
// height. The patch either reads those postings or it does not, and a patch that reads a grid it
// was not handed comes back at zero -- which is what the first run of this case measured, and it
// was the CASE that was wrong, not the composition.
constexpr int kZoom = 14;
constexpr double kLatDeg = 48.1372;
constexpr double kLonDeg = 11.5756;
constexpr double kBaseM = 500.0;

class Flat final : public outshine::GroundQuery {
public:
  [[nodiscard]] outshine::GroundSample At(double lat, double lon) const override {
    ++Asked_;
    return outshine::GroundSample::At(HeightAt(lat, lon));
  }

  [[nodiscard]] outshine::GroundSample Resident(double lat, double lon) const override {
    return At(lat, lon);
  }

  [[nodiscard]] double PostM(double latDeg) const override {
    (void)latDeg;
    return 30.0;
  }

  [[nodiscard]] outshine::Ground::GroundBlock BlockAt(int z, long x, long y) const override {
    Nodes_.assign((size_t)kSide * (size_t)kSide, (float)kBaseM);
    return outshine::Ground::GroundBlock::Over(Nodes_.data(), z, x, y, kSide, kSide - 1);
  }

  [[nodiscard]] static double HeightAt(double lat, double lon) {
    (void)lat;
    (void)lon;
    return kBaseM;
  }

  [[nodiscard]] long Asked() const { return Asked_; }

private:
  static constexpr int kSide = 33;
  mutable std::vector<float> Nodes_;
  mutable long Asked_ = 0;
};

}

int main(void) {
  using namespace outshine::Test;
  using namespace outshine::Generators;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  outshine::Ground::GroundMaterials materials;
  outshine::Ground::VegetationTemplates templates;
  if (!materials.Load("src/assets/world/ground-materials.json") ||
      !templates.Load("src/assets/world/vegetation.json", materials)) {
    Unprepared("the shipped ground tables do not load");
    return Report();
  }

  const std::shared_ptr<const GroundTable> table = TableOf(templates);
  std::printf("TEMPLATES %zu   the table carries %zu row(s)\n", templates.TemplateCount(),
              table ? templates.TemplateCount() : 0u);
  CHECK(table != nullptr,
        "**THE GROUND TABLE IS BUILT FROM THE SHIPPED TEMPLATES OUTSIDE THE ENGINE**: it was a "
        "member of `Core::Sim`, and a member of a class with one consumer is a capability no "
        "declaration can reach");

  const Region region = Region::Of(kZoom, kLatDeg, kLonDeg);
  const Flat ground;
  outshine::Ground::ClassField classes;

  Fields stands;
  Ground::Snapshot snapshot;
  const Snapped without = SnapshotOver(region, ground, classes, stands, table, &snapshot);
  std::printf("WITHOUT VECTORS   %s, ground queries made %ld\n",
              without == Snapped::Taken      ? "TAKEN"
              : without == Snapped::Waiting  ? "waiting"
                                             : "no ground",
              ground.Asked());

  CHECK(without != Snapped::Taken,
        "a snapshot with no class structure and no vectors is NOT taken -- the composition "
        "refuses by naming what it is short of, rather than handing back a half-built world a "
        "generator would walk");
  CHECK(snapshot.Patch != nullptr,
        "**AND THE HEIGHT PATCH IS BUILT ANYWAY**, from the ground query alone: the patch is the "
        "one part of a snapshot that needs nothing but the terrain, so a region whose vectors "
        "have not settled still knows its own shape");

  if (snapshot.Patch) {
    const double atOrigin = snapshot.Patch->HeightAslM(0.0, 0.0);
    const double atMiddle =
        snapshot.Patch->HeightAslM(0.5 * region.SpanEm(), 0.5 * region.SpanNm());
    std::printf("PATCH reads %.4f m at its origin and %.4f m at its middle, the block holds "
                "%.4f m\n", atOrigin, atMiddle, kBaseM);
    CHECK(std::fabs(atOrigin - kBaseM) < 1.0e-3 && std::fabs(atMiddle - kBaseM) < 1.0e-3,
          "and the patch reads the BLOCK the query handed it -- a patch that samples a grid it "
          "was not given comes back at zero, which is what this case measured before its own "
          "double filled its nodes");
  }

  Covers("world: the generator tier's input is four values built from a ground query and an OSM "
         "field, so a snapshot composes in a suite that links no engine source -- the table from "
         "the shipped templates, the patch from the query alone, and a composition short of its "
         "parts refusing rather than half-standing");
  return Report();
}
