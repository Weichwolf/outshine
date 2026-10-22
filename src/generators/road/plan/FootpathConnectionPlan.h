#ifndef OUTSHINE_GENERATORS_ROAD_PLAN_FOOTPATHCONNECTIONPLAN_H
#define OUTSHINE_GENERATORS_ROAD_PLAN_FOOTPATHCONNECTIONPLAN_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <vector>

#include "Earth.h"

namespace outshine::Generators {

namespace FootpathConnections {
struct Inputs;
struct Index;
}

enum class FootpathKind : uint8_t { Other, Path, Steps };
enum class FootpathEndRole : uint8_t { Unknown, Connectable, Terminal, Fragment };

struct FootpathSource {
  std::span<const EastNorth> Axis;
  double HalfWidthM = 0.0;
  FootpathKind Kind = FootpathKind::Other;
  std::optional<int32_t> Layer;
  std::optional<bool> Bridge, Tunnel, FootAccess;
  std::array<FootpathEndRole, 2> Ends{FootpathEndRole::Unknown, FootpathEndRole::Unknown};
  std::optional<double> PositionErrorBoundM;
};

struct FootpathConnection {
  uint32_t Branch = 0;
  uint32_t End = 0;
  uint32_t Target = 0;
  uint32_t Segment = 0;
  double TargetFraction = 0.0;
  double TargetStationM = 0.0;
  EastNorth JoinedAt;
  double AdvanceM = 0.0;
  double CrossingSine = 0.0;
  bool PositionBoundsKnown = false;
};

enum class FootpathEndDisposition : uint8_t {
  Proposed,
  UnknownEnd,
  Terminal,
  Fragment,
  UnknownGroundAccess,
  NoTarget,
  AmbiguousTargets,
  CandidateBudgetExceeded
};

struct FootpathEndResult {
  uint32_t Source = 0;
  uint32_t End = 0;
  FootpathEndDisposition Disposition = FootpathEndDisposition::UnknownEnd;
};

struct FootpathConnectionBudget {
  static constexpr size_t kDefaultSegments = 200000;
  static constexpr size_t kDefaultEndpoints = 4096;
  static constexpr size_t kDefaultCandidates = 100000;

  size_t Segments = kDefaultSegments;
  size_t Endpoints = kDefaultEndpoints;
  size_t Candidates = kDefaultCandidates;
};

struct FootpathConnectionRecipe {
  double ReachM = 0.5;
  double MinimumCrossingSine = 0.5;
};

struct FootpathConnectionWork {
  size_t SourceSegments = 0;
  size_t IndexedSegments = 0;
  size_t Queries = 0;
  size_t CandidateTests = 0;
  size_t MaximumCandidates = 0;
  size_t BudgetRefusals = 0;
  size_t IndexHeapBytes = 0;
  size_t IndexBuildHeapBytes = 0;
  size_t ScratchHeapBytes = 0;
};

enum class FootpathConnectionError : uint8_t { InvalidInput, CapacityExceeded, Canceled };

struct FootpathAxisEdit {
  uint32_t Source = 0;
  uint32_t Connection = 0;
};

class FootpathConnectionPlan {
public:
  [[nodiscard]] static std::expected<FootpathConnectionPlan, FootpathConnectionError>
  Prepare(std::span<const FootpathSource> sources,
          FootpathConnectionRecipe recipe = {},
          FootpathConnectionBudget budget = {},
          const std::stop_token &stop = {});

  [[nodiscard]] std::span<const FootpathConnection> Connections() const noexcept {
    return Connections_;
  }

  [[nodiscard]] std::span<const FootpathEndResult> Endpoints() const noexcept { return Endpoints_; }

  [[nodiscard]] const FootpathConnectionWork &Work() const noexcept { return Work_; }

  [[nodiscard]] std::span<const FootpathAxisEdit> EditsOf(uint32_t source) const;

  [[nodiscard]] std::vector<EastNorth> Axis(uint32_t source,
                                            std::span<const EastNorth> original) const;

private:
  [[nodiscard]] std::expected<void, FootpathConnectionError>
  FindConnections(std::span<const FootpathSource> sources,
                  const FootpathConnections::Inputs &inputs,
                  const FootpathConnections::Index *index,
                  FootpathConnectionRecipe recipe,
                  FootpathConnectionBudget budget,
                  const std::stop_token &stop);

  [[nodiscard]] std::expected<void, FootpathConnectionError>
  FindEndpoints(uint32_t source,
                std::span<const FootpathSource> sources,
                const FootpathConnections::Inputs &inputs,
                const FootpathConnections::Index *index,
                FootpathConnectionRecipe recipe,
                FootpathConnectionBudget budget,
                std::vector<uint32_t> &candidates,
                const std::stop_token &stop);

  std::vector<FootpathConnection> Connections_;
  std::vector<FootpathEndResult> Endpoints_;
  std::vector<FootpathAxisEdit> Edits_;
  FootpathConnectionWork Work_;
};

}

#endif
