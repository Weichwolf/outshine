#include "Xml.h"
#include "XmlAttribute.h"
#include "Check.h"
#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::string scratch;
  const std::string large(4096, 'x');
  CHECK(DecodeXmlAttribute(large, scratch) && scratch == large, "long attribute prepares scratch");
  const char *storage = scratch.data();
  const size_t capacity = scratch.capacity();
  CHECK(DecodeXmlAttribute("&amp;", scratch) && scratch == "&" && scratch.data() == storage &&
            scratch.capacity() == capacity,
        "short decoding reuses storage and clears prior text");
  CHECK(!DecodeXmlAttribute("prefix&bad;", scratch), "bad reference fails with reusable scratch");
  CHECK(DecodeXmlAttribute("", scratch) && scratch.empty() && scratch.capacity() == capacity,
        "empty retry clears partial text and retains storage");
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
