#include <scenario/Scenario.h>
#include "Check.h"
#include <string_view>
#include <type_traits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  static_assert(std::is_same_v<decltype(Scenario::View{}.scene()), std::string_view>);
  static_assert(noexcept(Scenario::View{}.scene()));
  Scenario::View view;
  view.setScene("harbour");
  CHECK(view.scene() == "harbour", "scene exposes the owned label as a borrowed view");
  Covers("format-neutral borrowed scenario metadata without a string owner escape");
  return Report();
}
