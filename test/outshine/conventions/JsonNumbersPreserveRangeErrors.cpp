#include "Json.h"
#include "Check.h"
#include <cmath>
#include <limits>
#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Json document;
  for (const std::string_view number : {"1e309", "-1e309", "1e9999", "1e-9999", "-1e-9999"}) {
    for (const std::string &text :
         {std::string(number), "{\"value\":" + std::string(number) + "}"}) {
      CHECK(!document.Parse(text.data(), text.size()) && !document.Root().Valid(),
            "numeric range failure rejects both root numbers and enclosing documents");
    }
  }

  struct Example {
    std::string_view Text;
    double Value;
  };

  const Example accepted[] = {
      {"1.7976931348623157e308", std::numeric_limits<double>::max()},
      {"-1.7976931348623157e308", -std::numeric_limits<double>::max()},
      {"2.2250738585072014e-308", std::numeric_limits<double>::min()},
      {"4.9406564584124654e-324", std::numeric_limits<double>::denorm_min()},
      {"-0", -0.0},
      {"0", 0.0},
  };
  for (const auto &example : accepted) {
    CHECK(document.Parse(example.Text.data(), example.Text.size()),
          "representable finite endpoint parses after rejected documents");
    CHECK(document.Root().Num() == example.Value &&
              std::signbit(document.Root().Num()) == std::signbit(example.Value),
          "finite endpoint and signed zero retain their IEEE double value");
  }
  return Report();
}
