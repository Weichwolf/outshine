#ifndef OUTSHINE_ENGINE_STREAMING_WEATHERPREPARATION_H
#define OUTSHINE_ENGINE_STREAMING_WEATHERPREPARATION_H

#include "SourceSession.h"
#include "Tasks.h"
#include <world/weather/WeatherSnapshot.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace outshine {
class WeatherPreparation {
public:
  enum class Phase : uint8_t { Fetching, Decoding, Ready, Missing, Failed, Cancelled };

  struct Workers {
    Tasks &Io;
    Tasks &Compute;
  };

  WeatherPreparation(std::shared_ptr<Data::SourceSession> sources,
                     Data::Transport &transport,
                     Workers workers,
                     int64_t utcS);
  ~WeatherPreparation();
  WeatherPreparation(const WeatherPreparation &) = delete;
  WeatherPreparation &operator=(const WeatherPreparation &) = delete;

  [[nodiscard]] Phase Advance();
  void Cancel() noexcept;
  [[nodiscard]] bool AwaitProgress(double seconds) const;

  [[nodiscard]] std::shared_ptr<const WeatherSnapshot> Snapshot() const noexcept {
    return Snapshot_;
  }

  [[nodiscard]] std::string_view Error() const noexcept { return Error_; }

  [[nodiscard]] bool Running() const noexcept { return Handle_ != Tasks::kNoTask; }

  [[nodiscard]] Phase CurrentPhase() const noexcept { return Phase_; }

private:
  struct Work {
    Work(std::shared_ptr<Data::SourceSession> sources, int64_t utcS);
    std::shared_ptr<Data::SourceSession> Sources;
    Data::SourceSet::Query Query;
    int64_t UtcS;
    std::atomic_bool Stopping = false;
    Data::Delivery Delivery = Data::Delivery::Waiting();
    std::optional<Data::Delivery::Answer> Answer;
    std::shared_ptr<const WeatherSnapshot> Snapshot;
    std::string Error;
    Phase Outcome = Phase::Fetching;
    bool Abandoned = false;
  };

  void Decode();
  void Abandon();
  void Publish();
  Data::Transport &Transport_;
  Tasks &Io_;
  Tasks &Compute_;
  Tasks *ActivePool_ = nullptr;
  Tasks::Handle Handle_ = Tasks::kNoTask;
  std::unique_ptr<Work> Work_;
  Phase Phase_ = Phase::Fetching;
  std::shared_ptr<const WeatherSnapshot> Snapshot_;
  std::string Error_;
};
}
#endif
