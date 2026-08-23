#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "Check.h"

#include "ScenarioLayer.h"
#include "ScenarioRead.h"

using outshine::ApplyLayer;
using outshine::ReadScenario;
using outshine::Scenario;

namespace {

size_t gAllocations = 0;
void *operator_new(size_t bytes) {
  ++gAllocations;
  void *held = std::malloc(bytes == 0 ? 1 : bytes);
  if (held == nullptr) { std::abort(); }
  return held;
}

const char *kBase =
    "<scenario name=\"town\" version=\"3\" active=\"winter\" epoch=\"1200\">"
    "<world lat=\"52\" lon=\"0\"/>"
    "<assets><asset uri=\"car.glb\" digest=\"aaa\"/></assets>"
    "</scenario>";

} // namespace

void *operator new(size_t bytes) { return operator_new(bytes); }
void *operator new[](size_t bytes) { return operator_new(bytes); }
void operator delete(void *held) noexcept { std::free(held); }
void operator delete[](void *held) noexcept { std::free(held); }
void operator delete(void *held, size_t) noexcept { std::free(held); }
void operator delete[](void *held, size_t) noexcept { std::free(held); }

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string error;
  Scenario town;
  CHECK(ReadScenario(kBase, std::strlen(kBase), town, error), "the base reads");
  if (town.Assets.empty()) { return Report(); }

  // board:1681, reopened: a layer that mentions only <world lon> used to wipe name, version,
  // active and epoch (four attributes it never spelt) and double the asset list, because
  // ApplyLayer parsed the same bytes TWICE -- once into a fragment, once onto a full deep
  // copy of the base -- and correctness rested on nobody reading the wreckage.
  const char *layer = "<scenario><world lon=\"9\"/></scenario>";
  std::vector<std::string> trace;
  CHECK(ApplyLayer(town, layer, std::strlen(layer), "winter", trace, error),
        "a layer that declares one attribute applies");

  CHECK(town.Named.Name == "town" && town.Named.Version == "3" &&
            town.Named.Active == "winter" && town.Named.Epoch == 1200.0,
        "**A LAYER KEEPS WHAT IT OMITS**: name, version, active and epoch survive a layer "
        "that never mentions them");
  CHECK(town.Assets.size() == 1,
        "**AND THE ROWS ARE NOT DOUBLED**: one asset in, one asset out");
  CHECK(town.Ground.Lon == 9.0 && town.Ground.Lat == 52.0,
        "the attribute the layer DOES declare replaces, and its neighbour in the same "
        "section keeps the base's value");

  {
    // the cost is only visible against a base worth copying: 400 rows is a city block's
    // worth of parked cars, and the deleted deep copy touched every string in it. The URIs
    // are deliberately past the small-string bound, so a copied row IS an allocation and
    // the linear term cannot hide inside SSO.
    constexpr int kRows = 400;
    const std::string longEnough = "content/vehicles/parked/generation-two/";
    std::string big = "<scenario name=\"city\"><assets>";
    for (int at = 0; at < kRows; ++at) {
      big += "<asset uri=\"" + longEnough + "a" + std::to_string(at) + ".glb\" digest=\"" +
             longEnough + "d" + std::to_string(at) + "\"/>";
    }
    big += "</assets></scenario>";

    Scenario city;
    CHECK(ReadScenario(big.c_str(), big.size(), city, error), "the big base reads");
    CHECK(city.Assets.size() == (size_t)kRows, "with every row");

    const size_t from = gAllocations;
    std::vector<std::string> alsoTrace;
    CHECK(ApplyLayer(city, layer, std::strlen(layer), "winter", alsoTrace, error),
          "and the one-attribute layer applies over it");
    const size_t oneLayer = gAllocations - from;

    Note("rows in the base", (double)kRows, "rows");
    Note("allocations applying the layer", (double)oneLayer, "allocations");
    Note("allocations per row", (double)oneLayer / (double)kRows, "allocations/row");

    CHECK(city.Assets.size() == (size_t)kRows, "the rows still do not double at 400");
    CHECK(oneLayer < (size_t)kRows,
          "**A LAYER IS READ ONCE**: applying a one-attribute layer costs FEWER "
          "allocations than the base has rows -- the cost is in the LAYER, never in the "
          "base it sits on, so the second parse and the copy of every string are gone");
  }

  Covers("III.11 a layer is parsed ONCE and merged in place: what it omits keeps the base's "
         "value, its rows do not double, and the whole-Scenario copy the second parse needed "
         "is gone (board:1681)");
  return Report();
}
