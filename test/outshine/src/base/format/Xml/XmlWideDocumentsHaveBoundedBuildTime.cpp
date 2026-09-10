#include "Xml.h"
#include "Check.h"
#include <chrono>
#include <cstdio>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr size_t count = 60000;
  std::string text = "<root>";
  for (size_t i = 0; i < count; ++i) { text += "<item id='" + std::to_string(i) + "'/>"; }
  text += "</root>";
  Xml xml;
  const auto start = std::chrono::steady_clock::now();
  CHECK(xml.Parse(text.data(), text.size()), "wide valid document parses");
  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  std::printf("60000 siblings: %.6f s\n", seconds);
  CHECK(seconds < 2.0, "wide document fits the two-second cold import budget");
  CHECK(xml.NodeCount() == count + 1, "all nodes retained");
  size_t visited = 0;
  for (const auto child : xml.Root().Children("item")) {
    CHECK(child.Attr("id") == std::to_string(visited), "sibling order retained");
    ++visited;
  }
  CHECK(visited == count, "each sibling visited exactly once");
  const std::string nested = "<root><a><x/><y/></a><b/><c><z/></c></root>";
  CHECK(xml.Parse(nested.data(), nested.size()), "parser state resets for nested document");
  CHECK(xml.Root().Count("a") == 1 && xml.Root().Count("b") == 1 && xml.Root().Count("c") == 1,
        "root sibling chain remains intact");
  CHECK(xml.Root().Child("a").Count("x") == 1 && xml.Root().Child("a").Count("y") == 1 &&
            xml.Root().Child("c").Count("z") == 1,
        "each depth keeps its own sibling chain");
  return Report();
}
