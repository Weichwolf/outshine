#include <utility>
#include "Check.h"
#include "HipRoofCodec.h"
#include "RoofSurface.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

BuildingShape Source() {
  BuildingShape shape;
  shape.Ring = {{.EastM = 0, .NorthM = -15.882787891137582},
                {.EastM = 6.3638537185468431, .NorthM = -15.882787891137582},
                {.EastM = 8.3525580055927318, .NorthM = -17.471069120240031},
                {.EastM = 8.3525580055927318, .NorthM = -19.456421281223612},
                {.EastM = 11.93222572227533, .NorthM = -19.456421281223612},
                {.EastM = 11.93222572227533, .NorthM = -17.471069120240031},
                {.EastM = 13.92093000932122, .NorthM = -15.882787891137582},
                {.EastM = 28.239600876051615, .NorthM = -15.485717653350321},
                {.EastM = 28.239600876051615, .NorthM = -18.662280334233799},
                {.EastM = 31.819268592734215, .NorthM = -18.662280334233799},
                {.EastM = 31.819268592734215, .NorthM = -7.1472490598528964},
                {.EastM = 30.626046020506681, .NorthM = -7.1472490598528964},
                {.EastM = 30.495876117199739, .NorthM = 0},
                {.EastM = 0, .NorthM = 0}};
  shape.Roof = RoofKind::Hip;
  shape.RiseM = 1;
  return shape;
}

void Complete(const BuildingShape &shape) {
  const RoofSurface roof(shape);
  BuildingScratch scratch;
  std::vector<EastNorth> triangles;
  CHECK(roof.Cover(shape.Ring, scratch, triangles),
        "real roof is fully emitted despite coincident skeleton nodes");
  double area = 0;
  for (size_t at = 0; at + 2 < triangles.size(); at += 3) {
    const auto a = triangles[at];
    const auto b = triangles[at + 1];
    const auto c = triangles[at + 2];
    const double cross =
        (b.EastM - a.EastM) * (c.NorthM - a.NorthM) - (b.NorthM - a.NorthM) * (c.EastM - a.EastM);
    CHECK(cross > 0, "every emitted triangle has positive finite projected area");
    area += cross / 2;
  }
  CHECK_NEAR(area, 519.44319875395922, 1e-9, "square metres", "no source roof area disappears");
  for (const auto p : shape.Ring) {
    CHECK_NEAR(roof.HeightAt(p), 0, 1e-9, "roof metres", "all original eaves keep their height");
  }
}

StructureBinary::Writer Legacy(const HipRoofPlan &plan) {
  std::vector<HipRoofPlan::Facet> facets(plan.Facets().begin(), plan.Facets().end());
  bool inserted = false;
  for (auto &facet : facets) {
    if (facet.SourceRing != 0 || facet.SourceEdge != 2) { continue; }
    const EastNorth duplicate{.EastM = 10.14239186393403, .NorthM = -0.7271358494688549};
    const auto node = std::ranges::find_if(facet.Ring, [&](EastNorth p) {
      return std::hypot(p.EastM - duplicate.EastM, p.NorthM - duplicate.NorthM) <=
             plan.PrecisionM();
    });
    CHECK(node != facet.Ring.end(), "legacy facet retains its real source edge and ridge");
    if (node == facet.Ring.end()) { continue; }
    facet.Ring.insert(node + 1, duplicate);
    inserted = true;
  }
  CHECK(inserted, "old payload reproduces the captured inner-node roundoff duplicate");
  StructureBinary::Writer writer;
  const auto point = [](auto &archive, const EastNorth &p) {
    return archive.Number(p.EastM) && archive.Number(p.NorthM);
  };
  CHECK(writer.Number(uint8_t{1}) && point(writer, plan.Origin()) &&
            writer.Number(plan.MaximumDistanceM()) && writer.Number(plan.PrecisionM()) &&
            writer.List(facets,
                        [&](auto &archive, const auto &facet) {
                          return point(archive, facet.Gradient) &&
                                 archive.Number(facet.InterceptM) &&
                                 archive.Number(facet.SourceRing) &&
                                 archive.Number(facet.SourceEdge) &&
                                 archive.List(facet.Ring, point);
                        }),
        "legacy bytes use the unchanged complete native asset format");
  return writer;
}
}

int main() {
  auto shape = Source();
  const auto prepared = RoofSurface::PrepareHip(shape);
  CHECK(prepared.has_value(), "real skeleton remains a valid prepared hip roof");
  if (!prepared) { return Report(); }
  Complete(shape);
  const auto &plan = **shape.HipRoof;
  CHECK(plan.Facets().size() == shape.Ring.size(), "every original source eave keeps its facet");
  const auto encoded = Legacy(plan);
  StructureBinary::Reader reader(encoded.Bytes);
  HipRoofCodec::Product restored;
  CHECK(HipRoofCodec::Fields(reader, restored),
        "old cached facets load without rebuilding skeleton");
  CHECK(reader.Bytes.empty() && restored && *restored, "complete cached payload is consumed");
  if (restored && *restored) {
    CHECK(HipRoofCodec::BoundTo(restored, shape.Ring, {}),
          "cache retains original source ownership");
    shape.HipRoof = std::move(restored);
    Complete(shape);
  }
  return Report();
}
