#include "Json.h"
#include "Check.h"
#include <limits>
#include <cstddef>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Json document;
  constexpr std::string_view valid =
      R"({"array":[true,false,null,-12.5e+2,{"escaped\u0020key":"\"\\\/\b\f\n\r\t\u0041\uD83D\uDE00"}]})";
  CHECK(document.Parse(valid.data(), valid.size()), "nested RFC grammar parses");
  const auto array = document.Root()["array"];
  CHECK(array.Size() == 5 && array[size_t{0}].Bool() && !array[1u].Bool() &&
            array[2u].GetKind() == Json::Kind::Null && array[3u].Num() == -1250,
        "containers retain boolean, null and exponent values");
  CHECK(array[4u]["escaped key"].Str() == "\"\\/\b\f\n\r\tA\xf0\x9f\x98\x80",
        "JSON escapes and surrogate pair decode to independently specified UTF-8");
  for (const std::string_view invalid : {R"("\q")",
                                         R"("\u12xz")",
                                         R"("\u123")",
                                         "\"raw\nline\"",
                                         "\"raw\tcolumn\"",
                                         R"({"bad\x":0})",
                                         "[1,]",
                                         "{\"x\":1,}",
                                         "[true false]",
                                         "trueX",
                                         "01",
                                         "1.",
                                         "1e+",
                                         "-",
                                         "[1",
                                         "{\"x\":2} trailing"}) {
    CHECK(!document.Parse(invalid.data(), invalid.size()) && !document.Ok(),
          "invalid strings, separators and numbers are rejected");
    CHECK(!document.Root().Valid() && document.Root().Size() == 0,
          "failed parse cannot expose a partial document");
  }
  CHECK(!document.Parse(nullptr, 0) && !document.Root().Valid(), "null input has no document");
  CHECK(!document.Parse("0", std::numeric_limits<size_t>::max()),
        "unaddressable text is rejected before copying input");
  constexpr std::string_view recovered = "[{},[],0]";
  CHECK(document.Parse(recovered.data(), recovered.size()) && document.Root().Size() == 3,
        "parser recovers after rejected input and accepts empty containers");
  const auto borrowed = document.Root().Source();
  CHECK(document.Parse(borrowed.data(), borrowed.size()) && document.Root().Size() == 3,
        "parsing the current source view preserves input until it has been copied");
  return Report();
}
