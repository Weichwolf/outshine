#ifndef OUTSHINE_GENERATORS_OSM_BUILDINGS_OSMSTRUCTUREPREPARATION_H
#define OUTSHINE_GENERATORS_OSM_BUILDINGS_OSMSTRUCTUREPREPARATION_H

#include "OsmStructureDescription.h"
#include "OsmStructureCell.h"
#include "StructureBake.h"
#include "Tasks.h"
#include <Address.h>

#include <expected>
#include <cstdint>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>
#include <variant>

namespace outshine::Generators::Osm {

class StructurePreparation {
public:
  enum class Phase : uint8_t { Working, Ready, Failed };

  struct Product {
    std::shared_ptr<const RawTile> Input;
    std::vector<Data::TileId> HeightTiles;
  };

  using SourceInputs = std::vector<std::shared_ptr<const SourceSnapshot>>;
  using CellInputs = std::vector<std::shared_ptr<const StructureCell>>;
  using Inputs = std::variant<SourceInputs, CellInputs>;

  StructurePreparation(Tasks &pool,
                       Inputs inputs,
                       StructurePolicy policy,
                       int heightZoom,
                       std::shared_ptr<const void> lifetime = {});
  // The pool outlives unfinished jobs; destruction never waits for them.
  ~StructurePreparation();
  StructurePreparation(const StructurePreparation &) = delete;
  StructurePreparation &operator=(const StructurePreparation &) = delete;

  [[nodiscard]] Phase Poll();

  void Cancel() noexcept { (void)Stop_.request_stop(); }

  [[nodiscard]] bool Running() const noexcept { return Phase_ == Phase::Working; }

  [[nodiscard]] bool AwaitSlice(double seconds) const;

  [[nodiscard]] std::span<const Product> Products() const noexcept { return Products_; }

  [[nodiscard]] std::string_view Error() const noexcept { return Error_; }

  [[nodiscard]] std::span<const Data::TileId> HeightTiles() const noexcept { return HeightTiles_; }

private:
  struct Output;

  std::shared_ptr<const void> Lifetime_;
  Tasks *Pool_;
  std::stop_source Stop_;
  std::shared_ptr<Output> Output_;
  std::vector<Product> Products_;
  std::vector<Data::TileId> HeightTiles_;
  std::string Error_;
  Phase Phase_ = Phase::Working;
};

}
#endif
