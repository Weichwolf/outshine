#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "Check.h"
#include "GroundMaterials.h"

namespace {

// THE ORACLE IS THE ORDERING, NOT THE VALUE. This tree's friction numbers are `[SET]` -- the
// centre of the published engineering range for each class, with no measurement carried here --
// so a case asserting that gravel is 0.55 would assert only that we agree with ourselves.
//
// What is NOT a matter of our choosing is the order. A tyre grips a dry bituminous carriageway
// better than it grips loose aggregate, loose aggregate better than saturated fine soil, and
// saturated fine soil better than a water film thick enough to lift the tread. Any table that
// inverts one of those pairs is wrong whatever its numbers are, and no re-measurement will ever
// reorder them: the mechanism is different in each band (adhesion on a coherent surface, shear
// within the layer on a loose one, hydrodynamic lift on a film).
//
// The second half is the DERIVATION. A ground contributes a FACTOR, not an absolute, because a
// tyre already carries the coefficient it was measured with -- on dry asphalt. So
//
//   frictionFactor = peakFriction / peakFriction(reference)
//
// and the reference class is exactly 1 by construction. The file declares only peakFriction; the
// factor exists once, in the reader. Declaring it in both places is one truth spelled twice, and
// the two spellings drift the first time somebody edits one.
//
// The negative control is a refusal: a table naming a reference class that does not exist has no
// scale at all, and the reader must say so rather than divide by a default.
constexpr double kExactly = 1e-6;

[[nodiscard]] float PeakOf(const outshine::Ground::GroundMaterials &table, const char *name) {
  const int at = table.Find(name);
  return at < 0 ? -1.0f : table.At((size_t)at).PeakFriction;
}

[[nodiscard]] float FactorOf(const outshine::Ground::GroundMaterials &table, const char *name) {
  const int at = table.Find(name);
  return at < 0 ? -1.0f : table.At((size_t)at).FrictionFactor;
}

[[nodiscard]] bool Wrote(const std::string &path, const std::string &held) {
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return false; }
  const bool whole = std::fwrite(held.data(), 1, held.size(), file) == held.size();
  return std::fclose(file) == 0 && whole;
}

[[nodiscard]] std::string Read(const std::string &path) {
  std::FILE *const file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) { return std::string(); }
  std::string held;
  char block[65536];
  size_t got = 0;
  while ((got = std::fread(block, 1, sizeof block, file)) > 0) { held.append(block, got); }
  std::fclose(file);
  return held;
}

}

int main(void) {
  using namespace outshine::Test;
  using outshine::Ground::GroundMaterials;

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case writes a broken table into the runner's nest and was given none");
    return Report();
  }

  const std::string declared = "src/assets/world/ground-materials.json";
  GroundMaterials table;
  if (!table.Load(declared.c_str())) {
    Unprepared(("the declared ground materials do not load: " + table.Error()).c_str());
    return Report();
  }

  std::printf("CLASSES %zu\n", table.Count());
  const char *const band[] = {"asphalt", "crushed_stone", "gravel", "grass_thatch", "mud", "water"};
  for (const char *one : band) {
    std::printf("  %-14s peak %.3f   factor %.4f\n", one, (double)PeakOf(table, one),
                (double)FactorOf(table, one));
  }

  bool everyClassCarriesOne = true;
  for (size_t at = 0; at < table.Count(); ++at) {
    everyClassCarriesOne = everyClassCarriesOne && table.At(at).PeakFriction > 0.0f &&
                           table.At(at).FrictionFactor > 0.0f;
  }
  CHECK(everyClassCarriesOne,
        "every declared ground class carries a positive peak friction, so a wheel that leaves "
        "the made surface finds a number under it whatever it drove onto");

  CHECK(PeakOf(table, "asphalt") > PeakOf(table, "crushed_stone") &&
            PeakOf(table, "crushed_stone") > PeakOf(table, "gravel") &&
            PeakOf(table, "gravel") > PeakOf(table, "grass_thatch") &&
            PeakOf(table, "grass_thatch") > PeakOf(table, "mud") &&
            PeakOf(table, "mud") > PeakOf(table, "water"),
        "**THE ORDER IS THE PHYSICS**: coherent bituminous surface, then interlocking aggregate, "
        "then rolling aggregate, then vegetation over soil, then saturated soil, then a water "
        "film -- three different mechanisms in one chain and no measurement will reorder them");

  CHECK(std::fabs((double)FactorOf(table, "asphalt") - 1.0) < kExactly,
        "the reference class is exactly 1 by construction, because a tyre's own coefficient was "
        "measured against it");
  CHECK(std::fabs((double)(FactorOf(table, "gravel") * PeakOf(table, "asphalt")) -
                  (double)PeakOf(table, "gravel")) < kExactly,
        "and every other factor is that class's peak divided by the reference's, DERIVED by the "
        "reader from the one number the file declares rather than declared a second time beside "
        "it");

  const std::string held = Read(declared);
  CHECK(held.find("\"frictionFactor\"") == std::string::npos,
        "the file itself carries no frictionFactor: one truth is spelled once, and a derived "
        "number that is also declared drifts from its derivation the first time either moves");

  const std::string broken = std::string(nest) + "/ground-materials-no-reference.json";
  std::string bent = held;
  const size_t names = bent.find("\"reference\": \"asphalt\"");
  if (names == std::string::npos) {
    Unprepared("the declared table names no reference class, so the control cannot be built");
    return Report();
  }
  bent.replace(names, std::string("\"reference\": \"asphalt\"").size(),
               "\"reference\": \"tarmacadam\"");
  if (!Wrote(broken, bent)) {
    Unprepared("the bent table could not be written into the nest");
    return Report();
  }

  GroundMaterials refused;
  const bool stood = refused.Load(broken.c_str());
  std::printf("REFERENCE 'tarmacadam' -> %s: %s\n", stood ? "STOOD" : "REFUSED",
              refused.Error().c_str());
  CHECK(!stood,
        "a table whose reference class does not exist is REFUSED with a reason: there is no "
        "scale to divide by, and a reader that fell back on a default would give every ground "
        "in the file a silently wrong grip");

  Covers("world: a ground class carries what it grips with as well as what it looks like, the "
         "order of those numbers is the physics and not our choosing, the factor a contact uses "
         "is derived from the one declared peak, and a table with no reference class is refused");
  return Report();
}
