#ifndef OUTSHINE_ENGINE_STREAMING_WATERGEOMETRYPREPARATION_H
#define OUTSHINE_ENGINE_STREAMING_WATERGEOMETRYPREPARATION_H

#include "WaterAsset.h"
#include "TangentFrame.h"
#include "Tasks.h"
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

namespace outshine {
class WaterGeometryPreparation {
public:
  struct Completed {
    Generators::WaterAsset Water;
    double WorkerMs = 0;
  };

  WaterGeometryPreparation(Tasks &pool, Generators::WaterAsset source, TangentFrame frame);
  ~WaterGeometryPreparation();
  WaterGeometryPreparation(const WaterGeometryPreparation &) = delete;
  WaterGeometryPreparation &operator=(const WaterGeometryPreparation &) = delete;
  [[nodiscard]] std::optional<std::expected<Completed, std::string>> Collect();

  void Cancel() noexcept { Stop_.request_stop(); }

  [[nodiscard]] bool Complete() const noexcept;

  [[nodiscard]] bool Matches(const Generators::WaterAsset &source) const noexcept {
    return Source_.SharesInputsWith(source);
  }

private:
  struct Work;
  Generators::WaterAsset Source_;
  std::shared_ptr<Work> Work_;
  std::stop_source Stop_;
};
}
#endif
