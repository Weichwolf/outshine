/* The class grid, laid down on one thread. doc/todo.md §1 carries the shape and why it is not
 * append-only. */
#ifndef CLASSBUILDER_H
#define CLASSBUILDER_H

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "ClassStructure.h"

namespace outshine::World {

class ClassBuilder {
public:
  enum class Resolution { Fine, Coarse };

  struct Ring {
    uint32_t First = 0, Count = 0;   /* into Job::Pts as ENU metre pairs, ring not closed */
  };
  /* The vector source's own geometry codes, so a decoded feature keeps its type across the handover
   * without a translation table. */
  enum class Shape : uint8_t { Point = 1, Line = 2, Polygon = 3 };

  struct Feature {
    uint32_t FirstRing = 0, RingCount = 0;
    int Rank = 0;
    uint16_t Tpl = 0;
    Shape Form = Shape::Polygon;
    float WidthM = 0.0f;
    float MinE = 0, MinN = 0, MaxE = 0, MaxN = 0;
  };
  /* Nothing here points outside the job: the arrays arrive by move and leave by move, so no address
   * into them survives on the calling side. */
  struct Job {
    Resolution Grain = Resolution::Fine;
    double CamE = 0, CamN = 0;
    double CellM = 1;
    int HalfCells = 0;
    int DefaultTemplate = 0;
    std::vector<float> Pts;
    std::vector<Ring> Rings;
    std::vector<Feature> Feats;      /* ascending rank; the order the grid is laid down in */
  };
  struct Handback {
    std::shared_ptr<const ClassStructure> Structure;
    Job Done;
  };

  ClassBuilder();
  ~ClassBuilder();
  ClassBuilder(const ClassBuilder &) = delete;
  ClassBuilder &operator=(const ClassBuilder &) = delete;

  /* Precondition: nothing submitted and nothing uncollected. The caller knows this from its own
   * bookkeeping, which is why it is asserted here and not queried (`I.6`). */
  void Submit(Job job);
  std::optional<Handback> Collect();

private:
  struct Hit { double X; int Dir; };
  /* Worker thread only. */
  struct Workspace {
    std::vector<uint8_t> Base, BaseRank;
    std::vector<int32_t> SeedHead, SeedNext;
    std::vector<uint32_t> SeedCount;
    std::vector<float> Edges, Curve;
    std::vector<uint32_t> ByY, Act;
    std::vector<int32_t> CellHead, CellNext;
    std::vector<uint32_t> CellStamp, CellEdge, CellCount;
    std::vector<Hit> Hits;
    std::vector<uint32_t> Seeds;   /* the cell-ordered re-emit, swapped into the grid */
  };

  void Run();
  void LayDown(const Job &job, ClassStructure::Grid &out, int &overflow);

  /* Three states, not a boolean over three: `Pending_` empty means either "nothing asked" or "the
   * worker has taken it", and those are not the same thing (`ES.9`). */
  enum class Stage { Idle, Building, Done };

  mutable std::mutex Mu_;
  std::condition_variable Cv_;
  std::optional<Job> Pending_;
  std::optional<Handback> Result_;
  Stage Stage_ = Stage::Idle;
  bool Stop_ = false;

  /* Owned by the worker thread alone. */
  std::shared_ptr<const ClassStructure::Grid> Fine_, Coarse_;
  Workspace Workspace_;
  uint64_t Version_ = 0;

  std::thread Thread_;
};

} // namespace outshine::World
#endif
