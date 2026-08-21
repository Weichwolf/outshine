#include <string>
#include <vector>

#include "Check.h"

#include "DrawList.h"

using outshine::AlphaMode;
using outshine::Material;
using outshine::StateOf;
using outshine::Render::DrawBatch;
using outshine::Render::DrawItem;
using outshine::Render::DrawList;
using outshine::Render::ViewLayer;

namespace {

DrawItem Draw(uint32_t firstIndex, uint32_t indexCount, uint32_t modelSlot) {
  Material material;
  material.Alpha = AlphaMode::Opaque;
  DrawItem item;
  item.Order.Layer = ViewLayer::World;
  item.Order.Surface = StateOf(material);
  item.Order.DepthFraction = 0.5;
  item.Order.MaterialSlot = 0;
  item.SourceFirstIndex = firstIndex;
  item.IndexCount = indexCount;
  item.ModelSlot = modelSlot;
  return item;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string error;

  {
    DrawList one;
    CHECK(one.Add(Draw(0, 30, 0), error) && one.Add(Draw(30, 30, 0), error),
          "two draws that stand in the same place, share a surface and run on");
    one.Compile();
    Note("batches they compile into", (double)one.Batches().size(), "batches");
    CHECK(one.Batches().size() == 1,
          "**MERGE INTO ONE BATCH, WHICH IS THE BEHAVIOUR THIS MUST NOT COST.** A picture of one "
          "placed thing submits exactly what it submitted before a placement existed");
    if (one.Batches().size() == 1) {
      CHECK(one.Batches()[0].IndexCount == 60 && one.Batches()[0].ModelSlot == 0,
            "carrying both runs and the placement they share");
    }
  }

  {
    DrawList two;
    CHECK(two.Add(Draw(0, 30, 0), error) && two.Add(Draw(30, 30, 1), error),
          "and two draws alike in every way EXCEPT where they stand");
    two.Compile();
    Note("batches they compile into", (double)two.Batches().size(), "batches");
    CHECK(two.Batches().size() == 2,
          "**DO NOT MERGE, BECAUSE A BATCH IS ONE PLACEMENT.** Merging them would draw the car at "
          "the road's transform, and the state that decides a batch boundary now includes where "
          "the thing stands -- surface, layout, kind AND placement");
    if (two.Batches().size() == 2) {
      Note("the first batch's placement", (double)two.Batches()[0].ModelSlot, "slot");
      Note("the second's", (double)two.Batches()[1].ModelSlot, "slot");
      CHECK(two.Batches()[0].ModelSlot == 0 && two.Batches()[1].ModelSlot == 1,
            "and each carries the placement its draw declared, so the encoder pushes a transform "
            "per batch rather than one per picture");
      CHECK(two.Batches()[0].IndexCount == 30 && two.Batches()[1].IndexCount == 30,
            "with the index runs split where the placement changes and nowhere else");
    }
  }

  {
    DrawList back;
    CHECK(back.Add(Draw(0, 30, 1), error) && back.Add(Draw(30, 30, 0), error) &&
              back.Add(Draw(60, 30, 1), error),
          "and three draws that alternate between two placements");
    back.Compile();
    Note("batches", (double)back.Batches().size(), "batches");
    CHECK(back.Batches().size() == 3,
          "**COMPILE TO THREE AND NOT TWO.** A batch is a contiguous index run, so a placement "
          "that comes back is a new batch rather than a re-entry into an old one -- which is what "
          "makes the encoder's push-when-it-changes correct rather than nearly correct");
  }

  Covers("I.9.4 a draw carries the placement of the thing it belongs to, and a batch never spans "
         "two placements -- so one picture can hold a road and a car standing in different places");
  return Report();
}
