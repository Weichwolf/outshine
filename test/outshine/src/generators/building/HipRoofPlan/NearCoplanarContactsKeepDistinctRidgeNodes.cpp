#include "Check.h"
#include "HipRoofPlan.h"
#include <array>
#include <cstddef>
#include <map>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const std::array<EastNorth, 32> ring{
      {{99952.1707635745, -199792.62142574132},  {99950.61529368235, -199791.3634593886},
       {99951.39027430816, -199790.7794695993},  {99949.34631004825, -199788.0670374126},
       {99948.18383911197, -199788.94302209478}, {99940.19897291246, -199776.73432293773},
       {99939.42399228665, -199777.31831272706}, {99935.33606376775, -199771.8934483549},
       {99936.40303928821, -199771.69694887847}, {99935.3305551272, -199769.46750242353},
       {99936.10553575301, -199768.8835126342},  {99934.45457044504, -199768.30503148725},
       {99934.64556128244, -199766.94606107334}, {99932.99459597355, -199766.36757992517},
       {99933.18558681094, -199765.00860951125}, {99932.30960212878, -199763.84613857494},
       {99933.47207306506, -199762.9701538928},  {99932.59608838198, -199761.8076829553},
       {99942.67083650292, -199754.21581570499}, {99945.590785445, -199758.09071882797},
       {99947.91572731758, -199756.3387494636},  {99949.66769668282, -199758.66369133742},
       {99954.7050707433, -199754.86775771226},  {99955.6765508422, -199755.35074344403},
       {99955.28906053171, -199755.64273833684}, {99968.72082566434, -199773.46729270133},
       {99970.37179096838, -199774.04577385314}, {99970.56829044632, -199775.11274937057},
       {99970.95578075679, -199774.82075447775}, {99971.92726086052, -199775.30374020585},
       {99972.41575522738, -199776.75820603862}, {99971.93276950107, -199777.7296861372}}};
  const auto plan = HipRoofPlan::Prepare(ring, {}, {});
  CHECK(plan.has_value(), "near-coplanar contacts retain separate valid ridge junctions");
  if (!plan) { return Report(); }
  CHECK(plan->Facets().size() == ring.size(), "every original eave owns a facet");
  std::map<std::pair<double, double>, double> distances;
  for (const auto &facet : plan->Facets()) {
    for (const auto point : facet.Ring) {
      const auto value = facet.DistanceAt(point);
      const auto [at, inserted] = distances.emplace(std::pair{point.EastM, point.NorthM}, value);
      if (!inserted) {
        CHECK_NEAR(value,
                   at->second,
                   plan->PrecisionM(),
                   "metres",
                   "shared ridge nodes retain the public plane bound");
      }
    }
  }
  return Report();
}
