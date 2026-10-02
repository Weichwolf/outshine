#ifndef OUTSHINE_ENGINE_STREAMING_ORIGINALSTRUCTUREPREPARATION_H
#define OUTSHINE_ENGINE_STREAMING_ORIGINALSTRUCTUREPREPARATION_H

#include "OsmStructureDescription.h"
#include "OsmStructureCell.h"
#include "StructureBake.h"
#include "Tasks.h"
#include <world/data/Address.h>

#include <expected>
#include <cstdint>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>
#include <variant>

namespace outshine {

class OriginalStructurePreparation {
public:
  enum class Phase : uint8_t { Working, Ready, Failed };

  struct Product {
    std::shared_ptr<const Generators::RawTile> Input;
    std::vector<Data::TileId> HeightTiles;
  };

  using SourceInputs = std::vector<std::shared_ptr<const Data::OsmSourceSnapshot>>;
  using CellInputs = std::vector<std::shared_ptr<const Generators::Osm::StructureCell>>;
  using Inputs = std::variant<SourceInputs, CellInputs>;

  OriginalStructurePreparation(Tasks &pool,
                               Inputs inputs,
                               outshine::Generators::Osm::StructurePolicy policy,
                               int heightZoom);
  ~OriginalStructurePreparation();
  OriginalStructurePreparation(const OriginalStructurePreparation &) = delete;
  OriginalStructurePreparation &operator=(const OriginalStructurePreparation &) = delete;

  [[nodiscard]] Phase Poll();

  void Cancel() noexcept { (void)Stop_.request_stop(); }

  [[nodiscard]] bool Running() const noexcept { return Handle_ != Tasks::kNoTask; }

  [[nodiscard]] bool AwaitSlice(double seconds) const;

  [[nodiscard]] std::span<const Product> Products() const noexcept { return Products_; }

  [[nodiscard]] std::string_view Error() const noexcept { return Error_; }

  [[nodiscard]] std::span<const Data::TileId> HeightTiles() const noexcept { return HeightTiles_; }

private:
  struct Output {
    std::expected<std::vector<Product>, std::string> Value =
        std::unexpected("original structure preparation has not completed");
    std::vector<Data::TileId> HeightTiles;
  };

  Tasks *Pool_;
  Tasks::Handle Handle_ = Tasks::kNoTask;
  std::stop_source Stop_;
  std::shared_ptr<Output> Output_;
  std::vector<Product> Products_;
  std::vector<Data::TileId> HeightTiles_;
  std::string Error_;
  Phase Phase_ = Phase::Working;
};

}
#endif
