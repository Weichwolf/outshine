#include <cstdio>
#include <cstdlib>
#include <string>

#include <Event.h>
#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// THE ORACLE IS WHAT A SHADOW IS, AND IT OWES NOTHING TO OUR DESIGN. A shadow is fixed by three
// things -- where the light stands, where the occluder stands, and where the receiver stands. The
// camera is not among them. Walk around a lamppost at noon and its shadow does not walk with you.
//
// So: hold the light still, hold the caster still, MOVE THE EYE, and the depth the light sees must
// not change by one bit. The shadow atlas is that depth and nothing else, so its least, its most
// and the count of texels it wrote are all invariant under a camera that moves.
//
// WHAT THIS CATCHES is a whole class and not one bug. A renderer that works camera-relative must
// subtract an origin somewhere, and the moment TWO subsystems each subtract one for themselves
// they can disagree -- the caster is placed against one origin and the light's frustum against
// another, and the shadow lands somewhere that is a function of the CAMERA. That is exactly the
// defect this tree carried: the light stage folded the eye into its own matrix and then subtracted
// it a second time for each caster, and a day went into two wrong repairs before the shape of it
// was clear.
//
// Unreal answers it with `FViewMatrices::PreViewTranslation`: the frame chooses ONE origin and the
// view, the light and every instance transform are built against THAT one. The subtraction happens
// once, at the place the frame is set up, and a stage has nowhere to put a second one -- here it
// cannot even name an eye, because `FrameContext` no longer carries one and the compiler refuses
// the attempt.
//
// WHAT THE NEGATIVE CONTROL MEASURED is worth keeping, because it corrects the intuition this case
// was written with. Subtracting the origin a SECOND time -- at 2.0x and again at a mere 1.02x --
// does not shift the shadow a little. It empties the atlas: 710387 texels to 0. The light frustum
// is fitted tightly around the caster, so any fraction of the eye distance that leaks into it
// exceeds its radius and the frustum misses the geometry entirely.
//
// So this defect does not read as a shadow in the wrong place. It reads as NO shadow, which is
// exactly what the drive measured while it stood -- 0.7 counts of shadow in the picture against
// 56.4 once one origin was restored. The invariance below is the claim; the vacuity guard above it
// is what actually fires, and both are the case doing its work.
//
// THE LEVER IS `Fill`. It is the fraction of the frame the subject is framed to occupy, so it sets
// the camera's distance and nothing else: same subject, same sun, two eyes. Both arms are required
// to cast the same number of batches, because two arms that cast different geometry would compare
// two different shadows and prove nothing.
constexpr int kFramePx = 64;

constexpr const char *kTriangleBase64 =
    "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAA"
    "AAAIA/";

[[nodiscard]] std::string Minimal(void) {
  return std::string(
      "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
      "\"nodes\":[{\"mesh\":0}],"
      "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1},"
      "\"material\":0}]}],"
      "\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.8,0.8,1.0]}}],"
      "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
      "\"min\":[0,0,0],\"max\":[1,1,0]},"
      "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
      "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
      "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36}],"
      "\"buffers\":[{\"byteLength\":72,\"uri\":\"data:application/octet-stream;base64,") +
      kTriangleBase64 + "\"}]}";
}

[[nodiscard]] bool Wrote(const std::string &path, const std::string &held) {
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return false; }
  const bool whole = std::fwrite(held.data(), 1, held.size(), file) == held.size();
  return std::fclose(file) == 0 && whole;
}

[[nodiscard]] double Measured(const outshine::Engine &engine, const char *what) {
  for (const outshine::Measure &held : engine.Numbers()) {
    if (held.What == what) { return held.How; }
  }
  return -1.0;
}

struct Seen {
  double Least, Most, Written, Cast;
};

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case writes its subject into the runner's nest and was given none");
    return Report();
  }
  const std::string under = nest;
  if (!Wrote(under + "/caster.gltf", Minimal())) {
    Unprepared("the subject could not be written into the nest");
    return Report();
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be rendered");
    return Report();
  }

  outshine::Engine engine;
  engine.Under(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.DrawsInto(outshine::Extent{kFramePx, kFramePx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  outshine::Scenario stands;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;
  stands.Lit.Key.ElevationDeg = 42.0;
  stands.Lit.Key.BearingDeg = 0.0;
  outshine::Asset shown;
  shown.Uri = "caster.gltf";
  shown.Kind = "gltf";
  stands.Assets.push_back(shown);

  const auto standAt = [&](double fill, Seen &seen) {
    outshine::Scenario arm = stands;
    arm.Render.Fill = fill;
    if (!engine.Declare(arm) || !engine.Advance() || !engine.RenderTo(outshine::Extent{})) {
      return false;
    }
    if (!engine.Inspects()) { return false; }
    seen.Least = Measured(engine, "the shadow atlas, least depth");
    seen.Most = Measured(engine, "its most");
    seen.Written = Measured(engine, "texels above the clear");
    seen.Cast = Measured(engine, "batches the shadow casts");
    return true;
  };

  Seen near {}, far {};
  if (!standAt(0.85, near) || !standAt(0.20, far)) {
    Unprepared(("an arm did not stand: " + engine.Error()).c_str());
    return Report();
  }

  std::printf("EYE CLOSE (fill 0.85)  atlas %.6f .. %.6f   %.0f texel(s)   %.0f batch(es)\n",
              near.Least, near.Most, near.Written, near.Cast);
  std::printf("EYE FAR   (fill 0.20)  atlas %.6f .. %.6f   %.0f texel(s)   %.0f batch(es)\n",
              far.Least, far.Most, far.Written, far.Cast);

  CHECK(near.Written > 0.0 && near.Cast > 0.0,
        "the close arm wrote an atlas at all, so the comparison below has two shadows to compare "
        "and not two empties agreeing");
  CHECK(near.Cast == far.Cast,
        "both arms cast the SAME geometry, so what follows compares one shadow against itself "
        "under two cameras and not two different shadows");
  CHECK(near.Least == far.Least && near.Most == far.Most,
        "**MOVING THE EYE DOES NOTHING TO THE DEPTH THE LIGHT SEES**: a shadow is fixed by the "
        "light, the occluder and the receiver, and the camera is not among them. A depth that "
        "moves with the eye means the eye reached the light's matrix -- which is what a second "
        "subsystem subtracting its own origin does, and it is why the frame now carries ONE "
        "pre-view translation that every matrix reads (`FViewMatrices::PreViewTranslation`)");
  CHECK(near.Written == far.Written,
        "and the same texels are written, not merely the same range: a frustum that shifted with "
        "the camera would still hold the caster and cover DIFFERENT ground, so the count is the "
        "half of this the range alone cannot see");

  Covers("the render: the frame carries one pre-view translation, every matrix is built against "
         "that one, and a camera that moves leaves the shadow exactly where it stood");
  return Report();
}
