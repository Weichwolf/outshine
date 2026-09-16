#include "DeclaredSources.h"
#include "Check.h"
#include <array>
#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  const std::array<Scenario::Provider, 3> declared = {{
      {.Kind = "terrain", .Pin = "dem-older", .Rank = 8, .WhenAbsent = "hand over"},
      {.Kind = "terrain", .Pin = "dem-2026-09", .Rank = -9, .WhenAbsent = "fail"},
      {.Kind = "vector",
       .Pin = "osm-2026-09",
       .Rank = std::numeric_limits<int>::max(),
       .WhenAbsent = "hand over"},
  }};
  std::string error;
  CHECK(RegisterDeclared(sources, declared, "sky", error), error.c_str());
  CHECK(sources.Count() == declared.size(), "every declared source is registered");
  if (sources.Count() == declared.size()) {
    const SourceDecl &preferred = sources.At(0).Declaration();
    const SourceDecl &older = sources.At(1).Declaration();
    const SourceDecl &vector = sources.At(2).Declaration();
    CHECK(preferred.Revision == "dem-2026-09" && preferred.Order == Rank{-9} &&
              preferred.OnAbsent == AbsencePolicy::Refuse,
          "lower terrain rank and absence policy reach the native source first");
    CHECK(older.Revision == "dem-older" && older.Order == Rank{8} &&
              older.OnAbsent == AbsencePolicy::HandOver,
          "next terrain rank remains available as fallback");
    CHECK(vector.Revision == "osm-2026-09" &&
              vector.Order == Rank{std::numeric_limits<int>::max()} &&
              vector.OnAbsent == AbsencePolicy::HandOver,
          "vector declaration preserves an extreme rank");
    SourceDecl anotherRevision = preferred;
    anotherRevision.Revision = "dem-2026-10";
    CHECK(ContentKey(preferred, Address::Whole(0)) !=
              ContentKey(anotherRevision, Address::Whole(0)),
          "a pin change produces a different cache identity");
  }
  SourceSet rejected(store);
  const std::array<Scenario::Provider, 1> invalid = {{
      {.Kind = "terrain", .WhenAbsent = "retry forever"},
  }};
  error.clear();
  CHECK(!RegisterDeclared(rejected, invalid, "sky", error) && !error.empty() &&
            rejected.Count() == 0,
        "unknown absence policy is rejected before registration");
  return Report();
}
