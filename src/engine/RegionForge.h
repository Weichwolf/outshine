#ifndef OUTSHINE_ENGINE_REGIONFORGE_H
#define OUTSHINE_ENGINE_REGIONFORGE_H

#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "GeneratorSet.h"
#include "Ground.h"
#include "Region.h"
#include "RegionPool.h"
#include "Yield.h"

namespace outshine::Clients {

class RegionForge {
public:

  struct Grown {
    Generators::Ground Where;
    Generators::RegionPool::Lease Space;
    std::vector<Generators::Yield::Note> Notes;
    std::vector<Generators::Yield> Yields;
    double OccupyMs = 0.0;
  };

  explicit RegionForge(const Generators::GeneratorSet &generators);
  ~RegionForge();
  RegionForge(const RegionForge &) = delete;
  RegionForge &operator=(const RegionForge &) = delete;

  void Request(const Generators::Ground &ground, Generators::RegionPool::Lease space);

  std::optional<Generators::Region> UnderWay() const;
  [[nodiscard]] bool Idle() const;

  std::optional<Grown> Collect();

  void Cancel();

private:
  void Run();

  enum class Stage { Idle, Growing, Done };

  struct Order {
    Generators::Ground Where;
    Generators::RegionPool::Lease Space;
  };

  const Generators::GeneratorSet &Gens_;

  mutable std::mutex Mu_;
  std::condition_variable Cv_;
  std::optional<Order> Pending_;
  std::optional<Grown> Result_;
  std::optional<Generators::Region> Where_;
  Stage Stage_ = Stage::Idle;
  bool Dropped_ = false;
  bool Stop_ = false;

  std::thread Thread_;
};

}
#endif
