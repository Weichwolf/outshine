#include "Xml.h"
#include "Check.h"
#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Xml xml;
  const std::string text =
      "<a v=\"&amp;&lt;&gt;&quot;&apos;&#65;&#x1F600;&amp;lt;\" w=\"a\t\r\nb&#9;&#10;&#13;\"/>";
  CHECK(xml.Parse(text.data(), text.size()), "valid references parse");
  CHECK(xml.Root().Attr("v") == "&<>\"'A\xF0\x9F\x98\x80&lt;",
        "entities decode once, including supplementary Unicode");
  CHECK(xml.Root().Attr("w") == "a  b\t\n\r",
        "literal whitespace normalizes but character references retain whitespace");
  for (const std::string_view value :
       {"&bogus;", "&amp", "&#0;", "&#xD800;", "&#x110000;", "&#xFFFE;", "&#-1;", "&#;", "<"}) {
    const std::string bad = "<a v=\"" + std::string(value) + "\"/>";
    CHECK(!xml.Parse(bad.data(), bad.size()), "invalid reference or literal is rejected");
  }
  return Report();
}
