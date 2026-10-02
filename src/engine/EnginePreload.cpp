#include "EngineHeld.h"
#include "OsmSourceAcquisition.h"
#include <Outshine.h>
#include <math/Units.h>

#include <chrono>
#include <cmath>
#include <expected>
#include <functional>
#include <ratio>

namespace outshine {
namespace Says {
constexpr auto kInvalidPreloadBudget = "preload requires finite nonnegative seconds";
}

namespace {
constexpr double kMostWaitS = 0.05;
constexpr double kBitsPerByte = 8.0;

void ReportPreload(const Engine &engine,
                   std::chrono::steady_clock::time_point began,
                   const std::function<void(const Loading &)> &tell) {
  if (!tell) { return; }
  Loading said = engine.loading();
  said.ElapsedS = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
  said.Megabits = said.ElapsedS > 0.0 ? said.FetchedMB * kBitsPerByte / said.ElapsedS : 0.0;
  tell(said);
}
}

Result Engine::preload(double patienceS) {
  return preloadWithQuality(patienceS, WorldQuality::Playable, {});
}

Result Engine::preload(double patienceS, const std::function<void(const Loading &)> &tell) {
  return preloadWithQuality(patienceS, WorldQuality::Playable, tell);
}

Result Engine::preload(double patienceS, WorldQuality required) {
  return preloadWithQuality(patienceS, required, {});
}

Result Engine::preloadWithQuality(double patienceS,
                                  WorldQuality required,
                                  const std::function<void(const Loading &)> &tell) {
  [[maybe_unused]] const auto logs = S_->Logs();
  if (const auto permission = S_->MutationPermission(); !permission) { return permission; }
  if (!std::isfinite(patienceS * kMsPerS) || patienceS < 0.0) {
    return std::unexpected(Says::kInvalidPreloadBudget);
  }
  const auto configured =
      S_->World.OsmSource ? S_->World.OsmSource->SetAcquisitionBudget(patienceS) : Result{};
  if (!configured) { return std::unexpected(configured.error()); }
  const auto began = std::chrono::steady_clock::now();
  S_->LastPreloadMs = 0.0;
  S_->PreloadPumpMs = 0.0;
  S_->PreloadFlushMs = 0.0;
  S_->PreloadAwaitMs = 0.0;
  S_->PreloadPumps = 0;
  S_->PreloadFlushes = 0;
  S_->PreloadAwaits = 0;
  S_->PreloadWaited = {};
  const auto elapsedMs = [](std::chrono::steady_clock::time_point started) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
        .count();
  };
  const auto timed = [this, began](Result result) -> Result {
    S_->LastPreloadMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    return result;
  };
  const double bound = patienceS;
  const GroundQuality quality =
      required == WorldQuality::Refined ? GroundQuality::Refined : GroundQuality::Playable;
  for (;;) {
    const auto pumpAt = std::chrono::steady_clock::now();
    const auto pumped = S_->PumpPreload();
    S_->PreloadPumpMs += elapsedMs(pumpAt);
    ++S_->PreloadPumps;
    if (!pumped) { return timed(pumped); }
    ReportPreload(*this, began, tell);
    if (!S_->Session.Declared.Ground.Declared && S_->Readiness(quality).Ready()) {
      return timed(Result{});
    }
    if (S_->CanBeginGroundCandidate() || S_->CanAdvanceGroundCandidate()) {
      const auto flushAt = std::chrono::steady_clock::now();
      const auto finished = S_->FlushPreloadGround(began, bound, quality);
      S_->PreloadFlushMs += elapsedMs(flushAt);
      ++S_->PreloadFlushes;
      if (!finished) { return timed(std::unexpected(finished.error())); }
      if (*finished == State::PreloadFlush::Ready) { return timed(Result{}); }
    }
    if (std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() >= bound) {
      return timed(S_->PreloadTimeout(bound, quality));
    }
    const double leftS =
        bound - std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    const double waitS = leftS < kMostWaitS ? leftS : kMostWaitS;
    const auto awaitAt = std::chrono::steady_clock::now();
    S_->AwaitPreloadProgress(waitS);
    S_->PreloadAwaitMs += elapsedMs(awaitAt);
    ++S_->PreloadAwaits;
  }
}

}
