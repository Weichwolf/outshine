#include "WeatherPreparation.h"
#include "WeatherCapture.h"
#include "OfflineTransport.h"
#include "Check.h"

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Data;
constexpr double kWorkerPollSeconds = 0.001;

struct Calls {
  std::thread::id Caller = std::this_thread::get_id();
  std::atomic_int Decodes = 0;
  std::atomic_bool DecodeOnCaller = false;
  bool WrongUtc = false;
  std::thread::id DecodeThread;
};

class WatchedSource final : public Source {
public:
  WatchedSource(std::unique_ptr<Source> source, Calls &calls)
      : Source_(std::move(source)), Calls_(calls) {}

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override {
    return Source_->Declaration();
  }

  [[nodiscard]] Coverage Covers(const Fetch &request) const noexcept override {
    return Source_->Covers(request);
  }

  [[nodiscard]] Address Serves(const Fetch &request) const noexcept override {
    return Source_->Serves(request);
  }

  [[nodiscard]] FetchStart Begin(const Address &at, Transport &wire) const override {
    return Source_->Begin(at, wire);
  }

  [[nodiscard]] Fetched Collect(const Address &at, Ticket ticket, Transport &wire) const override {
    return Source_->Collect(at, ticket, wire);
  }

  void Cancel(Ticket ticket, Transport &wire) const override { Source_->Cancel(ticket, wire); }

  [[nodiscard]] std::expected<std::optional<WeatherSnapshot>, DecodeFailure>
  DecodeWeather(std::span<const uint8_t> bytes, int64_t utcS) const override {
    ++Calls_.Decodes;
    Calls_.DecodeThread = std::this_thread::get_id();
    Calls_.DecodeOnCaller.store(std::this_thread::get_id() == Calls_.Caller);
    auto result = Source_->DecodeWeather(bytes, utcS);
    if (result && *result && Calls_.WrongUtc) { ++(**result).Values.ValidUtcS; }
    return result;
  }

private:
  std::unique_ptr<Source> Source_;
  Calls &Calls_;
};

class WatchedProvider final : public Provider {
public:
  WatchedProvider(const WeatherCapture &capture, Calls &calls)
      : Provider_(capture.Request), Calls_(calls) {}

  [[nodiscard]] std::string_view kind() const override { return "open-meteo"; }

  [[nodiscard]] std::expected<std::unique_ptr<Source>, std::string>
  make(const SourceProvider &declaration, std::string_view root) const override {
    auto source = Provider_.make(declaration, root);
    if (!source) { return std::unexpected(source.error()); }
    return std::make_unique<WatchedSource>(std::move(*source), Calls_);
  }

private:
  Generators::Weather::OpenMeteoProvider Provider_;
  Calls &Calls_;
};

struct TransportCalls {
  std::atomic_bool Waiting = false;
  std::atomic_int Begins = 0;
  std::atomic_int Cancels = 0;
  std::atomic_bool IoOnCaller = false;
  std::thread::id IoThread;
};

class WatchedTransport final : public Transport {
public:
  WatchedTransport(const WeatherCapture &capture, TransportCalls &calls)
      : Replay_(capture), Calls_(calls) {}

  [[nodiscard]] FetchStart Begin(const std::string &uri) override {
    CheckThread();
    ++Calls_.Begins;
    return Replay_.Begin(uri);
  }

  [[nodiscard]] Wire Collect(Ticket ticket) override {
    CheckThread();
    return Calls_.Waiting.load() ? Wire::Working() : Replay_.Collect(ticket);
  }

  void Cancel(Ticket ticket) override {
    CheckThread();
    ++Calls_.Cancels;
    Replay_.Cancel(ticket);
  }

private:
  void CheckThread() const noexcept {
    Calls_.IoThread = std::this_thread::get_id();
    if (std::this_thread::get_id() == Caller_) { Calls_.IoOnCaller.store(true); }
  }

  CapturedWeatherTransport Replay_;
  TransportCalls &Calls_;
  std::thread::id Caller_ = std::this_thread::get_id();
};

std::shared_ptr<SourceSession> Session(const WeatherCapture &capture,
                                       const WatchedProvider &provider,
                                       const std::string &directory) {
  ProviderRegistry registry;
  CHECK(registry.registerProvider(provider).has_value(), "the public source factory registers");
  const std::array declarations{capture.Declaration};
  auto sources = SourceSession::Open({.Directory = directory}, declarations, {}, registry);
  CHECK(sources.has_value(), "the source session configures without terrain or renderer setup");
  return sources ? *sources : nullptr;
}

WeatherPreparation::Phase Pump(WeatherPreparation &work, WeatherPreparation::Workers workers) {
  auto phase = work.CurrentPhase();
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while ((phase == WeatherPreparation::Phase::Fetching ||
          phase == WeatherPreparation::Phase::Decoding) &&
         std::chrono::steady_clock::now() < until) {
    phase = work.Advance();
    (void)workers.Io.AwaitCompletion(kWorkerPollSeconds);
    (void)workers.Compute.AwaitCompletion(kWorkerPollSeconds);
  }
  return phase;
}

