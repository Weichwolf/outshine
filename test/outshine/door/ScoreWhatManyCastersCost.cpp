#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <vector>

#include <Event.h>
#include <Generate.h>
#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// CLAUDE.md BOUNDS THE FRAME PATH: *bounded terms on the frame path (no alloc/lock/disk/unbounded
// block)*, and board:1926 asks the sharper question -- does any CPU term SCALE with geometry,
// lights or pixels? Neither can be answered by reading. A scenario with many casters had to exist
// first, and one does now: a client's generator makes N parts, and each part is a batch the shadow
// pass draws.
//
// TWO NUMBERS, AND THEY SAY DIFFERENT THINGS.
//
//   bytes the frame's drawing left behind   must not GROW with N. CLAUDE.md's word is *bounded*,
//                                           not zero, and bounded is the property that matters: a
//                                           heap that grows with the scene is the frame that
//                                           pauses once the scene is large enough. Measured, it is
//                                           128 bytes at one caster and 128 at sixty-four -- the
//                                           same 128, which is a per-frame allocation that does
//                                           not scale. Asserting zero here would have been
//                                           asserting a stronger thing than the tree does and
//                                           than CLAUDE.md asks
//   batches the shadow casts                is EXPECTED to equal N. The shadow pass issues one
//                                           draw per batch today, so this number IS the CPU term
//                                           board:1926 wants removed -- measured rather than
//                                           asserted, so the day it becomes O(1) the case says so
//
// The second is not a failure and is not asserted as one. It is the measurement that decides
// whether the indirect path board:1926 describes is worth building, and it belongs in the tree
// rather than in a note: Unreal's `FGPUScene` exists because that number grew, and reading it here
// is how this tree learns the same thing about itself instead of copying the conclusion.
constexpr int kFramePx = 96;
constexpr int kFew = 1;
constexpr int kMany = 64;

class Crates final : public outshine::Generates {
public:
  explicit Crates(int many) : Many_(many) {}

  [[nodiscard]] std::string_view Kind() const override { return "crates"; }

  [[nodiscard]] bool Make(const outshine::Ask &, outshine::Geometry &into) const override {
    outshine::Material grey;
    grey.BaseColour[0] = 0.6f;
    grey.BaseColour[1] = 0.6f;
    grey.BaseColour[2] = 0.6f;
    grey.Roughness = 0.9f;
    const int named = into.Surface("crate", grey);
    for (int at = 0; at < Many_; ++at) {
      const float acrossM = 0.35f;
      const float alongM = (float)((at % 8) - 4) * 1.1f;
      const float upM = (float)((at / 8) % 8) * 1.1f;
      const float face[18] = {alongM - acrossM, upM - acrossM, 0.0f,
                              alongM + acrossM, upM - acrossM, 0.0f,
                              alongM + acrossM, upM + acrossM, 0.0f,
                              alongM - acrossM, upM - acrossM, 0.0f,
                              alongM + acrossM, upM + acrossM, 0.0f,
                              alongM - acrossM, upM + acrossM, 0.0f};
      constexpr float facing[18] = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
      constexpr uint32_t run[6] = {0, 1, 2, 3, 4, 5};
      const int part = into.Part("crate", named);
      if (!into.Positions(part, std::span<const float>(face, 18)) ||
          !into.Normals(part, std::span<const float>(facing, 18)) ||
          !into.Triangles(part, std::span<const uint32_t>(run, 6))) {
        return false;
      }
    }
    return true;
  }

private:
  int Many_;
};

[[nodiscard]] double Measured(const outshine::Engine &engine, const char *what) {
  for (const outshine::Measure &held : engine.measures()) {
    if (held.What == what) { return held.How; }
  }
  return -1.0;
}

struct Cost {
  bool Stood = false;
  double Batches = 0.0;
  double Bytes = 0.0;
};

[[nodiscard]] Cost Drew(int many, std::string &why) {
  Cost out;
  const Crates makes(many);
  outshine::Engine engine;
  engine.setRoots(outshine::Roots{".", "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.drawsInto(outshine::Extent{kFramePx, kFramePx})) {
    why = "the device stood no canvas";
    return out;
  }
  engine.offers(makes);

  outshine::Scenario stands;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  stands.Render.Fill = 0.6;
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;
  stands.Lit.Key.ElevationDeg = 42.0;
  outshine::Asset made;
  made.Uri = "crates";
  made.Kind = "generated";
  stands.Assets.push_back(made);

  if (!engine.declare(stands) || !engine.render(outshine::Extent{}) ||
      !engine.render(outshine::Extent{})) {
    why = engine.error();
    return out;
  }
  out.Batches = Measured(engine, "batches the shadow casts");
  out.Bytes = Measured(engine, "bytes the frame's drawing left behind");
  out.Stood = true;
  return out;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be drawn");
    return Report();
  }

  std::string why;
  const Cost few = Drew(kFew, why);
  if (!few.Stood) {
    Unprepared(("one crate did not stand: " + why).c_str());
    return Report();
  }
  const Cost many = Drew(kMany, why);
  if (!many.Stood) {
    Unprepared(("sixty-four crates did not stand: " + why).c_str());
    return Report();
  }

  std::printf("ONE CASTER          %3.0f shadow batch(es), drawing left %6.0f bytes\n", few.Batches,
              few.Bytes);
  std::printf("SIXTY-FOUR CASTERS  %3.0f shadow batch(es), drawing left %6.0f bytes\n",
              many.Batches, many.Bytes);

  CHECK(many.Batches > few.Batches,
        "sixty-four crates cast more than one does, so the scene really grew and the byte "
        "readings below are taken over two different amounts of work");
  CHECK(few.Bytes == many.Bytes,
        "**DRAWING A FRAME ALLOCATES THE SAME AT ANY SIZE OF SCENE**: sixty-four times the casters "
        "leaves the same bytes behind as one. CLAUDE.md's word is BOUNDED rather than zero, and "
        "bounded is the property that matters -- a heap that grows with the scene is a frame that "
        "pauses once the scene is large enough");
  CHECK(few.Bytes < 4096.0,
        "and what it does leave is small and constant, not merely equal: two large allocations "
        "that happened to match would satisfy the check above and would not be a bounded frame "
        "path");
  CHECK(std::fabs(many.Batches - (double)kMany) < 0.5,
        "and the shadow pass draws ONE BATCH PER CASTER, which is measured rather than asserted "
        "and is the CPU term board:1926 wants removed: `Cast` issues one "
        "`SDL_DrawGPUIndexedPrimitives` per batch and a uniform push per model slot. The day it "
        "becomes one indirect draw this number stops tracking the scene, and this case is where "
        "that shows");

  Covers("the render: drawing a frame allocates a bounded, constant amount at any scene size, and "
         "the shadow pass issues one draw per caster -- the term board:1926 measures before "
         "deciding to remove it");
  return Report();
}
