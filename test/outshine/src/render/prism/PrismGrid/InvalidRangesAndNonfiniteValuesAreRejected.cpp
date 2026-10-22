#include "PrismGrid.h"
#include "Check.h"
#include <array>
#include <cstdint>
#include <limits>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
}

int main() {
  constexpr float nan = std::numeric_limits<float>::quiet_NaN();
  constexpr float infinity = std::numeric_limits<float>::infinity();
  const std::array valid{PrismSpan{.Lower = 0, .Upper = 6}, PrismSpan{.Lower = 1, .Upper = 2}};
  const PrismPatch unit{.Columns = 1, .Rows = 1};
  CHECK(!InspectPrismPatch({}, valid), "zero raster dimensions are invalid");
  auto outside = unit;
  outside.FirstSample = std::numeric_limits<uint64_t>::max();
  CHECK(!InspectPrismPatch(outside, valid), "a huge first sample cannot wrap into the buffer");
  outside.Columns = outside.Rows = std::numeric_limits<uint32_t>::max();
  CHECK(!InspectPrismPatch(outside, valid), "a huge interval is rejected without offset addition");
  outside = {.FirstSample = 1, .Columns = 2, .Rows = 1};
  CHECK(!InspectPrismPatch(outside, valid), "the entire addressed interval must be present");
  for (const float invalid : {0.f, -1.f, nan, infinity}) {
    auto patch = unit;
    patch.PitchX = invalid;
    CHECK(!InspectPrismPatch(patch, valid), "X pitch must be finite and positive");
    patch = unit;
    patch.PitchZ = invalid;
    CHECK(!InspectPrismPatch(patch, valid), "Z pitch must be finite and positive");
  }
  for (const float invalid : {0.f, -1.f, 1.01f, nan, infinity}) {
    auto patch = unit;
    patch.FillFraction = invalid;
    CHECK(!InspectPrismPatch(patch, valid), "fill is an explicit finite fraction in (0,1]");
  }
  for (const auto cell : {PrismSpan{.Lower = nan, .Upper = 0},
                          PrismSpan{.Lower = 0, .Upper = infinity},
                          PrismSpan{.Lower = -infinity, .Upper = 0}}) {
    const std::array invalid{cell};
    CHECK(!InspectPrismPatch(unit, invalid), "unknown nonfinite endpoints never imply empty space");
  }
  auto wide = PrismPatch{.Columns = 2, .Rows = 1};
  wide.PitchX = std::numeric_limits<float>::max();
  CHECK(!InspectPrismPatch(wide, valid), "unsupported Float32 extents fail before narrowing");
  const std::array tooHigh{PrismSpan{.Lower = -std::numeric_limits<float>::max(),
                                     .Upper = std::numeric_limits<float>::max()}};
  CHECK(!InspectPrismPatch(unit, tooHigh),
        "the proxy's complete vertical extent must remain finite");
  return Report();
}
