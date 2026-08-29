#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

// EVERY DECLARED BODY IS DRAWN, AND ONE MESH DRAWN N TIMES IS ONE CALL WITH N INSTANCES.
//
// Unreal keeps one FPrimitiveSceneProxy per primitive and the TRANSFORM per instance in FGPUScene,
// so N copies of a mesh are one draw over an instance run. RAGE puts every entity on its node's
// draw list and the same geometry is submitted once with a matrix per entity. Both agree, and
// neither has a first-body case.
//
// This tree had one. Engine::State::Draws carried Ticking.Freestanding.front() -- the FIRST
// freestanding body and no other -- while Falls() integrated all of them. A scenario declaring
// sixteen bodies ran sixteen in the simulation and drew one, and nothing refused: the other
// fifteen were simply absent, which is the quietest kind of wrong.
//
// WHAT THE NUMBERS HERE MEAN, because a draw COUNT alone cannot tell the two apart. One draw is
// what a correct engine reports for sixteen instances AND what a broken one reports for one body,
// so the count is read beside the TRIANGLES and the PLACEMENT ROWS:
//
//     bodies   draws   triangles   placements
//          1       1           1            1
//         16       1          16           16
//
// The draw count holding at one while the triangles multiply is the instancing; the placement
// rows are what the bodies moved. A reader that only counted calls would call both of those rows
// the same picture.

namespace {

constexpr int kFramePx = 96;

// 3 positions VEC3 then 3 normals VEC3, 72 bytes -- one lit triangle and nothing else, so the
// triangle count this case multiplies is a number it can state: ONE.
constexpr const char *kTriangleBase64 =
    "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/"
    "AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/";

[[nodiscard]] std::string OneTriangle(void) {
  return std::string("{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
                     "\"nodes\":[{\"mesh\":0}],"
                     "\"meshes\":[{\"primitives\":[{\"attributes\":"
                     "{\"POSITION\":0,\"NORMAL\":1},\"material\":0}]}],"
                     "\"materials\":[{\"pbrMetallicRoughness\":"
                     "{\"baseColorFactor\":[0.8,0.8,0.8,1.0]}}],"
                     "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
                     "\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
                     "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
                     "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
                     "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36}],"
                     "\"buffers\":[{\"byteLength\":72,\"uri\":"
                     "\"data:application/octet-stream;base64,") +
         kTriangleBase64 + "\"}]}";
}

[[nodiscard]] bool Wrote(const std::string &path, const std::string &held) {
  SDL_IOStream *out = SDL_IOFromFile(path.c_str(), "wb");
  if (out == nullptr) { return false; }
  const bool wrote = SDL_WriteIO(out, held.data(), held.size()) == held.size();
  return SDL_CloseIO(out) && wrote;
}

[[nodiscard]] double Measured(const outshine::Engine &engine, const char *what) {
  for (const outshine::Measure &held : engine.measures()) {
    if (held.What == what) { return held.How; }
  }
  return -1.0;
}

struct Drew {
  double Draws = -1.0;
  double Triangles = -1.0;
  double Placements = -1.0;
  double Differ = -1.0;
};

[[nodiscard]] outshine::Scenario Standing(const std::string &uri, int bodies) {
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

  for (int one = 0; one < bodies; ++one) {
    outshine::Body falls;
    falls.Name = "body" + std::to_string(one);
    falls.Asset = uri;
    falls.Placed = true;
    falls.MassKg = 1000.0;
    falls.WidthM = 1.0;
    falls.Stands.AtM[0] = (double)one * 3.0;
    falls.Stands.AtM[1] = 20.0 + (double)one;
    stands.Bodies.push_back(falls);
  }

  outshine::View watches;
  watches.Id = "station";
  watches.Sees.Placed = true;
  watches.Person = "first";
  watches.Sees.Stands.AtM[0] = 20.0;
  watches.Sees.Stands.AtM[1] = 12.0;
  watches.Sees.Stands.AtM[2] = 60.0;
  watches.Sees.FovDeg = 60.0;
  stands.Views.push_back(watches);
  return stands;
}

[[nodiscard]] bool Ran(const std::string &under, const std::string &uri, int bodies, Drew &out,
                       std::string &why) {
  outshine::Engine engine;
  engine.setRoots(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.drawsInto(outshine::Extent{kFramePx, kFramePx})) {
    why = "the device stood no canvas";
    return false;
  }
  if (!engine.declare(Standing(uri, bodies)) || !engine.assemble()) {
    why = engine.error();
    return false;
  }
  for (int step = 0; step < 6; ++step) {
    if (!engine.advance() || !engine.renderer().render(outshine::Extent{})) {
      why = engine.error();
      return false;
    }
  }
  out.Draws = Measured(engine, "subjects, drew");
  out.Triangles = Measured(engine, "subjects, triangles");
  out.Placements = Measured(engine, "subjects, placements");
  out.Differ = Measured(engine, "subjects, placements that differ");
  const double standing = Measured(engine, "bodies standing on no route");
  if (standing != (double)bodies) {
    why = "the simulation integrates " + std::to_string(standing) + " bodies and " +
          std::to_string(bodies) + " were declared";
    return false;
  }
  return true;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case needs the runner's nest and was given none");
    return Report();
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be drawn");
    return Report();
  }

  const std::string under = nest;
  const std::string uri = "one-triangle.gltf";
  if (!Wrote(under + "/" + uri, OneTriangle())) {
    Unprepared("the subject could not be written into the nest");
    return Report();
  }
  std::string why;
  Drew one, many;
  const bool stood = Ran(under, uri, 1, one, why) && Ran(under, uri, 16, many, why);
  if (!stood) { std::printf("a declaration did not stand: %s\n", why.c_str()); }

  std::printf("one body: %.0f draw(s) %.0f tri %.0f placement(s) %.0f differ\n", one.Draws,
              one.Triangles, one.Placements, one.Differ);
  std::printf("sixteen:  %.0f draw(s) %.0f tri %.0f placement(s) %.0f differ\n", many.Draws,
              many.Triangles, many.Placements, many.Differ);

  CHECK(stood && many.Placements == 16.0,
        "**EVERY DECLARED BODY REACHES THE PICTURE**: the simulation integrated sixteen and the "
        "renderer held one placement row for one of them, so fifteen were absent from a frame "
        "that looked finished. Unreal holds a proxy per primitive and RAGE puts every entity on "
        "its node's draw list; neither has anywhere for the second body to be dropped from");

  CHECK(stood && many.Differ == 16.0,
        "**AND EACH ONE STANDS SOMEWHERE OF ITS OWN**: sixteen rows that all held the same matrix "
        "would draw sixteen bodies in one place, which is a different fault wearing the same "
        "count. The bodies are declared three metres apart and fall from different heights, so "
        "sixteen DISTINCT rows is what carrying them separately means");

  CHECK(stood && many.Draws == one.Draws && many.Triangles == one.Triangles * 16.0,
        "**ONE MESH DRAWN SIXTEEN TIMES IS ONE CALL WITH SIXTEEN INSTANCES**: the draw count holds "
        "while the triangles multiply, which is FGPUScene's shape -- geometry submitted once, a "
        "transform per instance. Sixteen calls for sixteen copies of one mesh is the CPU cost "
        "board:1943 exists to remove, and duplicating the geometry per body would be a third "
        "answer neither engine gives");

  Covers("the door: every declared freestanding body is carried into the picture with a placement "
         "row of its own, and N copies of one mesh cost ONE draw call with N instances");
  return Report();
}
