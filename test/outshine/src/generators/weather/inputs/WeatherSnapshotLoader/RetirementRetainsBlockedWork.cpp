#include "Check.h"
#include "SourceSession.h"
#include "WeatherCapture.h"
#include "WeatherSnapshotLoader.h"
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <memory>
#include <semaphore>
#include <thread>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Test;
using outshine::Generators::Weather::WeatherSnapshotLoader;
constexpr auto kDeadline = std::chrono::seconds(3);

struct Gate {
  bool Decode = false;
  std::binary_semaphore Entered{0}, Release{0}, Gone{0};
  std::atomic_uint Polls{0}, Decodes{0}, Destroyed{0};
  std::thread::id Caller = std::this_thread::get_id();
  std::atomic_bool ReleasedOffCaller{false};
};

class HeldSource final : public Source {
public:
  HeldSource(std::unique_ptr<Source> source, Gate &gate)
      : Source_(std::move(source)), Gate_(gate) {}

  ~HeldSource() override {
    Gate_.ReleasedOffCaller = std::this_thread::get_id() != Gate_.Caller;
    ++Gate_.Destroyed;
    Gate_.Gone.release();
  }

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override {
    return Source_->Declaration();
  }

  [[nodiscard]] Coverage Covers(const SourceRequest &request) const noexcept override {
    return Source_->Covers(request);
  }

  [[nodiscard]] Address Serves(const SourceRequest &request) const noexcept override {
    return Source_->Serves(request);
  }

  FetchStart Begin(const Address &at, SourceTransport &wire) const override {
    return Source_->Begin(at, wire);
  }

  SourceResponse Collect(const Address &at, Ticket ticket, SourceTransport &wire) const override {
    ++Gate_.Polls;
    if (!Gate_.Decode) {
      Gate_.Entered.release();
      Gate_.Release.acquire();
      return SourceResponse::Working();
    }
    return Source_->Collect(at, ticket, wire);
  }

  void Cancel(Ticket ticket, SourceTransport &wire) const override {
    Source_->Cancel(ticket, wire);
  }

  [[nodiscard]] std::expected<std::optional<WeatherSnapshot>, DecodeFailure>
  DecodeWeather(std::span<const uint8_t> bytes, int64_t utcS) const override {
    ++Gate_.Decodes;
    Gate_.Entered.release();
    Gate_.Release.acquire();
    return Source_->DecodeWeather(bytes, utcS);
  }

private:
  std::unique_ptr<Source> Source_;
  Gate &Gate_;
};

class HeldProvider final : public Provider {
public:
  HeldProvider(const WeatherCapture &capture, Gate &gate)
      : Provider_(capture.Request), Gate_(gate) {}

  [[nodiscard]] std::string_view kind() const override { return "open-meteo"; }

  [[nodiscard]] std::expected<std::unique_ptr<Source>, std::string>
  make(const SourceProvider &declaration, std::string_view root) const override {
    auto source = Provider_.make(declaration, root);
    if (!source) { return std::unexpected(source.error()); }
    return std::make_unique<HeldSource>(std::move(*source), Gate_);
  }

private:
  Generators::Weather::OpenMeteoProvider Provider_;
  Gate &Gate_;
};

std::shared_ptr<SourceSession>
Open(const WeatherCapture &capture, Gate &gate, const std::string &directory) {
  const HeldProvider provider(capture, gate);
  ProviderRegistry registry;
  CHECK(registry.registerProvider(provider), "the held weather provider registers");
  const std::array declarations{capture.Declaration};
  auto session = SourceSession::Open({.Directory = directory}, declarations, {}, registry);
  CHECK(session, "the held weather session opens");
  return session ? *session : nullptr;
}

void Barrier(Tasks &pool) {
  const auto barrier = pool.Post([] {});
  CHECK(barrier != Tasks::kNoTask, "the worker accepts a terminal barrier");
  if (barrier != Tasks::kNoTask) { pool.Wait(barrier); }
}

