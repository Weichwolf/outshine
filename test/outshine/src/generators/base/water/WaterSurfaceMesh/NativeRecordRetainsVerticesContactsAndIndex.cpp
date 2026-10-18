#include "WaterSurfaceMeshCodec.h"
#include "WaterSurfaceMesh.h"
#include "BinaryValueArchive.h"
#include "Check.h"
#include "math/Vec3.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto built = WaterSurfaceMesh::Build(
      {.LongitudeDeg = 21, .LatitudeDeg = -37},
      {{{-100, -100, 123.456789123}},
       {{100, -100, 124.456789123}},
       {{100, 100, 123.956789123}},
       {{-100, 100, 123.956789123}}},
      {{.Vertices = {0, 1, 2}, .Surface = 17}, {.Vertices = {0, 2, 3}, .Surface = 17}});
  CHECK(built, "a canonical mesh is available for the native record");
  if (!built) { return Report(); }
  BinaryValueWriter out(4096);
  CHECK(WaterSurfaceMeshCodec::Fields(out, *built), "native mesh fits a bounded record");
  const auto bytes = std::move(out.Out).TakeBytes();
  BinaryValueReader in(bytes);
  WaterSurfaceMesh loaded;
  CHECK(WaterSurfaceMeshCodec::Fields(in, loaded) && in.In.Remaining() == 0,
        "the native record loads ready geometry and an already partitioned index");
  CHECK(std::ranges::equal(loaded.VerticesEnuM(), built->VerticesEnuM()) &&
            std::ranges::equal(loaded.Hierarchy().SourceOrder(), built->Hierarchy().SourceOrder()),
        "Float64 canonical coordinates and stored source order survive without quantization");
  CHECK(loaded.Faces().size() == 2 && loaded.Faces()[1].Surface == 17,
        "native facets retain source-surface binding");
  for (const double delta : {-.0001, 0., .0001}) {
    const LongitudeLatitude at{.LongitudeDeg = 21 + delta, .LatitudeDeg = -37 + delta};
    const auto before = built->LevelAt(at);
    const auto after = loaded.LevelAt(at);
    CHECK(before && after && *before == *after,
          "fresh native contacts are bitwise identical after loading");
  }
  BinaryValueWriter roundTrip(4096);
  CHECK(WaterSurfaceMeshCodec::Fields(roundTrip, loaded), "the loaded record can be stored");
  CHECK(std::move(roundTrip.Out).TakeBytes() == bytes,
        "record replay preserves every stored index bound and field byte");
  BinaryValueWriter tooSmall(bytes.size() - 1);
  CHECK(!WaterSurfaceMeshCodec::Fields(tooSmall, loaded), "the byte limit is binding");
  for (size_t size = 0; size < bytes.size(); ++size) {
    BinaryValueReader truncated(std::span(bytes).first(size));
    WaterSurfaceMesh untouched = loaded;
    CHECK(!WaterSurfaceMeshCodec::Fields(truncated, untouched),
          "every truncated native record is rejected");
    CHECK(untouched.LevelAt(untouched.Anchor()) == loaded.LevelAt(loaded.Anchor()),
          "failed native loads never replace a previously valid contact mesh");
  }
  auto oversized = bytes;
  std::ranges::fill(std::span(oversized).subspan(2 * sizeof(double), sizeof(uint64_t)), 255);
  BinaryValueReader badCount(oversized);
  CHECK(!WaterSurfaceMeshCodec::Fields(badCount, loaded),
        "counts exceeding available input bytes cannot allocate a mesh");
  return Report();
}
