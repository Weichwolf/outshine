#include "Xml.h"
#include "Check.h"
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Xml xml;
  for (const std::string_view text : {"<root/>",
                                      "<root />",
                                      "<root a='1'/>",
                                      "<root></root>",
                                      "<root></root \t\r\n>",
                                      "<root><child/><child></child ></root>"}) {
    CHECK(xml.Parse(text.data(), text.size()), "valid tag boundary is accepted");
  }
  for (const std::string_view text : {"<root></root junk>",
                                      "<root></root/>",
                                      "<root></root a='1'>",
                                      "<root/ extra='1'>",
                                      "<root/ >",
                                      "<root//>",
                                      "<root/",
                                      "<root></root",
                                      "<root></root ",
                                      "<root /\n>",
                                      "<root/<child/>",
                                      "<root><child/ junk='1'></root>"}) {
    CHECK(!xml.Parse(text.data(), text.size()), "malformed tag boundary is rejected");
  }
  return Report();
}