void Blocked(const WeatherCapture &capture, const std::string &directory, bool decode) {
  Gate gate;
  gate.Decode = decode;
  CapturedWeatherTransport transport(capture);
  Tasks io(1);
  Tasks compute(1);
  auto session = Open(capture, gate, directory);
  if (!session) { return; }
  const std::weak_ptr<SourceSession> lifetime = session;
  auto loader = std::make_unique<WeatherSnapshotLoader>(
      session,
      transport,
      WeatherSnapshotLoader::Workers{.Io = io, .Compute = compute},
      kCapturedWeatherUtcS);
  session.reset();
  bool entered = false;
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (!entered && std::chrono::steady_clock::now() < until) {
    (void)loader->Advance();
    entered = gate.Entered.try_acquire_for(std::chrono::milliseconds(1));
  }
  CHECK(entered, "weather work reaches the held provider or decoder");
  CHECK(!loader->AwaitProgress(0.002), "a held job cannot report completion from another pool");
  std::binary_semaphore retired(0);
  std::thread release([&] {
    loader.reset();
    retired.release();
  });
  CHECK(retired.try_acquire_for(std::chrono::milliseconds(250)),
        "loader destruction returns before the blocked work is released");
  CHECK(!lifetime.expired() && gate.Destroyed == 0,
        "unfinished weather work retains its source after loader retirement");
  gate.Release.release();
  release.join();
  Barrier(compute);
  Barrier(io);
  CHECK(lifetime.expired() && gate.Destroyed == 1 && gate.ReleasedOffCaller,
        "completed work releases its source exactly once on a worker");
  CHECK(decode ? gate.Decodes == 1 : gate.Decodes == 0,
        "retired acquisition never starts a decoder; an active decoder finishes once");
}

void IdleAndDiscard(const WeatherCapture &capture, const std::string &directory, bool queued) {
  Gate gate;
  CapturedWeatherTransport transport(capture);
  auto io = std::make_unique<Tasks>(1);
  Tasks compute(1);
  std::binary_semaphore entered(0);
  std::binary_semaphore release(0);
  CHECK(io->PostDetached([&] {
    entered.release();
    release.acquire();
  }),
        "the IO blocker is accepted");
  const bool held = entered.try_acquire_for(kDeadline);
  CHECK(held, "one IO worker is held before the loader schedules work");
  auto session = Open(capture, gate, directory);
  if (!session) {
    release.release();
    return;
  }
  const std::weak_ptr<SourceSession> lifetime = session;
  auto loader = std::make_unique<WeatherSnapshotLoader>(
      session,
      transport,
      WeatherSnapshotLoader::Workers{.Io = *io, .Compute = compute},
      kCapturedWeatherUtcS);
  session.reset();
  if (queued) { (void)loader->Advance(); }
  loader.reset();
  CHECK(!lifetime.expired() && gate.Destroyed == 0,
        "queued cleanup retains idle and queued inputs without caller IO");
  if (queued) {
    std::thread stop([&] { io.reset(); });
    CHECK(gate.Gone.try_acquire_for(kDeadline),
          "shutdown discards queued jobs and their retained inputs before joining the blocker");
    release.release();
    stop.join();
  } else {
    release.release();
    Barrier(*io);
  }
  CHECK(lifetime.expired() && gate.Destroyed == 1 && gate.ReleasedOffCaller && gate.Polls == 0 &&
            gate.Decodes == 0,
        "idle cleanup and queue discard release once without acquisition or decode");
}
}

int main() {
  const auto capture = RecordedWeatherCapture();
  CHECK(capture, "the recorded weather input exists");
  if (!capture) { return Report(); }
  const auto directory = PlantedPath("weather-retirement");
  Blocked(*capture, directory + "/provider", false);
  Blocked(*capture, directory + "/decoder", true);
  IdleAndDiscard(*capture, directory + "/idle", false);
  IdleAndDiscard(*capture, directory + "/discard", true);
  std::filesystem::remove_all(directory);
  return Report();
}
