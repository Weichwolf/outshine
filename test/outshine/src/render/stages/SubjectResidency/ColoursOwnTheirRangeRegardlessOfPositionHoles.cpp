#include "SubjectResidency.h"
#include "GpuPlacement.h"
#include "VertexUpload.h"
#include "Check.h"

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  SubjectResidency held;
  const auto unrelated = held.TakeVertices(1u << 20);
  const auto painted = held.TakeVertices(3);
  const auto colour = held.TakeColours(3);
  CHECK(painted.First >= unrelated.Count && colour.First == 0 && colour.Count == 1024,
        "millions of uncoloured positions reserve no optional colour holes");
  GpuPlacement placement;
  placement.ColourOffset = colour.First - painted.First;
  for (uint32_t vertex = 0; vertex < 3; ++vertex) {
    CHECK(placement.ColourOffset + painted.First + vertex == colour.First + vertex,
          "unsigned GPU addressing maps global positions to exact dense factors");
  }
  held.GiveColours(colour);
  const auto recycled = held.TakeColours(3);
  CHECK(recycled.First == colour.First && recycled.Count == colour.Count,
        "released colour storage is reused independently of vertex lifetimes");
  const float source = 1.5f;
  const auto transfer = VertexCrossing({.Which = SubjectResidency::Stream::Colour,
                                        .Source = {.From = &source},
                                        .Carried = true,
                                        .Components = 4},
                                       {.First = recycled.First, .Count = 3});
  CHECK(transfer && transfer->Usage == SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ &&
            transfer->From == &source && transfer->Bytes == 48 && transfer->Offset == 0,
        "float factors above one retain exact source data and storage-read usage");
  return Report();
}
