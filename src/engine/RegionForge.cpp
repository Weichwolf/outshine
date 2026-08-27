#include "RegionForge.h"

#include <cassert>
#include <chrono>

#include "StackProbe.h"

namespace outshine::Core {
namespace {

double MonotonicMs() {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

}

RegionForge::RegionForge(const Generators::GeneratorSet &generators)
    : Gens_(generators), Thread_([this] { Run(); }) {}

RegionForge::~RegionForge() {
  {
    std::lock_guard<std::mutex> lk(Mu_);
    Stop_ = true;
  }
  Cv_.notify_all();
  Thread_.join();
}

void RegionForge::Request(const Generators::Ground &ground, Generators::RegionPool::Lease space) {
  {
    std::lock_guard<std::mutex> lk(Mu_);
    assert(Stage_ == Stage::Idle);
    Where_ = ground.Where();
    Dropped_ = false;
    Pending_.emplace(Order{ground, std::move(space)});
    Stage_ = Stage::Growing;
  }
  Cv_.notify_one();
}

std::optional<Generators::Region> RegionForge::UnderWay() const {
  std::lock_guard<std::mutex> lk(Mu_);
  return Where_;
}

bool RegionForge::Idle() const {
  std::lock_guard<std::mutex> lk(Mu_);
  return Stage_ == Stage::Idle;
}

std::optional<RegionForge::Grown> RegionForge::Collect() {
  std::lock_guard<std::mutex> lk(Mu_);
  if (Stage_ != Stage::Done) return {};
  std::optional<Grown> out;

  if (!Dropped_) out = std::move(Result_);
  Result_.reset();
  Where_.reset();
  Dropped_ = false;
  Stage_ = Stage::Idle;
  return out;
}

void RegionForge::Cancel() {
  std::lock_guard<std::mutex> lk(Mu_);
  Dropped_ = true;
}

void RegionForge::Run() {
  StackProbe::Enter(StackProbe::Purpose::Region);
  for (;;) {
    std::optional<Order> order;
    {
      std::unique_lock<std::mutex> lk(Mu_);
      Cv_.wait(lk, [this] { return Stop_ || Pending_.has_value(); });
      if (Stop_) return;
      order = std::move(Pending_);
      Pending_.reset();
    }
    Grown grown{order->Where, std::move(order->Space), {}, {}, 0.0};
    size_t notes = 0;
    for (size_t g = 0; g < Gens_.Count(); g++) notes += Gens_.At(g).NoteNames().Size();
    grown.Notes.resize(notes);
    grown.Yields.reserve(Gens_.Count());
    for (size_t g = 0, at = 0; g < Gens_.Count(); g++) {
      const Span<const char *const> names = Gens_.At(g).NoteNames();
      grown.Yields.emplace_back(grown.Space.Sink(), names,
                                Span<Generators::Yield::Note>(grown.Notes.data() + at,
                                                              names.Size()));
      at += names.Size();
    }
    const double t0 = MonotonicMs();
    Gens_.Occupy(grown.Where, Span<Generators::Yield>(grown.Yields.data(), grown.Yields.size()));
    grown.OccupyMs = MonotonicMs() - t0;

    StackProbe::Mark();
    {
      std::lock_guard<std::mutex> lk(Mu_);
      Result_ = std::move(grown);
      Stage_ = Stage::Done;
    }
  }
}

}
