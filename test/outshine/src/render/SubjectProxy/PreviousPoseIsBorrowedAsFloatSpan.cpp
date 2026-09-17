#include <array>
#include <span>
#include <vector>
#include "Check.h"
#include "SubjectProxy.h"

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  SubjectProxy proxy;
  std::array<float, 9> positions{{-1, -1, 0, 1, -1, 0, 0, 1, 0}};
  proxy.Posed(positions);
  CHECK(proxy.Previous().data() == positions.data() && proxy.Previous().size() == positions.size(),
        "the proxy borrows the native float vertex sequence without conversion or ownership");
  positions[0] = 3;
  CHECK(proxy.Previous().front() == 3,
        "the proxy observes the owner-selected rendered pose until its next explicit replacement");
  proxy.Posed({});
  CHECK(proxy.Previous().empty(),
        "an absent rendered pose selects the renderer's current-position fallback");
  return Report();
}
