#ifndef OUTSHINE_ENGINE_STREAMING_STREETGRAPHPREPARATION_H
#define OUTSHINE_ENGINE_STREAMING_STREETGRAPHPREPARATION_H

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

#include "StreetGraphBuilder.h"
#include "Tasks.h"

namespace outshine {

class StreetGraphPreparation {
public:
  struct Completed {
    Ground::StreetGraphBuilder::Built Graph;
    double LongestSliceMs = 0.0;
  };

  StreetGraphPreparation(Tasks &pool, Ground::StreetGraphBuildJob job);
  ~StreetGraphPreparation();
  StreetGraphPreparation(const StreetGraphPreparation &) = delete;
  StreetGraphPreparation &operator=(const StreetGraphPreparation &) = delete;

  void Cancel() noexcept;
  [[nodiscard]] bool Complete() const noexcept;
  [[nodiscard]] const char *PhaseName() const noexcept;
  [[nodiscard]] size_t Advances() const noexcept;
  [[nodiscard]] std::optional<std::expected<Completed, std::string>> Collect();

private:
  struct Work;
  static void Run(Work &work, const std::stop_token &stop);
  std::shared_ptr<Work> Work_;
  std::stop_source Stop_;
};

}

#endif
