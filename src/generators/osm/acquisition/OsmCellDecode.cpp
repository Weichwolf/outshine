#include "OsmSourceAcquisition.h"
#include <world/data/Address.h>
#include <expected>
#include <memory>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine::Generators::Osm {

std::expected<SourceAcquisition::CellSource, std::string>
SourceAcquisition::DecodeCell(const SourceChunk &input,
                              const std::stop_token &stop,
                              const std::shared_ptr<const CellCompiler> &compiler) {
  auto loaded = ChunkSetLoader::ParseCell(input, stop);
  if (!loaded) { return std::unexpected(std::move(loaded.error())); }
  CellSource cell{.Snapshot = std::make_shared<const SourceSnapshot>(std::move(*loaded))};
  if (!cell.Snapshot->Cell) { return std::unexpected("decoded original cell has no address"); }
  cell.Address = *cell.Snapshot->Cell;
  cell.ChargedBytes = cell.Snapshot->StorageChargeBytes();
  if (!compiler) { return cell; }
  auto product = compiler->Compile(cell.Snapshot, stop);
  if (!product || !*product) {
    return std::unexpected(
        "original OSM cell " + Data::Address::AtGeoCell(cell.Address).Text() + " compilation: " +
        (product ? "cell compiler returned no product" : std::move(product.error())));
  }
  cell.Product = std::move(*product);
  cell.ChargedBytes = cell.Product->StorageChargeBytes();
  cell.Snapshot.reset();
  return cell;
}

}
