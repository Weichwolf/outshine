#ifndef OUTSHINE_GENERATORS_WEATHER_INPUTS_WEATHERSNAPSHOTLOADER_H
#define OUTSHINE_GENERATORS_WEATHER_INPUTS_WEATHERSNAPSHOTLOADER_H

#include "Tasks.h"
#include <WeatherSnapshot.h>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace outshine::Data {
class SourceSession;
class SourceTransport;
}

namespace outshine::Generators::Weather {
class WeatherSnapshotLoader {
public:
  enum class Phase : uint8_t { Acquiring, Decoding, Ready, Missing, Failed, Cancelled };

  struct Workers {
    Tasks &Io;
    Tasks &Compute;
  };

  // Pools and transport outlive unfinished jobs; destruction never waits for them.
  WeatherSnapshotLoader(std::shared_ptr<Data::SourceSession> sources,
                        Data::SourceTransport &transport,
                        Workers workers,
                        int64_t utcS);
  ~WeatherSnapshotLoader();
  WeatherSnapshotLoader(const WeatherSnapshotLoader &) = delete;
  WeatherSnapshotLoader &operator=(const WeatherSnapshotLoader &) = delete;

  [[nodiscard]] Phase Advance();
  void Cancel() noexcept;
  [[nodiscard]] bool AwaitProgress(double seconds) const;

  [[nodiscard]] std::shared_ptr<const WeatherSnapshot> Snapshot() const noexcept {
    return Snapshot_;
  }

  [[nodiscard]] std::string_view Error() const noexcept { return Error_; }

  [[nodiscard]] bool Running() const noexcept;

  [[nodiscard]] Phase CurrentPhase() const noexcept { return Phase_; }

private:
  struct Work;

  void Schedule(Tasks &pool, void (Work::*execute)());
  void Publish();
  Tasks &Io_;
  Tasks &Compute_;
  std::shared_ptr<Work> Work_;
  Phase Phase_ = Phase::Acquiring;
  std::shared_ptr<const WeatherSnapshot> Snapshot_;
  std::string Error_;
};
}
#endif
