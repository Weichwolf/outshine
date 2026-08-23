#ifndef OUTSHINE_GROUND_CLASSBUILDER_H
#define OUTSHINE_GROUND_CLASSBUILDER_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "ClassStructure.h"
#include "TangentFrame.h"

namespace outshine::Ground {

enum class ClassGrain { Fine, Coarse };

class ClassBuilder {
public:
  struct Ring {
    uint32_t First = 0, Count = 0;
  };

  enum class Shape : uint8_t { Point = 1, Line = 2, Polygon = 3 };

  struct Feature {
    uint32_t FirstRing = 0, RingCount = 0;
    int Rank = 0;
    uint16_t Tpl = 0;
    Shape Form = Shape::Polygon;
    float WidthM = 0.0f;
    float MinE = 0, MinN = 0, MaxE = 0, MaxN = 0;
  };

  struct Job {
    ClassGrain Grain = ClassGrain::Fine;

    TangentFrame Frame;
    double CamE = 0, CamN = 0;
    double CellM = 1;
    int HalfCells = 0;
    int UnmappedRow = 0;
    std::vector<float> Pts;
    std::vector<Ring> Rings;
    std::vector<Feature> Feats;
  };
  struct Handback {
    std::shared_ptr<const ClassStructure> Structure;
    Job Returned;
  };

  ClassBuilder();
  ~ClassBuilder();
  ClassBuilder(const ClassBuilder &) = delete;
  ClassBuilder &operator=(const ClassBuilder &) = delete;

  void Submit(Job job);
  std::optional<Handback> Collect();

  size_t HeapBytes() const { return HeapBytes_.load(std::memory_order_relaxed); }

private:
  struct Hit { double X; int Dir; };

  struct Workspace {
    std::vector<uint8_t> Base, BaseRank;
    std::vector<int32_t> SeedHead, SeedNext;
    std::vector<uint32_t> SeedCount;
    std::vector<float> Edges, Curve;
    std::vector<uint32_t> ByY, Act;
    std::vector<int32_t> CellHead, CellNext;
    std::vector<uint32_t> CellStamp, CellEdge, CellCount;
    std::vector<Hit> Hits;
    std::vector<uint32_t> Seeds;
  };

  void Run();
  void LayDown(const Job &job, ClassStructure::Grid &out, int &overflow);
  size_t ScratchBytes() const;

  enum class Stage { Idle, Building, Done };

  mutable std::mutex Mu_;
  std::condition_variable Cv_;
  std::optional<Job> Pending_;
  std::optional<Handback> Result_;
  Stage Stage_ = Stage::Idle;
  bool Stop_ = false;

  std::shared_ptr<const ClassStructure::Grid> Fine_, Coarse_;
  Workspace Workspace_;
  uint64_t Version_ = 0;
  std::atomic<size_t> HeapBytes_{0};

  std::thread Thread_;
};

}
#endif
