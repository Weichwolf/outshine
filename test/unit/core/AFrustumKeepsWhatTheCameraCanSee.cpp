#include <cmath>
#include <cstdio>
#include <vector>

#include "Check.h"

#include "Camera.h"

using outshine::AabbVisible;
using outshine::CameraBasis;
using outshine::CameraBasisFrom;
using outshine::Frustum;
using outshine::FrustumFrom;

namespace {

// board:1806, found by the claim that walks the map rather than by the hand sweep that filed
// it: Frustum is drawn in the CURRENT class diagram and named by nothing under test/. It is
// what decides whether anything is drawn at all, and its whole contract is one asymmetry --
// it may keep a box the camera cannot see, and it may never DROP one the camera can.
struct Box {
  float Min[3];
  float Max[3];
};

[[nodiscard]] Box At(float eastM, float upM, float northM, float halfM) {
  return Box{{eastM - halfM, upM - halfM, northM - halfM},
             {eastM + halfM, upM + halfM, northM + halfM}};
}

// the ground truth a frustum test may not contradict: a point is visible when its clip
// coordinates lie inside the canonical volume. Computed here from the matrix directly, so this
// is the plane extraction being checked rather than the extraction agreeing with itself.
[[nodiscard]] bool PointInClip(const float mvp[16], float x, float y, float z) {
  float clip[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  for (int at = 0; at < 4; ++at) {
    clip[at] = mvp[0 * 4 + at] * x + mvp[1 * 4 + at] * y + mvp[2 * 4 + at] * z + mvp[3 * 4 + at];
  }
  const float w = clip[3];
  return w > 0.0f && std::fabs(clip[0]) <= w && std::fabs(clip[1]) <= w &&
         clip[2] >= -w && clip[2] <= w;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const float eye[3] = {0.0f, 0.0f, 0.0f};
  const CameraBasis basis = CameraBasisFrom(0.0f, 0.0f, 0.0f, eye, 60.0f, 16.0f / 9.0f, 0.1f,
                                            1000.0f);
  const Frustum look = FrustumFrom(basis.mvp);
  Note("planes the frustum carries", 6.0, "planes");

  // 1. the obvious four: ahead is kept, behind and far outside are dropped.
  const struct {
    Box Where;
    int Wanted;
    const char *Why;
  } kPlain[] = {
      {At(0.0f, 0.0f, -10.0f, 1.0f), 1, "ten metres straight ahead"},
      {At(0.0f, 0.0f, 10.0f, 1.0f), 0, "ten metres straight behind"},
      {At(0.0f, 0.0f, -2000.0f, 1.0f), 0, "twice as far as the far plane"},
      {At(500.0f, 0.0f, -10.0f, 1.0f), 0, "far off to the right at ten metres"},
      {At(0.0f, 500.0f, -10.0f, 1.0f), 0, "far above at ten metres"},
  };
  size_t agreed = 0;
  for (const auto &one : kPlain) {
    const int kept = AabbVisible(&look, one.Where.Min, one.Where.Max);
    std::printf("NOTE %-42s kept:%d wanted:%d\n", one.Why, kept, one.Wanted);
    agreed += kept == one.Wanted ? 1u : 0u;
  }
  Note("rows the frustum answered as the geometry asks", (double)agreed, "of 5");
  CHECK(agreed == 5,
        "**A FRUSTUM KEEPS WHAT IS IN FRONT AND DROPS WHAT IS BEHIND**: six planes extracted "
        "from the view-projection matrix, and a box outside any one of them is outside the "
        "picture (board:1806)");

  // 2. the contract that matters: the test is CONSERVATIVE. A plane test on a box's positive
  //    vertex may keep a box no pixel of which is visible -- that costs a draw. It may never
  //    drop a box a pixel of which is visible -- that is a hole in the world. Swept over a
  //    grid of places, every point the clip volume accepts must sit in a box the frustum keeps.
  long swept = 0, seen = 0, dropped = 0;
  for (int ix = -40; ix <= 40; ++ix) {
    for (int iy = -24; iy <= 24; ++iy) {
      for (int iz = 1; iz <= 60; ++iz) {
        const float x = (float)ix * 4.0f;
        const float y = (float)iy * 4.0f;
        const float z = -(float)iz * 8.0f;
        ++swept;
        if (!PointInClip(basis.mvp, x, y, z)) { continue; }
        ++seen;
        const Box box = At(x, y, z, 0.25f);
        if (!AabbVisible(&look, box.Min, box.Max)) { ++dropped; }
      }
    }
  }
  Note("places swept", (double)swept, "places");
  Note("of them inside the clip volume", (double)seen, "places");
  Note("boxes around a visible point the frustum DROPPED", (double)dropped, "boxes");
  CHECK(seen > 1000, "the sweep really crossed the picture, so the next claim is not vacuous");
  CHECK(dropped == 0,
        "**AND IT NEVER DROPS WHAT THE CAMERA CAN SEE**: the cull is conservative by "
        "construction -- keeping a box nobody sees costs a draw call, dropping one somebody "
        "sees is a hole in the world, and only one of those two is recoverable");

  // 3. it is a function of the camera and of nothing else: turn the camera around and the two
  //    answers exchange.
  const CameraBasis behind = CameraBasisFrom(180.0f, 0.0f, 0.0f, eye, 60.0f, 16.0f / 9.0f, 0.1f,
                                             1000.0f);
  const Frustum turned = FrustumFrom(behind.mvp);
  const Box ahead = At(0.0f, 0.0f, -10.0f, 1.0f);
  const Box back = At(0.0f, 0.0f, 10.0f, 1.0f);
  Note("with the camera turned, the box ahead", (double)AabbVisible(&turned, ahead.Min, ahead.Max),
       "");
  Note("and the box behind", (double)AabbVisible(&turned, back.Min, back.Max), "");
  CHECK(AabbVisible(&turned, ahead.Min, ahead.Max) == 0 &&
            AabbVisible(&turned, back.Min, back.Max) == 1,
        "and turning the camera exchanges the two answers, so the frustum is the camera's own "
        "and carries no state of its own between frames");

  Covers("I.2.9 the frustum keeps what the camera can see and drops what it cannot, and it is "
         "conservative in the one direction that matters: over a sweep of the picture it never "
         "drops a box containing a point the clip volume accepts (board:1806)");
  return Report();
}
