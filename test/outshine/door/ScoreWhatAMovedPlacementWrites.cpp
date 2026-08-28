#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

// A SUBJECT THAT CHANGES PLACE WRITES A VELOCITY, AND A STILL ONE WRITES NONE.
//
// Unreal keeps a per-instance PREVIOUS transform in `FGPUScene` and advances it where the frame
// ends; RAGE double-buffers the same value. Both agree, and the reason is the same in either: TAA
// reprojects this frame's pixel to where it stood last frame, so a rigid subject whose transform
// changed and whose previous transform did NOT fetches its history from where it is not.
//
// THIS CASE EXISTS BECAUSE THE CORPUS CANNOT REACH THE QUESTION. `harness/shared/render/
// Parity.cpp` calls `PoseGeometry` and hands the engine BAKED vertices with `prevP`, so even a
// node-rotation asset like `khronos/glTF/AnimatedCube` arrives as a STATIC transform over moving
// vertices. Measured: with the previous placement row forced to equal the current one, khronos/
// glTF stays 444/444 and the driver's own moving-scene case reads the same 57600 px and the same
// 0.695012 ndc -- because that camera drives too and its motion sets both numbers. Nothing in
// this tree could tell the two apart.
//
// So this case declares the isolation instead of borrowing it: a FIXED eye over two subjects that
// differ in one thing, a node TRANSLATION track. The moving-pixel count is then the subject's own
// silhouette and nothing else's, and it is the tree's only door-level proof that the velocity
// target describes a subject rather than a camera.
//
// WHAT IT STILL DOES NOT REACH, measured rather than guessed. It does NOT exercise the placement
// row's previous half: with that forced to equal the current transform this case reads the same
// 0 and 118. The engine bakes node transforms into VERTICES exactly as the harness does, so an
// animated glTF arrives as moving vertices over a static transform and the velocity comes from
// `prevP`. The only thing in this tree that moves a PLACEMENT is a BODY, through `Live::Places`
// -- and a declared body cannot be measured here either, for a reason worth writing down:
// `Engine::State::Carries` computes the EYE from the body it carries, so the camera follows the
// falling body and the relative motion is zero by construction. Measured, with a body declared
// `Placed` at 0.05 m: 0 moving pixels over three frames. A fixed eye over a moving placement is
// not declarable through this door, and that is board:1998's next step rather than this case's.

namespace {

constexpr int kFramePx = 64;

constexpr const char *kTriangleBase64 =
    "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAA"
    "AAAIA/";

// Two keys a second apart: the node stands at the origin, then one unit east. 32 bytes -- two
// floats of time, then two VEC3 of translation.
constexpr const char *kTrackBase64 = "AAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAA=";

[[nodiscard]] std::string Walking(bool moving) {
  std::string held =
      std::string("{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
                  "\"nodes\":[{\"mesh\":0}],"
                  "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1},"
                  "\"material\":0}]}],"
                  "\"materials\":[{\"pbrMetallicRoughness\":"
                  "{\"baseColorFactor\":[0.8,0.8,0.8,1.0]}}],"
                  "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
                  "\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
                  "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
                  "{\"bufferView\":2,\"componentType\":5126,\"count\":2,\"type\":\"SCALAR\","
                  "\"min\":[0],\"max\":[1]},"
                  "{\"bufferView\":3,\"componentType\":5126,\"count\":2,\"type\":\"VEC3\"}],");
  if (moving) {
    held +=
        "\"animations\":[{\"samplers\":[{\"input\":2,\"output\":3,\"interpolation\":\"LINEAR\"}],"
        "\"channels\":[{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}}]}],";
  }
  held +=
      "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
      "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36},"
      "{\"buffer\":1,\"byteOffset\":0,\"byteLength\":8},"
      "{\"buffer\":1,\"byteOffset\":8,\"byteLength\":24}],"
      "\"buffers\":[{\"byteLength\":72,\"uri\":\"data:application/octet-stream;base64,";
  held += kTriangleBase64;
  held += "\"},{\"byteLength\":32,\"uri\":\"data:application/octet-stream;base64,";
  held += kTrackBase64;
  held += "\"}]}";
  return held;
}

[[nodiscard]] bool Wrote(const std::string &path, const std::string &held) {
  SDL_IOStream *out = SDL_IOFromFile(path.c_str(), "wb");
  if (out == nullptr) { return false; }
  const bool wrote = SDL_WriteIO(out, held.data(), held.size()) == held.size();
  return SDL_CloseIO(out) && wrote;
}

[[nodiscard]] outshine::Scenario Falling(const char *uri) {
  outshine::Scenario stands;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  stands.Render.Fill = 0.6;
  stands.Render.Fps = 4.0;
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;
  stands.Lit.Key.ElevationDeg = 42.0;
  outshine::Asset shown;
  shown.Uri = uri;
  shown.Kind = "gltf";
  stands.Assets.push_back(shown);

  outshine::Body falls;
  falls.Name = "falls";
  falls.Asset = uri;
  falls.MassKg = 1000.0;
  falls.WidthM = 1.0;
  falls.AssetSpanM = 1.0;
  falls.InertiaKgM2[0] = falls.InertiaKgM2[1] = falls.InertiaKgM2[2] = 100.0;
  falls.Placed = true;
  stands.Bodies.push_back(falls);

  outshine::View watches;
  watches.Id = "watches";
  watches.Person = "third";
  watches.Placed = true;
  watches.Stands.AtM[2] = 4.0;
  watches.FovDeg = 60.0;
  stands.Views.push_back(watches);
  return stands;
}

[[nodiscard]] outshine::Scenario Naming(const char *uri) {
  outshine::Scenario stands;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  stands.Render.Fill = 0.6;
  stands.Render.Fps = 2.0;
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;
  stands.Lit.Key.ElevationDeg = 42.0;
  stands.Lit.Key.BearingDeg = 0.0;
  outshine::Asset shown;
  shown.Uri = uri;
  shown.Kind = "gltf";
  stands.Assets.push_back(shown);
  return stands;
}

[[nodiscard]] double MovingPixelsIn(const std::string &under, const outshine::Scenario &stands,
                                    int steps, std::string &why) {
  outshine::Engine engine;
  engine.Under(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.DrawsInto(outshine::Extent{kFramePx, kFramePx})) {
    why = "the device stood no canvas";
    return -1.0;
  }
  if (!engine.Declare(stands) || !engine.Assemble()) {
    why = engine.Error();
    return -1.0;
  }
  for (int step = 0; step < steps; ++step) {
    if (!engine.Advance() || !engine.RenderTo(outshine::Extent{})) {
      why = engine.Error();
      return -1.0;
    }
  }
  if (!engine.Inspects()) {
    why = engine.Error();
    return -1.0;
  }
  for (const outshine::Measure &held : engine.Numbers()) {
    if (held.What == "pixels the velocity target says moved") { return held.How; }
  }
  return -1.0;
}

[[nodiscard]] double MovingPixelsOver(const std::string &under, const char *uri, int steps,
                                      std::string &why) {
  outshine::Engine engine;
  engine.Under(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.DrawsInto(outshine::Extent{kFramePx, kFramePx})) {
    why = "the device stood no canvas";
    return -1.0;
  }
  if (!engine.Declare(Naming(uri)) || !engine.Assemble()) {
    why = engine.Error();
    return -1.0;
  }
  for (int step = 0; step < steps; ++step) {
    if (!engine.Advance() || !engine.RenderTo(outshine::Extent{})) {
      why = engine.Error();
      return -1.0;
    }
  }
  if (!engine.Inspects()) {
    why = engine.Error();
    return -1.0;
  }
  for (const outshine::Measure &held : engine.Numbers()) {
    if (held.What == "pixels the velocity target says moved") { return held.How; }
  }
  return -1.0;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case writes its subjects into the runner's nest and was given none");
    return Report();
  }
  const std::string under = nest;
  if (!Wrote(under + "/walks.gltf", Walking(true)) ||
      !Wrote(under + "/stands.gltf", Walking(false))) {
    Unprepared("the subjects could not be written into the nest");
    return Report();
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be drawn");
    return Report();
  }

