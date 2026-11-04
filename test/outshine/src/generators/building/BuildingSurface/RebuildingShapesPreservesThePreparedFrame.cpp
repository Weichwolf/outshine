#include "BuildingScratch.h"
#include "BuildingSurface.h"
#include "Check.h"
#include "Geodesy.h"
#include "PreparedStructureCodec.h"
#include <array>
#include <cmath>
#include <limits>

using namespace outshine;
using namespace outshine::Test;

int main() {
  const std::array ring{1.0, 1.0, 1.0, 1.0001, 1.0001, 1.0001, 1.0001, 1.0};
  StructurePlan plan;
  plan.RingLatLon = ring;
  plan.BaseAslM = plan.SeatAslM = plan.FootAslM = 100;
  plan.HeightM = 12;
  plan.HeightMeasured = true;
  plan.PitchedShare = 0;
  plan.WallColour = Vec3f{{0.4f, 0.3f, 0.2f}};
  GeoToEcef({.LongitudeDeg = ring[1], .LatitudeDeg = ring[0], .HeightM = plan.BaseAslM},
            plan.AnchorEcef);
  Generators::BuildingScratch scratch;
  const auto stored = Generators::BuildingSurface::Prepare(plan, scratch);
  CHECK(stored.has_value(), "the original surface has a complete coordinate frame");
  if (!stored) { return Report(); }
  plan.AnchorEcef[0] = std::nextafter(plan.AnchorEcef[0], std::numeric_limits<double>::infinity());
  const auto recomputed = Generators::BuildingSurface::Prepare(plan, scratch);
  CHECK(recomputed && !Generators::CompatiblePreparedSurface(*stored, *recomputed),
        "one representable anchor step changes the recomputed frame");
  plan.Prepared = &*stored;
  const auto rebuilt = Generators::BuildingSurface::Prepare(plan, scratch);
  CHECK(rebuilt && Generators::CompatiblePreparedSurface(*stored, *rebuilt),
        "rebuilding shapes reuses the stored frame exactly");
  if (!rebuilt) { return Report(); }
  CHECK(rebuilt->Origin() == stored->Origin() && rebuilt->Bounds().Min == stored->Bounds().Min &&
            rebuilt->Bounds().Max == stored->Bounds().Max,
        "the restored bounds remain in their original coordinate frame");
  plan.MinimumHeightM = 2;
  const auto differentHeight = Generators::BuildingSurface::Prepare(plan, scratch);
  CHECK(differentHeight && !Generators::CompatiblePreparedSurface(*stored, *differentHeight),
        "reusing the frame never accepts a different geometric height contract");
  plan.MinimumHeightM = 0;
  plan.WallColour = Vec3f{{0.8f, 0.3f, 0.2f}};
  const auto differentMaterial = Generators::BuildingSurface::Prepare(plan, scratch);
  CHECK(differentMaterial && !Generators::CompatiblePreparedSurface(*stored, *differentMaterial),
        "reusing the frame never accepts different material parameters");
  return Report();
}
