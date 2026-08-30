#include "DrawList.h"

#include <algorithm>

namespace outshine::Render {

namespace {

bool SameState(const DrawBatch &batch, const DrawItem &item) {
  return batch.MaterialSlot == item.Order.MaterialSlot && batch.Layout == item.Layout &&
         batch.Kind == item.Order.Surface.Kind() && batch.ModelSlot == item.ModelSlot &&
         batch.Instances == item.Instances;
}

} // namespace

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
  Jobs_.clear();
  IndexCount_ = 0;
}

void DrawList::Compile() {
  std::sort(Draws_.begin(), Draws_.end(), [](const DrawItem &a, const DrawItem &b) {
    const DrawKey left = DrawKey::Of(a.Order), right = DrawKey::Of(b.Order);
    return left == right ? a.Submitted < b.Submitted : left < right;
  });

  Runs_.clear();
  Batches_.clear();
  Jobs_.clear();
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
    Batches_.push_back({draw.FirstIndex,
                        draw.IndexCount,
                        draw.Order.MaterialSlot,
                        draw.Layout,
                        draw.Order.Surface.Kind(),
                        1,
                        draw.ModelSlot,
                        draw.Instances,
                        0,
                        0});
  }

  size_t at = 0;
  for (DrawBatch &batch : Batches_) {
    batch.FirstJob = (uint32_t)(Jobs_.size() / kJobWords);
    const size_t upTo = at + batch.Draws;

    bool wholly = batch.Instances == 1;
    for (size_t look = at; look < upTo && look < Draws_.size() && wholly; ++look) {
      wholly = Draws_[look].ClusterCount > 0;
    }
    for (; at < upTo && at < Draws_.size(); ++at) {
      if (!wholly) { continue; }
      const DrawItem &draw = Draws_[at];
      for (uint32_t one = 0; one < draw.ClusterCount; ++one) {
        Jobs_.push_back(draw.FirstCluster + one);
        Jobs_.push_back((uint32_t)(&batch - Batches_.data()));
        Jobs_.push_back(draw.FirstIndex);
        Jobs_.push_back(0u);
      }
    }
    batch.JobCount = (uint32_t)(Jobs_.size() / kJobWords) - batch.FirstJob;
  }
}

void DrawList::JobsAddress(std::span<const DagCluster> clusters) {
  size_t at = 0;
  for (const DrawBatch &batch : Batches_) {
    const size_t upTo = at + batch.Draws;
    uint32_t job = batch.FirstJob;
    for (; at < upTo && at < Draws_.size(); ++at) {
      if (batch.JobCount == 0) { continue; }
      const DrawItem &draw = Draws_[at];
      for (uint32_t one = 0; one < draw.ClusterCount; ++one, ++job) {
        const size_t cluster = (size_t)draw.FirstCluster + one;
        if (cluster >= clusters.size()) { continue; }
        Jobs_[(size_t)job * kJobWords + 2u] =
            draw.FirstIndex + (clusters[cluster].First - draw.SourceFirstIndex);
        Jobs_[(size_t)job * kJobWords + 3u] = clusters[cluster].Count;
      }
    }
  }
}

} // namespace outshine::Render
