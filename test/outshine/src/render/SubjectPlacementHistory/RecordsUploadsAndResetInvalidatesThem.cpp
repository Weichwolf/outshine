#include "Check.h"
#include "SubjectPlacementHistory.h"

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;

  SubjectPlacementHistory history;
  history.Resize(2);
  Mat4 body;
  body[12] = 4.0;
  Mat4 built;
  built[13] = 7.0;
  CHECK(history.NeedsBodyUpload(0, body) && history.NeedsBuiltUpload(built),
        "an unrecorded placement differs from every finite transform");

  history.RecordsUpload(0, body, built);
  CHECK(!history.NeedsBodyUpload(0, body) && !history.NeedsBuiltUpload(built),
        "the recorded upload contains both placement transforms");
  CHECK(history.NeedsBodyUpload(1, body), "each body keeps independent uploaded-placement history");

  history.Reset();
  CHECK(history.NeedsBodyUpload(0, body) && history.NeedsBuiltUpload(built),
        "reset invalidates all uploaded placement transforms together");
  return Report();
}
