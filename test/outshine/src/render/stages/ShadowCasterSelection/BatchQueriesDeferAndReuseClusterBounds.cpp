#include "Check.h"
#include "ShadowCasterSelection.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

struct Fixture {
  ShadowCasterSelection Selection;
  std::array<DrawBatch, 4> Batches{};
  std::array<GpuPlacement, 4> Rows{};
  std::array<GeometryClusterTemplate, 8> Jobs{};
  std::array<float, 24> Spheres{};

  Fixture() {
    Spheres[3] = Spheres[15] = 0.25f;
    Spheres[12] = 10;
    for (size_t at = 0; at < Batches.size(); ++at) {
      auto &batch = Batches[at];
      batch.FirstIndex = 12;
      batch.IndexCount = 6;
      batch.ModelSlot = static_cast<uint32_t>(at);
      batch.FirstJob = static_cast<uint32_t>(at * 2);
      batch.JobCount = 2;
      batch.WorldSphere = BatchSphere{static_cast<float>(at * 100 + 5), 0, 0, 6};
      Rows[at].Current = {
          1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, static_cast<float>(at * 100), 0, 0, 1};
      Jobs[at * 2] = {.SphereIndex = 0, .RelativeFirstIndex = 12, .IndexCount = 3};
      Jobs[at * 2 + 1] = {.SphereIndex = 1, .RelativeFirstIndex = 15, .IndexCount = 3};
    }
  }

  void Prepare() { Selection.Prepare(Batches, Jobs, Spheres, Rows, 0, true); }
};

void DeferredQueries() {
  Fixture f;
  f.Prepare();
  CHECK(f.Selection.Ready() && f.Selection.CachedRanges() == 0,
        "preparing roots neither qualifies nor transforms any cluster range");
  const size_t rootsOnly = f.Selection.HeapBytes();
  FrameContext frame;
  const Mat4 identity;
  std::ranges::copy(identity, frame.Mvp.begin());
  const BatchFrustum patch(frame, {});
  size_t rootsSelected = 0;
  for (size_t at = 0; at < f.Batches.size(); ++at) {
    if (patch.Outside(f.Batches[at].WorldSphere)) { continue; }
    ++rootsSelected;
    CHECK(f.Selection.Ranges(at).size() == 2, "only a selected root resolves its clusters");
  }
  CHECK(rootsSelected == 1 && f.Selection.CachedRanges() == 2,
        "coarse light-patch selection avoids all six unrelated cluster bounds");
  CHECK(f.Selection.HeapBytes() == rootsOnly + 8 * sizeof(ShadowCasterRange),
        "first demand admits the bounded resident range capacity without evaluating other leaves");
  const auto first = f.Selection.Ranges(0);
  const auto saved = first[0];
  const size_t retained = f.Selection.HeapBytes();
  CHECK(f.Selection.Ranges(0).data() == first.data() && f.Selection.CachedRanges() == 2 &&
            f.Selection.HeapBytes() == retained,
        "repeated cascade queries reuse the same resolved ranges and allocation");
  CHECK(f.Selection.Ranges(3).size() == 2 && f.Selection.CachedRanges() == 4,
        "another independently demanded root resolves exactly its own ranges");
  const auto again = f.Selection.Ranges(0);
  CHECK(again[0].First == saved.First && again[0].Count == saved.Count &&
            again[0].Bounds == saved.Bounds,
        "querying a cached root after another query retains its exact source records");
  f.Selection.Invalidate();
  CHECK(!f.Selection.Ready() && f.Selection.CachedRanges() == 0 && f.Selection.Ranges(0).empty(),
        "invalidated borrowed source tables cannot publish stale ranges");
  f.Prepare();
  CHECK(f.Selection.CachedRanges() == 0, "a new source revision starts without old leaf bounds");
  CHECK(f.Selection.Ranges(0).size() == 2 &&
            f.Selection.HeapBytes() == rootsOnly + 8 * sizeof(ShadowCasterRange),
        "accepted source revision reuses previously needed storage");
  for (size_t at = 0; at < f.Batches.size(); ++at) { (void)f.Selection.Ranges(at); }
  CHECK(f.Selection.CachedRanges() == 8 &&
            f.Selection.HeapBytes() == rootsOnly + 8 * sizeof(ShadowCasterRange),
        "fully demanded range storage is capped at the resident job count");
}

void SharedTemplates() {
  Fixture f;
  for (auto &batch : f.Batches) { batch.FirstJob = 0; }
  f.Selection.Prepare(f.Batches, std::span(f.Jobs).first(2), f.Spheres, f.Rows, 0, true);
  for (size_t at = 0; at < f.Batches.size(); ++at) {
    CHECK(f.Selection.Ranges(at).size() == 2, "each instance can reuse the same source templates");
  }
  CHECK(f.Selection.CachedRanges() == 8,
        "retained shadow bounds are budgeted by demanded instances without duplicating templates");
}

void ChangedSourcesAndFallbacks() {
  Fixture f;
  f.Prepare();
  CHECK(f.Selection.Ranges(1).size() == 2, "the original source revision resolves");
  f.Selection.Invalidate();
  f.Rows[1].Current[12] = -200;
  f.Batches[1].FirstIndex = 400;
  CHECK(f.Selection.Ranges(1).empty(),
        "invalidated input views are not read after source mutation");
  f.Rows[1].Current[12] = 200;
  f.Batches[1].FirstIndex = 12;
  f.Prepare();
  const auto changed = f.Selection.Ranges(1);
  CHECK(changed.size() == 2 && changed[0].Bounds.value_or(BatchSphere{})[0] == 200,
        "the next accepted revision replaces cached transforms");
  f.Jobs[0].IndexCount = 0;
  f.Prepare();
  CHECK(f.Selection.CachedRanges() == 0 && f.Selection.Ranges(0).empty() &&
            f.Selection.CachedRanges() == 0,
        "invalid coverage publishes no partial interval selection");
  CHECK(f.Selection.Ranges(2).size() == 2 && f.Selection.CachedRanges() == 2,
        "an invalid root does not prevent independent valid roots");
  f.Rows[2].Current[3] = 0.25f;
  f.Prepare();
  const auto unknown = f.Selection.Ranges(2);
  CHECK(unknown.size() == 2 && !unknown[0].Bounds && !unknown[1].Bounds,
        "unknown projective roots keep complete source intervals without false rejection");
  CHECK(f.Selection.Ranges(f.Batches.size()).empty(),
        "out-of-range roots retain the draw fallback");
  f.Selection.Invalidate();
  f.Batches[0].JobCount = std::numeric_limits<uint32_t>::max();
  f.Prepare();
  const size_t retained = f.Selection.HeapBytes();
  CHECK(f.Selection.Ranges(0).empty() && f.Selection.HeapBytes() == retained,
        "invalid counts are rejected before allocating or reading any source range");
}
}

int main() {
  DeferredQueries();
  ChangedSourcesAndFallbacks();
  SharedTemplates();
  return Report();
}
