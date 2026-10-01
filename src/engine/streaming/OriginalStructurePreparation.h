#ifndef OUTSHINE_ENGINE_STREAMING_ORIGINALSTRUCTUREPREPARATION_H
#define OUTSHINE_ENGINE_STREAMING_ORIGINALSTRUCTUREPREPARATION_H

#include "OriginalStructureInput.h"
#include "Tasks.h"

#include <expected>
#include <cstdint>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {

class OriginalStructurePreparation {
public:
  enum class Phase : uint8_t { Working, Ready, Failed };

  OriginalStructurePreparation(Tasks &pool,
                               std::shared_ptr<const Data::OsmSourceSnapshot> source,
                               Generators::OriginalStructurePolicy policy,
                               int heightZoom);
  ~OriginalStructurePreparation();
  OriginalStructurePreparation(const OriginalStructurePreparation &) = delete;
  OriginalStructurePreparation &operator=(const OriginalStructurePreparation &) = delete;

  [[nodiscard]] Phase Poll();

  void Cancel() noexcept { (void)Stop_.request_stop(); }

  [[nodiscard]] bool Running() const noexcept { return Handle_ != Tasks::kNoTask; }

  [[nodiscard]] bool AwaitSlice(double seconds) const;

  [[nodiscard]] const std::shared_ptr<const Generators::RawTile> &Input() const noexcept {
    return Input_;
  }

  [[nodiscard]] std::string_view Error() const noexcept { return Error_; }

  [[nodiscard]] std::span<const Data::TileId> HeightTiles() const noexcept { return HeightTiles_; }

private:
  struct Output {
    std::expected<Generators::RawTile, std::string> Value =
        std::unexpected("original structure preparation has not completed");
    std::vector<Data::TileId> HeightTiles;
  };

  Tasks *Pool_;
  Tasks::Handle Handle_ = Tasks::kNoTask;
  std::stop_source Stop_;
  std::shared_ptr<Output> Output_;
  std::shared_ptr<const Generators::RawTile> Input_;
  std::vector<Data::TileId> HeightTiles_;
  std::string Error_;
  Phase Phase_ = Phase::Working;
};

}
#endif
