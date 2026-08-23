#include <cstdio>
#include <cstring>
#include <string>

#include "Check.h"

#include "TreeSpecies.h"

using outshine::Generators::TreeSpecies;

namespace {

[[nodiscard]] bool Parsed(const char *text, TreeSpecies &species) {
  return species.Parse(text, std::strlen(text));
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  {
    TreeSpecies fine;
    CHECK(Parsed("{\"name\":\"spruce\",\"height_m\":22,\"whorl_count\":5,"
                 "\"whorl_spacing\":3}",
                 fine),
          "an honest species parses");
  }
  {
    TreeSpecies bad;
    CHECK(!Parsed("{\"name\":\"spruce\",\"height_m\":22,\"whorl_count\":5,"
                  "\"whorl_spacing\":0}",
                  bad) &&
              bad.Error().find("divides by it") != std::string::npos,
        "**A WHORL EVERY ZERO STEPS REFUSES AT ASSEMBLY** naming both numbers -- never a "
        "SIGFPE mid-growth (board:1699)");
  }
  {
    TreeSpecies flat;
    CHECK(!Parsed("{\"name\":\"moss\",\"height_m\":0}", flat) &&
              flat.Error().find("height") != std::string::npos,
          "a tree without height refuses");
  }
  {
    TreeSpecies wild;
    CHECK(!Parsed("{\"name\":\"kraken\",\"height_m\":5,\"max_order\":40}", wild) &&
              wild.Error().find("max_order") != std::string::npos,
          "and an order past the grower's own bound refuses naming the bound");
  }

  Covers("II.10 a species that would divide by zero, grow without height or branch past the "
         "grower's bound refuses at parse, naming its numbers (board:1699)");
  return Report();
}