void RealAndOffline(const WeatherCapture &capture, const std::string &directory) {
  Calls calls;
  const WatchedProvider provider(capture, calls);
  TransportCalls wireCalls;
  WatchedTransport replay(capture, wireCalls);
  Tasks io(1);
  Tasks compute(1);
  const WeatherPreparation::Workers workers{.Io = io, .Compute = compute};
  auto sources = Session(capture, provider, directory);
  if (!sources) { return; }
  const std::weak_ptr<SourceSession> weak = sources;
  std::shared_ptr<const WeatherSnapshot> held;
  {
    WeatherPreparation work(sources, replay, workers, kCapturedWeatherUtcS);
    sources.reset();
    CHECK(!weak.expired(), "the active weather request retains its exact source/store session");
    CHECK(Pump(work, workers) == WeatherPreparation::Phase::Ready,
          "real original bytes produce the exact native UTC on the compute worker");
    held = work.Snapshot();
    CHECK(held && held->Values.ValidUtcS == kCapturedWeatherUtcS && held->Values.CloudLow == 0.0,
          "known-zero low cloud and exact UTC reach the owned publication");
    CHECK(calls.Decodes == 1 && !calls.DecodeOnCaller && !wireCalls.IoOnCaller,
          "the control caller performs neither source IO nor native normalization");
    CHECK(calls.DecodeThread != wireCalls.IoThread,
          "IO and native normalization use the two independently supplied worker pools");
  }
  CHECK(weak.expired() && held && held->Source.PayloadSha256 == capture.Declaration.PayloadSha256,
        "the immutable publication owns its provenance after source/session destruction");
  auto warm = Session(capture, provider, directory);
  if (!warm) { return; }
  OfflineTransport offline;
  WeatherPreparation cached(warm, offline, workers, kCapturedWeatherUtcS);
  CHECK(Pump(cached, workers) == WeatherPreparation::Phase::Ready,
        "another source session replays the original bytes offline through existing cache");
  CHECK(warm->Sources().Counters().ProviderStarts == 0 && warm->Sources().Counters().FromStore == 1,
        "offline replay starts no provider or transport work");
  WeatherPreparation missing(warm, offline, workers, kCapturedWeatherUtcS + 1);
  CHECK(Pump(missing, workers) == WeatherPreparation::Phase::Missing && !missing.Snapshot(),
        "a non-hour UTC stays missing instead of rounding to a forecast hour");
  calls.WrongUtc = true;
  WeatherPreparation wrong(warm, offline, workers, kCapturedWeatherUtcS);
  CHECK(Pump(wrong, workers) == WeatherPreparation::Phase::Failed && !wrong.Snapshot(),
        "a source violating exact UTC is rejected before publication");
}

void PendingCancellation(const WeatherCapture &capture, const std::string &directory) {
  Calls calls;
  const WatchedProvider provider(capture, calls);
  TransportCalls wireCalls;
  WatchedTransport replay(capture, wireCalls);
  wireCalls.Waiting.store(true);
  Tasks io(1);
  Tasks compute(1);
  const WeatherPreparation::Workers workers{.Io = io, .Compute = compute};
  auto sources = Session(capture, provider, directory);
  if (!sources) { return; }
  WeatherPreparation work(sources, replay, workers, kCapturedWeatherUtcS);
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (wireCalls.Begins == 0 && std::chrono::steady_clock::now() < until) {
    CHECK(work.Advance() == WeatherPreparation::Phase::Fetching,
          "pending remote work returns control without a decode or wait");
    (void)workers.Io.AwaitCompletion(kWorkerPollSeconds);
  }
  CHECK(wireCalls.Begins == 1 && calls.Decodes == 0,
        "one remote query remains pending without decode");
  work.Cancel();
  CHECK(wireCalls.Cancels == 0 && !work.Snapshot(),
        "cancel only signals; it performs no caller IO");
  while ((wireCalls.Cancels == 0 || work.Running()) && std::chrono::steady_clock::now() < until) {
    (void)work.Advance();
    (void)workers.Io.AwaitCompletion(kWorkerPollSeconds);
  }
  CHECK(wireCalls.Cancels == 1 && !wireCalls.IoOnCaller && calls.Decodes == 0,
        "the IO worker cancels its owned query exactly once without a late publication");
}
}

int main() {
  const auto capture = RecordedWeatherCapture();
  CHECK(capture.has_value(), "the unchanged recorded forecast fixture is present");
  if (!capture) { return Report(); }
  const auto directory = PlantedPath("weather-preparation");
  RealAndOffline(*capture, directory + "/ready");
  PendingCancellation(*capture, directory + "/cancel");
  std::filesystem::remove_all(directory);
  return Report();
}
