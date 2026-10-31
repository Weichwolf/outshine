#include "PreparationFixture.h"
#include "OsmSourceCapture.h"
#include "Check.h"
#include <algorithm>
#include <memory>
#include <stop_token>
#include <vector>

namespace {
using namespace PreparationFixture;

void Products() {
  Tasks pool(1);
  const auto source = Source();
  StructurePreparation original(
      pool, StructurePreparation::SourceInputs{source}, kDefaultStructurePolicy, kHeightZoom);
  Barrier(pool);
  CHECK(original.Poll() == StructurePreparation::Phase::Ready && !original.Running(),
        "the original source completes its own preparation");
  CHECK(original.Products().size() == 1 && !original.HeightTiles().empty(),
        "a building product has concrete terrain demand");
  if (original.Products().size() != 1) { return; }
  const auto raw = original.Products().front().Input;
  CHECK(raw && raw->Structures.size() == 1 && raw->LatLon.size() >= 8 &&
            raw->SourceInputs.Archive.lock() == source,
        "native building input retains geometry and its original archive identity");
  const auto compiler = MakeStructureCellCompiler(kDefaultStructurePolicy);
  const auto cell = compiler->Compile(source, std::stop_token{});
  CHECK(cell && *cell, "the same original source compiles into a bounded native cell");
  if (!cell || !*cell) { return; }
  const auto compiled = std::dynamic_pointer_cast<const StructureCell>(*cell);
  CHECK(compiled, "the compiler provides an actual structure cell");
  if (!compiled) { return; }
  StructurePreparation native(
      pool, StructurePreparation::CellInputs{compiled}, kDefaultStructurePolicy, kHeightZoom);
  Barrier(pool);
  CHECK(native.Poll() == StructurePreparation::Phase::Ready && native.Products().size() == 1,
        "already compiled input reaches the same product stage");
  if (native.Products().size() != 1) { return; }
  const auto nativeRaw = native.Products().front().Input;
  CHECK(nativeRaw && raw && nativeRaw->LatLon == raw->LatLon &&
            nativeRaw->Structures.size() == raw->Structures.size() &&
            nativeRaw->SourceInputs.Archive.lock() == compiled,
        "compiled input preserves source geometry and keeps its own archive identity");
  CHECK(std::ranges::equal(original.HeightTiles(), native.HeightTiles()) &&
            std::ranges::equal(original.HeightTiles(), original.Products().front().HeightTiles),
        "both paths produce the same complete terrain tile set");
  StructurePreparation invalid(
      pool, StructurePreparation::SourceInputs{source}, kDefaultStructurePolicy, -1);
  Barrier(pool);
  CHECK(invalid.Poll() == StructurePreparation::Phase::Failed && invalid.Products().empty() &&
            !invalid.Error().empty(),
        "invalid terrain demand fails without publishing partial buildings");
  StructurePreparation canceled(
      pool, StructurePreparation::SourceInputs{source}, kDefaultStructurePolicy, kHeightZoom);
  Barrier(pool);
  canceled.Cancel();
  CHECK(canceled.Poll() == StructurePreparation::Phase::Failed && canceled.Products().empty(),
        "cancellation before publication prevents a late ready product");
}
}

int main() {
  Products();
  return outshine::Test::Report();
}
