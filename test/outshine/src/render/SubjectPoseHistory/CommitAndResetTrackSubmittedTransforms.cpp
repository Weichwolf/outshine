#include "Check.h"
#include "SubjectPoseHistory.h"

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;

  SubjectPoseHistory history;
  history.Resize(2);
  Mat4 body;
  body[12] = 4.0;
  Mat4 built;
  built[13] = 7.0;
  CHECK(history.BodyChanged(0, body) && history.BuiltChanged(built),
        "an unsubmitted pose differs from every finite transform");

  history.Commit(0, body, built);
  CHECK(!history.BodyChanged(0, body) && !history.BuiltChanged(built),
        "commit records both halves of the submitted pose");
  CHECK(history.BodyChanged(1, body), "each body keeps independent submission history");

  history.Reset();
  CHECK(history.BodyChanged(0, body) && history.BuiltChanged(built),
        "reset invalidates all submitted transforms together");
  return Report();
}
