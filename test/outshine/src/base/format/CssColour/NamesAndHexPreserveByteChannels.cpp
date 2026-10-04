#include "src/base/format/CssColour.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(ParseCssColour(" white ") == 0xffffffffu, "named white is opaque, ignoring outer space");
  CHECK(ParseCssColour("WhItE") == 0xffffffffu, "CSS names ignore ASCII case");
  CHECK(ParseCssColour("burlywood") == 0xdeb887ffu, "delivered timber paint keeps its CSS bytes");
  CHECK(ParseCssColour(" BisQue ") == 0xffe4c4ffu,
        "delivered plaster paint retains mixed-case names");
  CHECK(ParseCssColour("wheat") == 0xf5deb3ffu, "delivered warm facade paint is recognized");
  CHECK(ParseCssColour("lightgoldenrodyellow") == 0xfafad2ffu,
        "long standard names retain every channel");
  CHECK(ParseCssColour("rebeccapurple") == 0x663399ffu, "CSS4's additional name is recognized");
  CHECK(ParseCssColour("darkslategrey") == ParseCssColour("darkslategray"),
        "standard gray and grey aliases agree");
  CHECK(ParseCssColour("#d4c") == 0xdd44ccffu, "short hex expands every nibble independently");
  CHECK(ParseCssColour("#bd8161") == 0xbd8161ffu, "the delivered RGB spelling preserves its bytes");
  CHECK(ParseCssColour("#A1b2C3d4") == 0xa1b2c3d4u, "eight hex digits retain explicit alpha");
  CHECK(ParseCssColour("transparent") == 0u, "transparent is a value, not a missing colour");
  for (const auto text : {"", "#", "#12", "#12345", "#00000g", "unknown-colour"}) {
    CHECK(!ParseCssColour(text), "invalid spellings remain distinguishable from black");
  }
  return Report();
}
