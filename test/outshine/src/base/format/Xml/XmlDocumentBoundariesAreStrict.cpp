#include "Xml.h"
#include "Check.h"
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Xml xml;
  for (const std::string_view text : {"<root/>",
                                      " \t\r\n<root/> \n",
                                      "<!--before--><root a='1' b='2'/><!--after-->",
                                      "<root a = '1'\tb=\"2\" />",
                                      "<root>text<!--ok-->more</root>",
                                      "\xEF\xBB\xBF<root/>"}) {
    CHECK(xml.Parse(text.data(), text.size()), "valid document boundary accepted");
  }
  for (const std::string_view text : {"junk<root/>",
                                      "<root/>junk",
                                      "<root/> &amp;",
                                      "<root a='1'b='2'/>",
                                      "<root a=\"1\"b=\"2\"></root>",
                                      "<!--bad--comment--><root/>",
                                      "<root><!--bad---></root>",
                                      "<root/><!--bad--comment-->",
                                      " <root/>\xEF\xBB\xBF"}) {
    CHECK(!xml.Parse(text.data(), text.size()), "malformed document rejected");
    CHECK(!xml.Root().Valid() && xml.NodeCount() == 0 && !xml.Error().empty(),
          "failed parse exposes no partial nodes and supplies a diagnosis");
  }
  constexpr std::string_view valid = "<retry/>";
  CHECK(xml.Parse(valid.data(), valid.size()) && xml.Error().empty(), "valid retry clears error");
  return Report();
}
