#include "src/base/format/CssColour.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(ParseCssColour(" white ") == 0xffffffffu, "named white is opaque, ignoring outer space");
  CHECK(ParseCssColour("WhItE") == 0xffffffffu, "CSS names ignore ASCII case");
  CHECK(ParseCssColour("#d4c") == 0xdd44ccffu, "short hex expands every nibble independently");
  CHECK(ParseCssColour("#bd8161") == 0xbd8161ffu, "the delivered RGB spelling preserves its bytes");
  CHECK(ParseCssColour("#A1b2C3d4") == 0xa1b2c3d4u, "eight hex digits retain explicit alpha");
  CHECK(ParseCssColour("transparent") == 0u, "transparent is a value, not a missing colour");
  for (const auto text : {"", "#", "#12", "#12345", "#00000g", "unknown-colour"}) {
    CHECK(!ParseCssColour(text), "invalid spellings remain distinguishable from black");
  }
  return Report();
}
