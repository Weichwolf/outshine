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
