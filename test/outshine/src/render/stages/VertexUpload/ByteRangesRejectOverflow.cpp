#include "VertexUpload.h"
#include "Check.h"
#include <limits>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  const float source = 1;
  for (const uint32_t components : {2u, 3u, 4u}) {
    VertexStreamUpload upload{.Which = SubjectResidency::Stream::Vertex,
                              .Source = {.From = &source},
                              .Carried = true,
                              .Components = components};
    const auto maximum = std::numeric_limits<uint32_t>::max() / (components * sizeof(float));
    const auto exact =
        VertexCrossing(upload, {.First = 1, .Count = static_cast<uint32_t>(maximum - 1)});
    CHECK(exact.has_value() && exact->Offset == components * sizeof(float) &&
              exact->Bytes == (maximum - 1) * components * sizeof(float) && exact->From == &source,
          "largest complete vertex range preserves byte offset and extent");
    CHECK(!VertexCrossing(upload, {.First = 1, .Count = static_cast<uint32_t>(maximum)}),
          "combined end exceeding byte addressing is rejected");
    CHECK(!VertexCrossing(upload, {.First = 0, .Count = 1u << 30}),
          "wrapped zero byte count is rejected");
    CHECK(!VertexCrossing(upload, {.First = 1u << 30, .Count = 1}),
          "wrapped starting offset is rejected");
    upload.Carried = false;
    const auto inactive = VertexCrossing(upload, {.First = 1u << 30, .Count = 1u << 30});
    CHECK(inactive.has_value() && inactive->Bytes == 0 && !inactive->Stands(),
          "inactive stream carries no bytes or producer");
  }
  return Report();
}
