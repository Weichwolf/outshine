#include "HeightProgram.h"
#include "Check.h"

#include <limits>
#include <stop_token>

namespace {

outshine::Generators::RoadProfiles::HeightProgram Program() {
  return {.Conditions =
              {.Rows = 1, .Columns = 1, .ColumnOffsets = {0, 1}, .RowIndices = {0}, .Values = {-1}},
          .Upper = {-.5},
          .Variables = {{0, 1}},
          .AdjustmentVariable = 0};
}

void InvalidSparseStorageIsRefusedBeforeLibraryAccess() {
  using namespace outshine::Generators::RoadProfiles;
  using namespace outshine::Test;
  auto program = Program();
  program.Conditions.ColumnOffsets[1] = 2;
  auto fit = FitHeights(program, {}, {});
  CHECK(!fit && fit.error() == HeightFitError::InvalidInput,
        "out-of-bounds sparse offsets are refused");
  program = Program();
  program.Conditions.RowIndices[0] = -1;
  fit = FitHeights(program, {}, {});
  CHECK(!fit && fit.error() == HeightFitError::InvalidInput, "negative row indices are refused");
  program = Program();
  program.Conditions.Values[0] = std::numeric_limits<double>::quiet_NaN();
  fit = FitHeights(program, {}, {});
  CHECK(!fit && fit.error() == HeightFitError::InvalidInput, "nonfinite coefficients are refused");
  program = Program();
  program.Variables[0].Minimum = 2;
  fit = FitHeights(program, {}, {});
  CHECK(!fit && fit.error() == HeightFitError::InvalidInput,
        "reversed variable bounds are refused");
}

void CapacityAndCancellationRemainExplicit() {
  using namespace outshine::Generators::RoadProfiles;
  using namespace outshine::Test;
  const auto program = Program();
  auto fit = FitHeights(program, {.Nonzeros = 0}, {});
  CHECK(!fit && fit.error() == HeightFitError::CapacityExceeded,
        "the local nonzero budget is enforced");
  std::stop_source stop;
  stop.request_stop();
  fit = FitHeights(program, {}, stop.get_token());
  CHECK(!fit && fit.error() == HeightFitError::Canceled,
        "pre-canceled jobs do not enter the library");
  fit = FitHeights(program, {.Seconds = 0}, {});
  CHECK(!fit && fit.error() == HeightFitError::InvalidInput,
        "a zero time budget is not unlimited work");
  fit = FitHeights(program, {}, {});
  CHECK(fit.has_value(), "a valid local problem remains available after rejected inputs");
  if (fit) {
    CHECK_NEAR(fit->MaximumAdjustmentM, .5, 1e-9, "m", "the one-variable optimum is retained");
  }
}

}

int main() {
  InvalidSparseStorageIsRefusedBeforeLibraryAccess();
  CapacityAndCancellationRemainExplicit();
  return outshine::Test::Report();
}
