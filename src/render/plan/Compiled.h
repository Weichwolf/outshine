#ifndef OUTSHINE_RENDER_PLAN_COMPILED_H
#define OUTSHINE_RENDER_PLAN_COMPILED_H

#include <array>
#include <string_view>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "RenderCatalogue.h"

namespace outshine::Render {

enum class Transfer { Linear, Filmic };

enum class ScenePrecision { Half, Float };

template <typename Held> struct Spelt {
  std::string_view Said;
  Held Means{};
};

template <typename Held, size_t Count>
[[nodiscard]] constexpr bool SpellsEvery(const std::array<Spelt<Held>, Count> &rows, size_t many) {
  if (rows.size() != many) { return false; }
  for (size_t at = 0; at < rows.size(); ++at) {
    if (rows[at].Said.empty()) { return false; }
    for (size_t other = at + 1; other < rows.size(); ++other) {
      if (rows[at].Said == rows[other].Said) { return false; }
      if (static_cast<int>(rows[at].Means) == static_cast<int>(rows[other].Means)) { return false; }
    }
  }
  return true;
}

inline constexpr std::array<Spelt<Transfer>, 2> kTransfers = {
    {{.Said = "linear", .Means = Transfer::Linear}, {.Said = "filmic", .Means = Transfer::Filmic}}};

inline constexpr std::array<Spelt<ScenePrecision>, 2> kPrecisions = {
    {{.Said = "half", .Means = ScenePrecision::Half},
     {.Said = "float", .Means = ScenePrecision::Float}}};

static_assert(SpellsEvery(kTransfers, 2), "both transfer curves are spelt, once each");
static_assert(SpellsEvery(kPrecisions, 2), "and both precisions");

template <typename Held, size_t Count>
[[nodiscard]] constexpr std::optional<Held> Spells(const std::array<Spelt<Held>, Count> &rows,
                                                   std::string_view said) {
  for (const Spelt<Held> &row : rows) {
    if (row.Said == said) { return row.Means; }
  }
  return std::nullopt;
}

template <typename Held, size_t Count>
[[nodiscard]] inline std::string Spellings(const std::array<Spelt<Held>, Count> &rows) {
  std::string said;
  for (size_t at = 0; at < rows.size(); ++at) {
    if (at > 0) { said += at + 1 == rows.size() ? " and " : ", "; }
    said += rows[at].Said;
  }
  return said;
}

template <typename T> class Declared {
public:
  Declared() = default;

  explicit Declared(T value) : Value_(value), Set_(true) {}

  [[nodiscard]] bool IsSet() const { return Set_; }

  [[nodiscard]] bool operator==(const Declared &other) const {
    return Set_ == other.Set_ && (!Set_ || Value_ == other.Value_);
  }

  [[nodiscard]] T Or(T fallback) const { return Set_ ? Value_ : fallback; }

private:
  T Value_{};
  bool Set_ = false;
};

struct PlanSpec {
  std::vector<Resource> Outputs;
  std::vector<Stage> Content;

  Declared<float> Exposure;
  Declared<Transfer> Display;
  Declared<ScenePrecision> Precision;

  [[nodiscard]] bool operator==(const PlanSpec &other) const {
    return Outputs == other.Outputs && Content == other.Content && Exposure == other.Exposure &&
           Display == other.Display && Precision == other.Precision;
  }
};

class Compiled {
public:
  struct Pass {
    PassKind Kind;
    std::string Name;
    size_t First;
    size_t Count;

    AttachmentSet Targets;

    AttachmentSet Buffers;
    Resource Depth = kNoEdge;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<const Compiled>, std::string>
  Compile(const PlanSpec &spec);
  [[nodiscard]] static std::optional<Stage> StageByName(std::string_view name);
  [[nodiscard]] static std::optional<Resource> ResourceByName(std::string_view name);

private:
  [[nodiscard]] static bool
  CompileInto(const PlanSpec &spec, std::shared_ptr<const Compiled> *out, std::string &error);

public:
  [[nodiscard]] bool Holds(Stage stage) const { return HeldStage_[static_cast<size_t>(stage)]; }

  [[nodiscard]] bool Holds(Resource resource) const {
    return HeldResource_[static_cast<size_t>(resource)];
  }

  [[nodiscard]] static uint32_t Stride(Resource resource) {
    return kResources[static_cast<size_t>(resource)].Stride;
  }

  [[nodiscard]] bool Stored(Resource resource) const {
    return Stored_[static_cast<size_t>(resource)];
  }

  [[nodiscard]] Resource Bound(Resource resource) const {
    return Bound_[static_cast<size_t>(resource)];
  }

  [[nodiscard]] const std::vector<Stage> &Order() const { return Order_; }

  [[nodiscard]] const std::vector<Pass> &Passes() const { return Passes_; }

  [[nodiscard]] int PassCount() const { return static_cast<int>(Passes_.size()); }

  [[nodiscard]] bool Fused(Stage stage) const { return Fused_[static_cast<size_t>(stage)]; }

  [[nodiscard]] int SettleFrames() const { return SettleFrames_; }

  [[nodiscard]] Transfer Display() const { return Display_; }

  [[nodiscard]] float Exposure() const { return Exposure_; }

  [[nodiscard]] TexelFormat Format(Resource resource) const {
    return Format_[static_cast<size_t>(resource)];
  }

  [[nodiscard]] ScenePrecision Precision() const { return Precision_; }

  [[nodiscard]] const std::string &Digest() const { return Digest_; }

  [[nodiscard]] const std::vector<std::string> &Merges() const { return Merges_; }

  [[nodiscard]] const std::vector<std::string> &Aliases() const { return Aliases_; }

private:
  Compiled() = default;

  std::array<bool, kStageCount> HeldStage_ = {{}};
  std::array<bool, kResourceCount> HeldResource_ = {{}};
  std::array<bool, kResourceCount> Stored_ = {{}};
  std::array<bool, kStageCount> Fused_ = {{}};
  std::array<Resource, kResourceCount> Bound_ = {{}};
  std::array<TexelFormat, kResourceCount> Format_ = {{}};
  std::vector<Stage> Order_;
  std::vector<Pass> Passes_;
  std::vector<std::string> Merges_;
  std::vector<std::string> Aliases_;
  std::string Digest_;
  Transfer Display_ = Transfer::Filmic;
  ScenePrecision Precision_ = ScenePrecision::Half;
  float Exposure_ = 1.0f;
  int SettleFrames_ = 1;
};

} // namespace outshine::Render
#endif