  std::string why;
  const double still = MovingPixelsOver(under, "stands.gltf", 3, why);
  if (still < 0.0) {
    Unprepared(("the still subject would not stand: " + why).c_str());
    return Report();
  }
  const double walked = MovingPixelsOver(under, "walks.gltf", 3, why);
  if (walked < 0.0) {
    Unprepared(("the moving subject would not stand: " + why).c_str());
    return Report();
  }

  std::string fallWhy;
  const double fell = MovingPixelsIn(under, Falling("stands.gltf"), 3, fallWhy);

  std::printf("A SUBJECT THAT STANDS STILL   %.0f pixel(s) move\n", still);
  std::printf("ONE THAT MOVES                %.0f pixel(s) move\n", walked);
  std::printf("A BODY FALLING PAST A VIEW    %.0f pixel(s) move%s\n", fell,
              fell < 0.0 ? fallWhy.c_str() : "");

  // A PLACEMENT THAT MOVES WRITES A VELOCITY, and this is the only arrangement in the tree that
  // can say so. A node track moves VERTICES -- the engine bakes node transforms exactly as
  // `harness/shared/render/Parity.cpp` does -- so `walked` above is `prevP`'s work and would read
  // 118 with the placement row's previous half disabled. The only thing that moves a PLACEMENT is
  // a BODY, and until board:2000 a view could not stand still while one moved: `Carries` computed
  // the eye FROM the body it carried, so the relative motion was zero by construction.
  //
  // Now a view declares a station, a body falls past it, and the number is the body's own.
  CHECK(fell > 0.0,
        "**A PLACEMENT THAT MOVES WRITES A VELOCITY**: Unreal keeps a per-instance PREVIOUS "
        "transform in FGPUScene and RAGE double-buffers the same value, and both do it so TAA can "
        "reproject this frame's pixel to where it stood last frame. A renderer that reuses the "
        "CURRENT transform as the previous one sends a moving rigid subject to fetch its history "
        "from where it is NOT -- which reads as a TAA fault and is a missing row");

  CHECK(still == 0.0 && walked > 0.0,
        "**A SUBJECT THAT MOVES WRITES A VELOCITY AND A STILL ONE WRITES NONE**: the eye does not "
        "move in either run and the two declarations differ in exactly one thing, so every moving "
        "pixel here is the subject's own. Unreal's base pass writes velocity for the same reason "
        "and RAGE keeps the buffer for its own reprojection; a still subject that writes velocity "
        "hands TAA a history sample from the wrong place, and a moving one that writes none hands "
        "it the same fault in the other direction");

  Covers("the render: a placement that moves writes a velocity and one that does not writes none, "
         "measured over a FIXED eye so the number is the subject's motion and nothing else's");
  return Report();
}
