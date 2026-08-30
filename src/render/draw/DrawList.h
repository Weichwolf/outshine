#ifndef OUTSHINE_RENDER_DRAW_DRAWLIST_H
#define OUTSHINE_RENDER_DRAW_DRAWLIST_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <span>

#include "ClusterDag.h"
#include "DrawKey.h"

namespace outshine::Render {

enum class VertexLayout : uint8_t {
  Position,
  PositionUv,
  PositionUvUv1,
  PositionNormal,
  PositionNormalUv,
  PositionNormalUvUv1,
  PositionNormalUvTangent,
  PositionNormalUvUv1Tangent,
  PositionColour,
  PositionUvColour,
  PositionUvUv1Colour,
  PositionNormalColour,
  PositionNormalUvColour,
  PositionNormalUvUv1Colour,
  PositionNormalUvTangentColour,
  PositionNormalUvUv1TangentColour
};

enum class VertexAttribute : uint8_t {
  None = 0,
  Uv = 1u << 0,
  Uv1 = 1u << 1,
  Normal = 1u << 2,
  Tangent = 1u << 3,
  Colour = 1u << 4
};

[[nodiscard]] constexpr VertexAttribute operator|(VertexAttribute a, VertexAttribute b) {
  return static_cast<VertexAttribute>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

[[nodiscard]] constexpr bool Holds(VertexAttribute set, VertexAttribute one) {
  return (static_cast<uint8_t>(set) & static_cast<uint8_t>(one)) != 0;
}

struct VertexLayoutRow {
  VertexLayout Layout = VertexLayout::Position;
  VertexAttribute Carries = VertexAttribute::None;
};

inline constexpr std::array kVertexLayouts = {
    VertexLayoutRow{VertexLayout::Position, VertexAttribute::None},
    VertexLayoutRow{VertexLayout::PositionUv, VertexAttribute::Uv},
    VertexLayoutRow{VertexLayout::PositionUvUv1, VertexAttribute::Uv | VertexAttribute::Uv1},
    VertexLayoutRow{VertexLayout::PositionNormal, VertexAttribute::Normal},
    VertexLayoutRow{VertexLayout::PositionNormalUv, VertexAttribute::Uv | VertexAttribute::Normal},
    VertexLayoutRow{VertexLayout::PositionNormalUvUv1,
                    VertexAttribute::Uv | VertexAttribute::Uv1 | VertexAttribute::Normal},
    VertexLayoutRow{VertexLayout::PositionNormalUvTangent,
                    VertexAttribute::Uv | VertexAttribute::Normal | VertexAttribute::Tangent},
    VertexLayoutRow{VertexLayout::PositionNormalUvUv1Tangent,
                    VertexAttribute::Uv | VertexAttribute::Uv1 | VertexAttribute::Normal |
                        VertexAttribute::Tangent},
    VertexLayoutRow{VertexLayout::PositionColour, VertexAttribute::Colour},
    VertexLayoutRow{VertexLayout::PositionUvColour, VertexAttribute::Uv | VertexAttribute::Colour},
    VertexLayoutRow{VertexLayout::PositionUvUv1Colour,
                    VertexAttribute::Uv | VertexAttribute::Uv1 | VertexAttribute::Colour},
    VertexLayoutRow{VertexLayout::PositionNormalColour,
                    VertexAttribute::Normal | VertexAttribute::Colour},
    VertexLayoutRow{VertexLayout::PositionNormalUvColour,
                    VertexAttribute::Uv | VertexAttribute::Normal | VertexAttribute::Colour},
    VertexLayoutRow{VertexLayout::PositionNormalUvUv1Colour,
                    VertexAttribute::Uv | VertexAttribute::Uv1 | VertexAttribute::Normal |
                        VertexAttribute::Colour},
    VertexLayoutRow{VertexLayout::PositionNormalUvTangentColour,
                    VertexAttribute::Uv | VertexAttribute::Normal | VertexAttribute::Tangent |
                        VertexAttribute::Colour},
    VertexLayoutRow{VertexLayout::PositionNormalUvUv1TangentColour,
                    VertexAttribute::Uv | VertexAttribute::Uv1 | VertexAttribute::Normal |
                        VertexAttribute::Tangent | VertexAttribute::Colour}};

[[nodiscard]] constexpr bool VertexLayoutsIndexThemselves() {
  for (size_t at = 0; at < kVertexLayouts.size(); ++at) {
    if (static_cast<size_t>(kVertexLayouts[at].Layout) != at) { return false; }
  }
  return true;
}

static_assert(VertexLayoutsIndexThemselves(),
              "a layout's place in the table is the index its pipelines are built at");

[[nodiscard]] constexpr VertexAttribute AttributesOf(VertexLayout layout) {
  return kVertexLayouts[static_cast<size_t>(layout)].Carries;
}

[[nodiscard]] constexpr bool CarriesUv(VertexLayout layout) {
  return Holds(AttributesOf(layout), VertexAttribute::Uv);
}

[[nodiscard]] constexpr bool CarriesUv1(VertexLayout layout) {
  return Holds(AttributesOf(layout), VertexAttribute::Uv1);
}

[[nodiscard]] constexpr bool CarriesNormal(VertexLayout layout) {
  return Holds(AttributesOf(layout), VertexAttribute::Normal);
}

[[nodiscard]] constexpr bool CarriesTangent(VertexLayout layout) {
  return Holds(AttributesOf(layout), VertexAttribute::Tangent);
}

[[nodiscard]] constexpr bool CarriesColour(VertexLayout layout) {
  return Holds(AttributesOf(layout), VertexAttribute::Colour);
}

struct VertexRun {
  uint32_t Floats = 0;
  uint32_t Location = 0;
};

inline constexpr uint32_t kMostVertexRuns = 7;

[[nodiscard]] constexpr uint32_t RunsOf(VertexLayout layout, bool writesVelocity, VertexRun *out) {
  uint32_t n = 0;
  out[n++] = VertexRun{3, 0};
  if (CarriesUv(layout)) { out[n++] = VertexRun{2, 1}; }
  if (CarriesUv1(layout)) { out[n++] = VertexRun{2, 6}; }
  out[n++] = VertexRun{3, CarriesNormal(layout) ? 3u : 2u};
  if (CarriesTangent(layout)) { out[n++] = VertexRun{4, 4}; }
  if (CarriesColour(layout)) { out[n++] = VertexRun{4, 7}; }
  if (writesVelocity) { out[n++] = VertexRun{3, 5}; }
  return n;
}

[[nodiscard]] constexpr uint32_t RichestRunCount() {
  uint32_t most = 0;
  for (const VertexLayoutRow &row : kVertexLayouts) {
    VertexRun runs[16] = {};
    const uint32_t n = RunsOf(row.Layout, true, runs);
    most = n > most ? n : most;
  }
  return most;
}

static_assert(RichestRunCount() == kMostVertexRuns,
              "the richest vertex layout fills a different number of runs than the pipeline "
              "declares room for, so one of the two was changed without the other");

struct VertexRunsCarried {
  bool Uv = false;
  bool Uv1 = false;
  bool Normal = false;
  bool Tangent = false;
  bool Colour = false;
};

[[nodiscard]] constexpr bool LayoutOf(const VertexRunsCarried &carried, VertexLayout &out) {
  VertexAttribute wanted = VertexAttribute::None;
  if (carried.Uv) { wanted = wanted | VertexAttribute::Uv; }
  if (carried.Uv1) { wanted = wanted | VertexAttribute::Uv1; }
  if (carried.Normal) { wanted = wanted | VertexAttribute::Normal; }
  if (carried.Tangent) { wanted = wanted | VertexAttribute::Tangent; }
  if (carried.Colour) { wanted = wanted | VertexAttribute::Colour; }
  for (const VertexLayoutRow &row : kVertexLayouts) {
    if (row.Carries != wanted) { continue; }
    out = row.Layout;
    return true;
  }
  return false;
}

struct DrawItem {
  DrawOrder Order;
  uint32_t SourceFirstIndex = 0;
  uint32_t IndexCount = 0;
  VertexLayout Layout = VertexLayout::Position;
  uint32_t FirstIndex = 0;

  uint32_t ModelSlot = 0;
  uint32_t Instances = 1;

  uint32_t FirstCluster = 0;
  uint32_t ClusterCount = 0;

  uint32_t Submitted = 0;
};

struct DrawBatch {
  uint32_t FirstIndex = 0;
  uint32_t IndexCount = 0;
  uint32_t MaterialSlot = 0;
  VertexLayout Layout = VertexLayout::Position;
  SurfaceKind Kind = SurfaceKind::Opaque;

  uint32_t Draws = 1;

  uint32_t ModelSlot = 0;
  uint32_t Instances = 1;

  uint32_t FirstJob = 0;
  uint32_t JobCount = 0;
};

struct IndexRun {
  uint32_t SourceFirst = 0;
  uint32_t Count = 0;
};

class DrawList {
public:
  [[nodiscard]] bool Add(const DrawItem &item, std::string &error);

  void Clear();

  void Compile();

  void JobsAddress(std::span<const DagCluster> clusters);

  [[nodiscard]] const std::vector<DrawItem> &Draws() const { return Draws_; }

  [[nodiscard]] const std::vector<IndexRun> &Runs() const { return Runs_; }

  [[nodiscard]] const std::vector<DrawBatch> &Batches() const { return Batches_; }

  [[nodiscard]] const std::vector<uint32_t> &ClusterJobs() const { return Jobs_; }

  static constexpr size_t kJobWords = 4;

  [[nodiscard]] uint32_t IndexCount() const { return IndexCount_; }

  [[nodiscard]] bool Empty() const { return Draws_.empty(); }

private:
  std::vector<DrawItem> Draws_;
  std::vector<IndexRun> Runs_;
  std::vector<DrawBatch> Batches_;
  std::vector<uint32_t> Jobs_;
  uint32_t IndexCount_ = 0;
};

} // namespace outshine::Render
#endif
