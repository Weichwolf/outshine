#include <atomic>
#include <algorithm>

#include "src/generators/building/StructureBake.h"
#include "Check.h"

namespace {
struct Scratch final : outshine::MeshScratch {};

class RefusingMesher final : public outshine::StructureMesher {
public:
  explicit RefusingMesher(outshine::StructureMeshError error) : Error_(error) {}

  std::unique_ptr<outshine::MeshScratch> Scratch() const override {
    return std::make_unique<::Scratch>();
  }

  std::expected<void, outshine::StructureMeshError>
  Mesh(const outshine::StructurePlan &plan,
       outshine::MeshScratch &,
       outshine::Raised &) const noexcept override {
    ++Calls;
    LastCoarseness = plan.Coarseness;
    return std::unexpected(Error_);
  }

  mutable size_t Calls = 0;
  mutable outshine::LevelOfDetail LastCoarseness = outshine::LevelOfDetail::Fine;

private:
  outshine::StructureMeshError Error_;
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Generators::RawTile raw;
  raw.LatLon = {47, 9, 47, 9.0001, 47.0001, 9.0001, 47.0001, 9};
  raw.Structures.push_back({.PointCount = 4, .Cell = {.Index = 1}, .HeightM = 6});
  raw.FocalPx = 1000;
  raw.Eye = {.LongitudeDeg = 9, .LatitudeDeg = 47};
  raw.TileSpanM = 1000;
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0);
  const auto heights = Ground::HeightField::Of(0, {block});
  for (auto error : {StructureMeshError::InvalidPlan,
                     StructureMeshError::IncompatibleScratch,
                     StructureMeshError::BuildFailed}) {
    RefusingMesher mesher(error);
    auto scratch = mesher.Scratch();
    Generators::BakedTile output;
    const auto result = Generators::BakeStructures(raw, *heights, mesher, *scratch, output);
    CHECK(mesher.Calls == 1, "fixture reaches actual per-building mesher");
    CHECK(!result, "mesh rejection cannot become a successful tile bake");
    if (!result) {
      const auto *reason = std::get_if<StructureMeshError>(&result.error());
      CHECK(reason && *reason == error, "specific mesher error reaches bake boundary");
      CHECK(!Generators::Describe(result.error()).empty(),
            "diagnostic is available without allocation");
    }
  }
  raw.Structures.push_back(raw.Structures.front());
  RefusingMesher unsupported(StructureMeshError::UnsupportedFootprint);
  auto scratch = unsupported.Scratch();
  Generators::BakedTile output;
  const auto result = Generators::BakeStructures(raw, *heights, unsupported, *scratch, output);
  CHECK(result && unsupported.Calls == 2 && output.UnsupportedMeshes == 2,
        "unsupported forms are counted and processing continues to the next building");
  raw.LatLon.insert(raw.LatLon.end(), {47, 8.9998, 47.0001, 8.9998});
  raw.Ways.push_back({.LocalFirst = 4, .PointCount = 2, .HalfWidthM = 4});
  RefusingMesher frontedMesher(StructureMeshError::UnsupportedFootprint);
  auto frontedScratch = frontedMesher.Scratch();
  Generators::BakedTile fronted;
  const auto frontedResult =
      Generators::BakeStructures(raw, *heights, frontedMesher, *frontedScratch, fronted);
  CHECK(frontedResult && fronted.Fronted == 2,
        "a nearby road line gives every building in its tile a known frontage");

  Generators::RawTile distant = raw;
  distant.Eye = {.LongitudeDeg = 9, .LatitudeDeg = 0};
  RefusingMesher distantMesher(StructureMeshError::UnsupportedFootprint);
  auto distantScratch = distantMesher.Scratch();
  Generators::BakedTile distantOutput;
  const auto distantResult =
      Generators::BakeStructures(distant, *heights, distantMesher, *distantScratch, distantOutput);
  CHECK(distantResult, "a distant tile finishes its bake");
  CHECK(distantMesher.Calls == 1 && distantMesher.LastCoarseness == LevelOfDetail::Massed,
        "a distant tile batches its buildings into one massed mesh");
  CHECK(distantOutput.Prints.size() == 2, "a distant tile retains every footprint");
  CHECK(distantOutput.FootprintDetails.front() == LevelOfDetail::Massed,
        "a distant building uses massed geometry");

  Generators::RawTile threshold = raw;
  threshold.LatLon = {47, 9.0047, 47, 9.0048, 47.0001, 9.0048, 47.0001, 9.0047};
  threshold.Structures.resize(1);
  threshold.Ways.clear();
  RefusingMesher thresholdMesher(StructureMeshError::UnsupportedFootprint);
  auto thresholdScratch = thresholdMesher.Scratch();
  Generators::BakedTile thresholdOutput;
  const auto thresholdResult = Generators::BakeStructures(
      threshold, *heights, thresholdMesher, *thresholdScratch, thresholdOutput);
  CHECK(thresholdResult && thresholdOutput.Prints.size() == 1 &&
            thresholdOutput.FootprintDetails.front() == LevelOfDetail::Fine,
        "the eye reuse guard retains fine detail just beyond the ordinary shell threshold");

