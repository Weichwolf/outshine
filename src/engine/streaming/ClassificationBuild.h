#ifndef OUTSHINE_ENGINE_STREAMING_CLASSIFICATIONBUILD_H
#define OUTSHINE_ENGINE_STREAMING_CLASSIFICATIONBUILD_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include "ClassificationRasterizer.h"
#include "GroundClassBuffer.h"
#include "Tasks.h"
#include "TangentFrame.h"

namespace outshine::Ground {
enum class ClassGrain { Fine, Coarse };

class ClassificationBuild {
public:
  struct SourceRevision {
    uint64_t Frame = 0;
    uint64_t Fine = 0;
    uint64_t Coarse = 0;
    bool operator==(const SourceRevision &) const = default;
  };

  struct Job {
    ClassGrain Grain = ClassGrain::Fine;
    TangentFrame Frame;
    SourceRevision Source;
    int UnmappedRow = 0;
    Generators::ClassificationRasterizer::Input Raster;
  };

  struct Handback {
    std::shared_ptr<const ClassStructure> Structure;
    std::shared_ptr<const Render::GroundClassBuffer> Upload;
    Job Returned;
    double BuildMs = 0;
  };

  explicit ClassificationBuild(Tasks &pool);
  ~ClassificationBuild();
  ClassificationBuild(const ClassificationBuild &) = delete;
  ClassificationBuild &operator=(const ClassificationBuild &) = delete;

  [[nodiscard]] bool Submit(Job job);
  [[nodiscard]] std::optional<Handback> Collect();
  [[nodiscard]] bool AwaitCompletion(double seconds);
  [[nodiscard]] size_t HeapBytes() const;
  void Cancel() noexcept;

private:
  struct Work;
  static void Run(Work &work, const std::stop_token &stop);
  Tasks &Pool_;
  std::shared_ptr<Work> Work_;
  std::stop_source Stop_;
};
}
#endif
