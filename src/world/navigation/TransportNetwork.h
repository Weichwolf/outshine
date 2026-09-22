#ifndef OUTSHINE_WORLD_NAVIGATION_TRANSPORTNETWORK_H
#define OUTSHINE_WORLD_NAVIGATION_TRANSPORTNETWORK_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "GroundStack.h"
#include "Wayfinding.h"

namespace outshine::World {

class TransportNetwork {
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
    double WeaveLongestMs = 0.0;
    Path::NetworkWeaveJob::SliceWorst WeaveSlices;
    Path::Network::WeaveTimings WeavePhases;
    double CrossingsMs = 0.0;
    double ElevateMs = 0.0;
    double ElevateLongestMs = 0.0;
    Path::NetworkElevationJob::SliceWorst ElevationSlices;
  };

  [[nodiscard]] static Built BuildOneShot(const Ground::GroundStack &stack);

private:
  friend class TransportNetworkBuildJob;
  static constexpr double kNodeSnapM = 2.0;
  [[nodiscard]] static std::expected<void, std::string_view>
  LayWays(const Ground::StreetField &ways, std::span<const double> points, Path::Network &graph);
};

class TransportNetworkBuildJob {
public:
  [[nodiscard]] static std::expected<TransportNetworkBuildJob, std::string>
  Begin(const Ground::GroundStack &stack);
  TransportNetworkBuildJob(const TransportNetworkBuildJob &) = delete;
  TransportNetworkBuildJob &operator=(const TransportNetworkBuildJob &) = delete;
  TransportNetworkBuildJob(TransportNetworkBuildJob &&) noexcept = default;
  TransportNetworkBuildJob &operator=(TransportNetworkBuildJob &&) noexcept = default;

  [[nodiscard]] std::expected<bool, std::string> Advance(const Ground::GroundStack &stack,
                                                         size_t itemsMost);
  [[nodiscard]] std::expected<TransportNetwork::Built, std::string_view> Take() &&;

  [[nodiscard]] double LongestSliceMs() const noexcept { return LongestSliceMs_; }

private:
  enum class Stage : uint8_t {
    BeginWeave,
    Weave,
    Crossings,
    BeginElevation,
    Elevation,
    Publish,
    Done
  };
  explicit TransportNetworkBuildJob(Path::Network &&graph);
  [[nodiscard]] std::expected<void, std::string> BeginWeave();
  [[nodiscard]] std::expected<void, std::string> AdvanceWeave(size_t itemsMost);
  [[nodiscard]] std::expected<void, std::string> ClassifyCrossings();
  void BeginElevation(const Ground::GroundStack &stack);
  [[nodiscard]] std::expected<void, std::string> AdvanceElevation(size_t itemsMost);
  void Publish();

  Path::Network Graph_;
  std::unique_ptr<Path::NetworkWeaveJob> Weave_;
  std::unique_ptr<Path::NetworkElevationJob> Elevation_;
  TransportNetwork::Built Built_;
  double LongestSliceMs_ = 0.0;
  Stage Stage_ = Stage::BeginWeave;
};

}

#endif