  RefusingMesher oneShotMesher(StructureMeshError::UnsupportedFootprint);
  auto oneShotScratch = oneShotMesher.Scratch();
  Generators::BakedTile oneShot;
  const auto oneShotResult =
      Generators::BakeStructures(raw, *heights, oneShotMesher, *oneShotScratch, oneShot);
  RefusingMesher slicedMesher(StructureMeshError::UnsupportedFootprint);
  auto slicedScratch = slicedMesher.Scratch();
  Generators::StructureBakeProgress progress;
  const auto first = progress.AdvanceStructures(raw, *heights, slicedMesher, *slicedScratch, 1);
  CHECK(progress.BakedStructures() == 1,
        "a completed range exposes its exact structure count after the worker boundary");
  const auto second = progress.AdvanceStructures(raw, *heights, slicedMesher, *slicedScratch, 1);
  CHECK(oneShotResult && first && second && !*first && *second,
        "a bounded bake retains its aggregate until its final structure range");
  auto sliced = progress.Finalize(raw, slicedMesher, *slicedScratch);
  CHECK(sliced && slicedMesher.Calls == oneShotMesher.Calls &&
            sliced->Prints.size() == oneShot.Prints.size() &&
            sliced->UnsupportedMeshes == oneShot.UnsupportedMeshes &&
            sliced->Built.WallRun == oneShot.Built.WallRun &&
            sliced->Built.RoofRun == oneShot.Built.RoofRun,
        "one-structure ranges produce the same complete tile as one uninterrupted bake");

  Generators::RawTile many = raw;
  many.Ways.clear();
  many.Structures.assign(257, raw.Structures.front());
  RefusingMesher manyOneShotMesher(StructureMeshError::UnsupportedFootprint);
  auto manyOneShotScratch = manyOneShotMesher.Scratch();
  Generators::BakedTile manyOneShot;
  const auto manyOneShotResult = Generators::BakeStructures(
      many, *heights, manyOneShotMesher, *manyOneShotScratch, manyOneShot);
  RefusingMesher manySlicedMesher(StructureMeshError::UnsupportedFootprint);
  auto manySlicedScratch = manySlicedMesher.Scratch();
  Generators::StructureBakeProgress manyProgress;
  for (size_t range = 0; range < 4; ++range) {
    const auto incomplete =
        manyProgress.AdvanceStructures(many, *heights, manySlicedMesher, *manySlicedScratch, 64);
    CHECK(incomplete && !*incomplete && manyProgress.BakedStructures() == (range + 1) * 64,
          "each complete 64-structure range retains the private aggregate");
  }
  const auto manyStructuresComplete =
      manyProgress.AdvanceStructures(many, *heights, manySlicedMesher, *manySlicedScratch, 64);
  CHECK(manyStructuresComplete && *manyStructuresComplete,
        "the final structure range completes without exposing its private aggregate");
  auto manyFinalized = manyProgress.Finalize(many, manySlicedMesher, *manySlicedScratch);
  CHECK(manyOneShotResult && manyFinalized &&
            manyProgress.BakedStructures() == many.Structures.size(),
        "the final short range and finalization complete a 257-structure aggregate");
  CHECK(manyFinalized && manySlicedMesher.Calls == manyOneShotMesher.Calls &&
            manyFinalized->Prints.size() == manyOneShot.Prints.size() &&
            std::ranges::equal(manyFinalized->Prints,
                               manyOneShot.Prints,
                               [](const auto &left, const auto &right) {
                                 return left.FirstPoint == right.FirstPoint &&
                                        left.PointCount == right.PointCount &&
                                        left.HeightM == right.HeightM &&
                                        left.BaseM == right.BaseM && left.SeatM == right.SeatM &&
                                        left.FootM == right.FootM && left.Source == right.Source &&
                                        left.Street.Known == right.Street.Known &&
                                        left.Street.KerbEm == right.Street.KerbEm &&
                                        left.Street.KerbNm == right.Street.KerbNm &&
                                        left.Street.AlongE == right.Street.AlongE &&
                                        left.Street.AlongN == right.Street.AlongN &&
                                        left.Street.ToStreetE == right.Street.ToStreetE &&
                                        left.Street.ToStreetN == right.Street.ToStreetN;
                               }) &&
            manyFinalized->FootprintDetails == manyOneShot.FootprintDetails &&
            manyFinalized->SeatSpreadM == manyOneShot.SeatSpreadM &&
            manyFinalized->AcrossM == manyOneShot.AcrossM &&
            manyFinalized->Digest == manyOneShot.Digest &&
            manyFinalized->UnsupportedMeshes == manyOneShot.UnsupportedMeshes &&
            manyFinalized->OsmHeights == manyOneShot.OsmHeights &&
            manyFinalized->DefaultHeights == manyOneShot.DefaultHeights &&
            manyFinalized->Fronted == manyOneShot.Fronted &&
            manyFinalized->Lumped == manyOneShot.Lumped &&
            manyFinalized->Blocks == manyOneShot.Blocks &&
            manyFinalized->NoGround == manyOneShot.NoGround &&
            manyFinalized->Built.WallRun == manyOneShot.Built.WallRun &&
            manyFinalized->Built.RoofRun == manyOneShot.Built.RoofRun,
        "five bounded ranges equal one uninterrupted 257-structure bake");

  RefusingMesher cancelledMesher(StructureMeshError::UnsupportedFootprint);
  auto cancelledScratch = cancelledMesher.Scratch();
  Generators::StructureBakeProgress cancelled;
  std::atomic_bool stopping{false};
  const auto beforeCancel =
      cancelled.AdvanceStructures(raw, *heights, cancelledMesher, *cancelledScratch, 1, &stopping);
  stopping.store(true);
  const auto afterCancel =
      cancelled.AdvanceStructures(raw, *heights, cancelledMesher, *cancelledScratch, 1, &stopping);
  CHECK(beforeCancel && !*beforeCancel && !afterCancel,
        "cancelling between ranges cannot report a complete tile");
  if (!afterCancel) {
    const auto *reason = std::get_if<Generators::StructureBakeErrorKind>(&afterCancel.error());
    CHECK(reason && *reason == Generators::StructureBakeErrorKind::Cancelled,
          "cancellation reaches the generator boundary as its explicit error");
  }
  return Report();
}
