#include "SubjectResidency.h"
#include "GpuPlacement.h"
#include "VertexUpload.h"
#include "Check.h"

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  SubjectResidency held;
  const auto unrelated = held.TakeVertices(1u << 20);
  const auto mapped = held.TakeVertices(3);
  const auto tangent = held.TakeTangents(3);
  CHECK(mapped.First >= unrelated.Count && tangent.First == 0 && tangent.Count == 3,
        "a late mapped mesh reserves no tangent slots for unrelated positions");
  GpuPlacement placement;
  placement.TangentOffset = tangent.First - mapped.First;
  for (uint32_t vertex = 0; vertex < 3; ++vertex) {
    CHECK(placement.TangentOffset + mapped.First + vertex == tangent.First + vertex,
          "GPU addressing preserves each tangent at a nonzero global vertex index");
  }
  held.GiveTangents(tangent);
  const auto recycled = held.TakeTangents(3);
  CHECK(recycled.First == tangent.First && held.TangentRoom() == 3,
        "released tangent slots are reused without growing the dense arena");
  const float source = -1.0f;
  const auto upload = VertexCrossing({.Which = SubjectResidency::Stream::Tangent,
                                      .Source = {.From = &source},
                                      .Carried = true,
                                      .Components = 4},
                                     recycled);
  CHECK(upload && upload->Usage == SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ &&
            upload->Bytes == 48 && upload->Offset == 0 && upload->From == &source,
        "three tangents transfer exactly 48 bytes independently of the position prefix");
  return Report();
}
