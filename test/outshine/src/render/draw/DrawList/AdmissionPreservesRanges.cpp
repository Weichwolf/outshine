#include "DrawList.h"
#include "Check.h"
#include <limits>
#include <string>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  constexpr uint32_t maximum = std::numeric_limits<uint32_t>::max();
  constexpr uint32_t largestTriangles = (maximum / sizeof(uint32_t) / 3) * 3;
  std::string error;
  DrawList list;
  DrawItem item{.IndexCount = largestTriangles};
  CHECK(list.Add(item, error), "largest whole-triangle index buffer is admitted");
  list.Compile();
  item.IndexCount = 3;
  CHECK(!list.Add(item, error), "cumulative index bytes cannot exceed GPU addressing");
  CHECK(list.Draws().size() == 1 && list.IndexCount() == largestTriangles,
        "failed admission preserves source and compiled state");
  list.Clear();
  CHECK(list.Add(item, error), "clear resets admission budgets");
  for (unsigned field = 0; field < 3; ++field) {
    DrawItem invalid{.IndexCount = 3};
    if (field == 0) { invalid.SourceFirstIndex = maximum - 1; }
    if (field == 1) { invalid.ModelSlot = maximum; }
    if (field == 2) {
      invalid.FirstCluster = maximum;
      invalid.ClusterCount = 1;
    }
    CHECK(!list.Add(invalid, error), "overflowing source, model or cluster end is rejected");
    CHECK(list.Draws().size() == 1, "rejected range does not append a draw");
  }
  list.Compile();
  CHECK(list.Add(item, error), "compile retains capacity for subsequent valid admission");
  list.Compile();
  CHECK(list.IndexCount() == 6 && list.Batches().size() == 1,
        "valid draws still batch into two triangles");
  DrawItem jobs{.IndexCount = 3,
                .ClusterCount = maximum / (DrawList::kJobWords * sizeof(uint32_t))};
  list.Clear();
  CHECK(list.Add(jobs, error), "maximum job byte budget is admitted without allocating jobs");
  jobs.ClusterCount = 1;
  CHECK(!list.Add(jobs, error), "cumulative job bytes are bounded before compilation");
  list.Clear();
  CHECK(list.Add(jobs, error), "clear resets job budget");
  return Report();
}
