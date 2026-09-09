#include <Outshine.h>
#include "Check.h"
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  std::vector<float> output{1, 2, 3};
  const auto colour = engine.renderer().readPixels(Buffer::Colour, output);
  CHECK(!colour && colour.error().find("RGBA8") != std::string::npos,
        "byte colour selection is refused before render-target preparation");
  const auto unknown = engine.renderer().readPixels(static_cast<Buffer>(255), output);
  CHECK(!unknown && unknown.error().find("unknown") != std::string::npos,
        "unknown selection is refused before render-target preparation");
  CHECK(output == std::vector<float>({1, 2, 3}), "invalid selection preserves caller output");
  return Report();
}
