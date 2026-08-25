#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "ContentStore.h"
#include "DeclaredSources.h"
#include "SourceSet.h"

using outshine::Provider;
using namespace outshine::Data;

namespace {

[[nodiscard]] ContentStore::Config StoreOff() {
  ContentStore::Config c;
  c.Using = ContentStore::Use::Off;
  return c;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string error;
  {
    ContentStore store(StoreOff());
    SourceSet sources(store);
    const std::vector<Provider> declared = {{"terrain", "2026-08-20", 0, "hand over"},
                                            {"vector", "", 1, "hand over"}};
    CHECK(RegisterDeclared(sources, declared, "src/assets/sky", error),
          "**A SCENARIO DECLARES ITS PROVIDERS AND THE CATALOGUE STANDS THEM** -- two "
          "kinds, two sources, no boolean that means a list somebody wrote down twice");
    CHECK(sources.Count() == 2, "exactly what was declared registered, nothing assumed");
  }
  {
    ContentStore store(StoreOff());
    SourceSet sources(store);
    CHECK(RegisterDeclared(sources, {}, "src/assets/sky", error) && sources.Count() == 0,
          "**ZERO PROVIDERS IS A VALID DECLARATION**: the engine registers nothing and "
          "assumes nothing -- a studio scenario needs no upstream");
  }
  {
    ContentStore store(StoreOff());
    SourceSet sources(store);
    const std::vector<Provider> wrong = {{"weather", "", 0, ""}};
    CHECK(!RegisterDeclared(sources, wrong, "", error) &&
              error.find("weather") != std::string::npos &&
              error.find("terrain") != std::string::npos,
          "**A KIND THE ENGINE DOES NOT CARRY REFUSES**, naming it and listing what it "
          "does carry");
  }
  {
    ContentStore store(StoreOff());
    SourceSet sources(store);
    const std::vector<Provider> doubled = {{"terrain", "", 0, ""}, {"terrain", "", 1, ""}};
    CHECK(!RegisterDeclared(sources, doubled, "", error) &&
              error.find("two providers") != std::string::npos,
          "two providers of one kind refuse -- a lookup with two answers has none");
  }
  {
    // the shipped battery is a CONVENIENCE the client selects; the engine never assumes it
    ContentStore store(StoreOff());
    SourceSet sources(store);
    CHECK(RegisterDeclared(sources, ShippedProviders(), "src/assets/sky", error) &&
              sources.Count() == 3,
          "the shipped terrain-vector-stars battery registers when SELECTED");
  }

  Covers("I.54 providers are declared by the scenario rather than compiled in: the "
         "catalogue stands what is declared, refuses a stranger naming its own kinds, zero "
         "is valid, and WithUpstreams is gone -- the stars provider is file-backed, so the "
         "suite's verdict owes nothing to the weather (board:1494)");
  return Report();
}
