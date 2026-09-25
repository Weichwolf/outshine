#ifndef OUTSHINE_WORLD_GROUND_VECTORSTREETGRAPH_H
#define OUTSHINE_WORLD_GROUND_VECTORSTREETGRAPH_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "GroundStack.h"
#include "Wayfinding.h"

namespace outshine::Ground {

class VectorStreetGraph {
public:
  struct Built {
    std::shared_ptr<const Path::Network> Graph;
    size_t Ways = 0;
    size_t Nodes = 0;
    size_t Edges = 0;
    size_t Junctions = 0;
    Path::Network::Elevated Elevated;
    std::string Refusal;
    double LayMs = 0.0;
    double WeaveMs = 0.0;
    double BeginWeaveMs = 0.0;
    double CleanupWeaveMs = 0.0;
    double CleanupWeaveLongestMs = 0.0;
    double WeaveLongestMs = 0.0;
    Path::NetworkWeaveJob::SliceWorst WeaveSlices;
    Path::Network::WeaveTimings WeavePhases;
    double CrossingsMs = 0.0;
    double CrossingsLongestMs = 0.0;
    Path::Network::Swept CrossingSweep;
    Path::NetworkCrossingJob::SliceWorst CrossingSlices;
    double ElevateMs = 0.0;
    double BeginElevationMs = 0.0;
    double ElevateLongestMs = 0.0;
    Path::NetworkElevationJob::SliceWorst ElevationSlices;
    double PublishMs = 0.0;
  };

  [[nodiscard]] static Built BuildOneShot(const Ground::GroundStack &stack);

private:
  friend class VectorStreetGraphBuildJob;
  static constexpr double kNodeSnapM = 2.0;
  [[nodiscard]] static std::expected<void, std::string_view>
  LayWays(const Ground::StreetField &ways, std::span<const double> points, Path::Network &graph);
};

class VectorStreetGraphBuildJob {
public:
  [[nodiscard]] static std::expected<VectorStreetGraphBuildJob, std::string>
  Begin(const Ground::GroundStack &stack, Path::Network::HeightSource heightOf);
  VectorStreetGraphBuildJob(const VectorStreetGraphBuildJob &) = delete;
  VectorStreetGraphBuildJob &operator=(const VectorStreetGraphBuildJob &) = delete;
  VectorStreetGraphBuildJob(VectorStreetGraphBuildJob &&) noexcept = default;
  VectorStreetGraphBuildJob &operator=(VectorStreetGraphBuildJob &&) noexcept = default;

  [[nodiscard]] std::expected<bool, std::string> Advance(size_t itemsMost);
  [[nodiscard]] std::expected<VectorStreetGraph::Built, std::string_view> Take() &&;

  [[nodiscard]] double LongestSliceMs() const noexcept { return LongestSliceMs_; }

  [[nodiscard]] const char *PhaseName() const noexcept;

private:
  enum class Stage : uint8_t {
    BeginWeave,
    Weave,
    CleanupWeave,
    BeginCrossings,
    Crossings,
    BeginElevation,
    Elevation,
    Publish,
    Done
  };
  VectorStreetGraphBuildJob(Path::Network &&graph, Path::Network::HeightSource heightOf);
  [[nodiscard]] std::expected<void, std::string> BeginWeave();
  [[nodiscard]] std::expected<void, std::string> AdvanceWeave(size_t itemsMost);
  [[nodiscard]] std::expected<void, std::string> CleanupWeave(size_t itemsMost);
  [[nodiscard]] std::expected<void, std::string> BeginCrossings();
  [[nodiscard]] std::expected<void, std::string> AdvanceCrossings(size_t pairsMost);
  void BeginElevation();
  [[nodiscard]] std::expected<void, std::string> AdvanceElevation(size_t itemsMost);
  void Publish();

  Path::Network Graph_;
  std::unique_ptr<Path::NetworkWeaveJob> Weave_;
  std::unique_ptr<Path::NetworkCrossingJob> Crossings_;
  std::unique_ptr<Path::NetworkElevationJob> Elevation_;
  Path::Network::HeightSource HeightOf_;
  VectorStreetGraph::Built Built_;
  double LongestSliceMs_ = 0.0;
  Stage Stage_ = Stage::BeginWeave;
};

}

#endif
