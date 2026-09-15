#include "Xml.h"
#include "Check.h"
#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (bool fallback : {false, true}) {
    for (std::string_view token : {"yes", "true", "1", "no", "false", "0"}) {
      const std::string input = "<world enabled=\"" + std::string(token) + "\"/>";
      Xml xml;
      CHECK(xml.Parse(input.data(), input.size()), "flag fixture parses");
      const bool expected = token == "yes" || token == "true" || token == "1";
      CHECK(xml.Root().Flag("enabled", fallback) == expected,
            "explicit writer and legacy spellings override either fallback");
      CHECK(xml.Root().Flag("absent", fallback) == fallback,
            "absent attribute preserves the caller's fallback");
    }
  }
  Xml decoded;
  const std::string input = "<world on=\"y&#101;s\" off=\"n&#111;\"/>";
  CHECK(decoded.Parse(input.data(), input.size()), "encoded flag fixture parses");
  CHECK(decoded.Root().Flag("on", false) && !decoded.Root().Flag("off", true),
        "flag conversion uses decoded attribute values");
  return Report();
}
