#include "DrawList.h"

#include <algorithm>

namespace outshine::Render {

namespace {

bool SameState(const DrawBatch &batch, const DrawItem &item) {
  return batch.MaterialSlot == item.Order.MaterialSlot && batch.Layout == item.Layout &&
         batch.Kind == item.Order.Surface.Kind() && batch.ModelSlot == item.ModelSlot;
}

}

bool DrawList::Add(const DrawItem &item, std::string &error) {
  if (item.IndexCount == 0) {
    error = "a draw of no indices was added to the list, and a draw that covers nothing is a draw "
            "that cannot be seen to be missing";
    return false;
  }
  if (item.IndexCount % 3 != 0) {
    error = "a draw of " + std::to_string(item.IndexCount) +
            " indices is not a whole number of triangles";
    return false;
  }
  if (item.Order.MaterialSlot >= kMaterialSlots) {
    error = "the draw names material slot " + std::to_string(item.Order.MaterialSlot) +
            " and a draw key addresses " + std::to_string(kMaterialSlots);
    return false;
  }
  if (item.Order.Viewport >= kViewportSlots) {
    error = "the draw names viewport " + std::to_string(item.Order.Viewport) +
            " and a draw key addresses " + std::to_string(kViewportSlots);
    return false;
  }

  Draws_.push_back(item);
  Draws_.back().Submitted = (uint32_t)Draws_.size() - 1u;
  return true;
}

void DrawList::Clear() {
  Draws_.clear();
  Runs_.clear();
  Batches_.clear();
  IndexCount_ = 0;
}

void DrawList::Compile() {

  std::sort(Draws_.begin(), Draws_.end(), [](const DrawItem &a, const DrawItem &b) {
    const DrawKey left = DrawKey::Of(a.Order), right = DrawKey::Of(b.Order);
    return left == right ? a.Submitted < b.Submitted : left < right;
  });

  Runs_.clear();
  Batches_.clear();
  Runs_.reserve(Draws_.size());
  IndexCount_ = 0;
  for (DrawItem &draw : Draws_) {
    draw.FirstIndex = IndexCount_;
    Runs_.push_back({draw.SourceFirstIndex, draw.IndexCount});
    IndexCount_ += draw.IndexCount;
    if (!Batches_.empty() && SameState(Batches_.back(), draw) &&
        Batches_.back().FirstIndex + Batches_.back().IndexCount == draw.FirstIndex) {
      Batches_.back().IndexCount += draw.IndexCount;
      ++Batches_.back().Draws;
      continue;
    }
    Batches_.push_back({draw.FirstIndex, draw.IndexCount, draw.Order.MaterialSlot, draw.Layout,
                        draw.Order.Surface.Kind(), 1, draw.ModelSlot});
  }
}

}
