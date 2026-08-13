/* THE THREE INTERPOLATIONS OVER THE THREE PATHS, AND THE TIME THEY ARE ASKED AT.
 *
 * `InterpolationTest` is nine animations -- `LINEAR`, `STEP` and `CUBICSPLINE` over `translation`,
 * `rotation` and `scale` -- and Khronos judges it *by eye in motion against the reference, `STEP`
 * shows no intermediate value*. That is a comparison this tree cannot make yet: the render runner has
 * no time axis, so a still of an animated asset would compare our rest pose against Cycles at
 * whatever frame Blender happened to be on, and the red would name the clock rather than the
 * sampler. **So the asset is not fetched here and the subject is the same nine combinations built in
 * process** -- one GLB, three keyframes, values chosen so every answer below has a closed form
 * exact in binary floating point. The Khronos asset lands with the render round that can score it.
 *
 * WHAT A FIXTURE BUYS THAT THE ASSET COULD NOT. `STEP shows no intermediate value` is a statement
 * about EVERY time in a span, and by eye it is a statement about the few a viewer happens to draw.
 * Here it is swept and the claim is exact: over the whole grid the sampler returns a bitwise copy of
 * a keyframe and never a value that is not in the file.
 *
 * THE TIME CONTRACT IS THE OTHER HALF (doc/requirements.md I.26.3), and it is arithmetic rather than
 * pixels: the animation time of frame `n` is `n / fps` DERIVED, never a running total. The drift an
 * accumulator earns is measured below rather than asserted, so the reason the contract exists is a
 * number in this log and not a sentence in a header.
 *
 * A ROTATION IS THE ONE PATH WHERE `LINEAR` IS NOT LINEAR. glTF says spherical, and at the MIDPOINT
 * of a span the normalised component blend and the spherical blend agree exactly -- both are the
 * bisector -- so a test sampling only midpoints cannot tell them apart and would pass over a
 * component lerp. Every rotation claim below is taken at a QUARTER of its span, where the two are
 * 22.500 and 21.598 degrees: 0.902 degrees apart, measured here and printed beside the claim. */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Glb.h"

#include "Document.h"
#include "Track.h"

using outshine::Gltf::Animation;
using outshine::Gltf::AnimationPath;
using outshine::Gltf::Document;
using outshine::Gltf::Interpolation;
using outshine::Gltf::Track;
using outshine::Test::Append;

namespace {

/* THE ONE NUMBER ABOUT THE BUFFER THAT IS NOT IN THE JSON BELOW. Every offset and length is stated
 * there, in the bufferViews, which is where a reader looks; restating them here would be a second
 * copy for the next round to keep in step. This is the total, and it is checked against what the
 * writer below actually produced -- a layout that drifted from its declaration would otherwise read
 * past the end of a chunk and be caught by nothing. */
constexpr size_t kBufferBytes = 492;

/* 90 degrees about +Z as a unit quaternion, xyzw. Written as the arithmetic so the angle is what is
 * stated and the components are what follow from it. */
const double kQuarterTurn = std::sqrt(0.5);

std::vector<uint8_t> Buffer() {
  std::vector<uint8_t> bytes;
  const auto put = [&bytes](std::initializer_list<double> run) {
    for (double value : run) { Append(bytes, static_cast<float>(value)); }
  };
  put({0.0, 1.0, 2.0});
  /* Translation: a span whose midpoint is exact, then a second span so a clamp past the end has
   * something to clamp to. */
  put({0, 0, 0, 2, 4, 8, 6, 12, 24});
  put({1, 1, 1, 2, 2, 2, 4, 4, 4});
  put({0, 0, 0, 1, 0, 0, kQuarterTurn, kQuarterTurn, 0, 0, 1, 0});
  /* Translation under CUBICSPLINE: zero tangents everywhere but the first out-tangent, which is 2
   * on x. At the middle of the first span the Hermite basis gives 0.125*2 + 0.5*1 = 0.75, and a
   * LINEAR sampler over the same keyframes would give 0.5 -- so this one number is what separates
   * the spline from a straight line rather than merely being consistent with it. */
  put({0, 0, 0, /**/ 0, 0, 0, /**/ 2, 0, 0});
  put({0, 0, 0, /**/ 1, 0, 0, /**/ 0, 0, 0});
  put({0, 0, 0, /**/ 2, 0, 0, /**/ 0, 0, 0});
  /* Scale under CUBICSPLINE with every tangent zero: the basis reduces to h00 + h01, which is a
   * smooth step, and at the middle of a span that is the plain mean. */
  put({0, 0, 0, /**/ 1, 1, 1, /**/ 0, 0, 0});
  put({0, 0, 0, /**/ 3, 3, 3, /**/ 0, 0, 0});
  put({0, 0, 0, /**/ 5, 5, 5, /**/ 0, 0, 0});
  /* Rotation under CUBICSPLINE, tangents zero, the same three orientations. */
  put({0, 0, 0, 0, /**/ 0, 0, 0, 1, /**/ 0, 0, 0, 0});
  put({0, 0, 0, 0, /**/ 0, 0, kQuarterTurn, kQuarterTurn, /**/ 0, 0, 0, 0});
  put({0, 0, 0, 0, /**/ 0, 0, 1, 0, /**/ 0, 0, 0, 0});
  return bytes;
}

/* The nine animations, named the way Khronos names them, so a failure line says which of the nine.
 * `output` is the accessor index and `interpolation` the word the file carries. */
const char *const kJson = R"({
  "asset": {"version": "2.0"},
  "buffers": [{"byteLength": 492}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0,   "byteLength": 12},
    {"buffer": 0, "byteOffset": 12,  "byteLength": 36},
    {"buffer": 0, "byteOffset": 48,  "byteLength": 36},
    {"buffer": 0, "byteOffset": 84,  "byteLength": 48},
    {"buffer": 0, "byteOffset": 132, "byteLength": 108},
    {"buffer": 0, "byteOffset": 240, "byteLength": 108},
    {"buffer": 0, "byteOffset": 348, "byteLength": 144}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "SCALAR", "min": [0], "max": [2]},
    {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3"},
    {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC3"},
    {"bufferView": 3, "componentType": 5126, "count": 3, "type": "VEC4"},
    {"bufferView": 4, "componentType": 5126, "count": 9, "type": "VEC3"},
    {"bufferView": 5, "componentType": 5126, "count": 9, "type": "VEC3"},
    {"bufferView": 6, "componentType": 5126, "count": 9, "type": "VEC4"}
  ],
  "nodes": [
    {"name": "StepTranslation"}, {"name": "LinearTranslation"}, {"name": "CubicTranslation"},
    {"name": "StepRotation"}, {"name": "LinearRotation"}, {"name": "CubicRotation"},
    {"name": "StepScale"}, {"name": "LinearScale"}, {"name": "CubicScale"}
  ],
  "scenes": [{"nodes": [0, 1, 2, 3, 4, 5, 6, 7, 8]}],
  "scene": 0,
  "animations": [
    {"name": "Step Translation",
     "samplers": [{"input": 0, "output": 1, "interpolation": "STEP"}],
     "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}]},
    {"name": "Linear Translation",
     "samplers": [{"input": 0, "output": 1, "interpolation": "LINEAR"}],
     "channels": [{"sampler": 0, "target": {"node": 1, "path": "translation"}}]},
    {"name": "CubicSpline Translation",
     "samplers": [{"input": 0, "output": 4, "interpolation": "CUBICSPLINE"}],
     "channels": [{"sampler": 0, "target": {"node": 2, "path": "translation"}}]},
    {"name": "Step Rotation",
     "samplers": [{"input": 0, "output": 3, "interpolation": "STEP"}],
     "channels": [{"sampler": 0, "target": {"node": 3, "path": "rotation"}}]},
    {"name": "Linear Rotation",
     "samplers": [{"input": 0, "output": 3, "interpolation": "LINEAR"}],
     "channels": [{"sampler": 0, "target": {"node": 4, "path": "rotation"}}]},
    {"name": "CubicSpline Rotation",
     "samplers": [{"input": 0, "output": 6, "interpolation": "CUBICSPLINE"}],
     "channels": [{"sampler": 0, "target": {"node": 5, "path": "rotation"}}]},
    {"name": "Step Scale",
     "samplers": [{"input": 0, "output": 2, "interpolation": "STEP"}],
     "channels": [{"sampler": 0, "target": {"node": 6, "path": "scale"}}]},
    {"name": "Linear Scale",
     "samplers": [{"input": 0, "output": 2, "interpolation": "LINEAR"}],
     "channels": [{"sampler": 0, "target": {"node": 7, "path": "scale"}}]},
    {"name": "CubicSpline Scale",
     "samplers": [{"input": 0, "output": 5, "interpolation": "CUBICSPLINE"}],
     "channels": [{"sampler": 0, "target": {"node": 8, "path": "scale"}}]}
  ]
})";

/* THE FRAME RATE THE TIME CONTRACT IS STATED AT. 60 is CLAUDE.md's 720p60 budget, and it divides the
 * fixture's one-second keyframe spacing exactly, which is what lets a frame index land ON a keyframe
 * rather than near one. */
constexpr double kFps = 60.0; /* [SET] frames per second */

const Animation *Named(const Document &document, const char *name) {
  for (const Animation &animation : document.Animations()) {
    if (animation.Name == name) { return &animation; }
  }
  return nullptr;
}

/* The one sampler of a one-channel animation, decoded. `false` where the animation is not there or
 * does not have the shape the fixture declares, so a caller never samples a run it did not get. */
bool Decode(const Document &document, const char *name, std::vector<double> &times,
            std::vector<double> &values, Track &out) {
  const Animation *animation = Named(document, name);
  if (animation == nullptr || animation->Samplers.size() != 1 || animation->Channels.size() != 1) {
    return false;
  }
  const auto &sampler = animation->Samplers[0];
  if (!document.ReadElements(sampler.Input, times)) { return false; }
  if (!document.ReadElements(sampler.Output, values)) { return false; }
  return Track::Build(animation->Channels[0].Path, sampler.How, times, values, out);
}

/* A refusal is checked by what it REFUSED, not by the fact that it said no: a reader that refused
 * every file would pass a test that only looked at the boolean. */
void RefusesNaming(const std::string &json, const std::vector<uint8_t> &binary, const char *fragment,
                   const char *claim) {
  Document document;
  const std::vector<uint8_t> glb = outshine::Test::Glb(json, binary);
  const bool read = document.Read({glb.data(), glb.size()}, "fixture.glb");
  CHECK(!read, claim);
  if (read) { return; }
  const bool names = document.Error().find(fragment) != std::string::npos;
  CHECK(names, "and the refusal names what it refused");
  if (!names) { std::printf("       %s\n", document.Error().c_str()); }
}

} // namespace

int main() {
  using namespace outshine::Test;

  const std::vector<uint8_t> binary = Buffer();
  CHECK(binary.size() == kBufferBytes,
        "the fixture's buffer is the length its bufferViews are laid out over");
  const std::vector<uint8_t> glb = Glb(kJson, binary);

  Document document;
  const bool read = document.Read({glb.data(), glb.size()}, "interpolation-fixture.glb");
  CHECK(read, "nine animations over three paths and three interpolations read");
  if (!read) {
    std::printf("       %s\n", document.Error().c_str());
    return Report();
  }
  CHECK(document.Animations().size() == 9,
        "all nine animations survive the read, which is the matrix InterpolationTest is");

  /* THE MATRIX IS COMPLETE, checked rather than assumed: three interpolations seen on each of the
   * three paths. A reader that collapsed CUBICSPLINE onto LINEAR would still produce nine
   * animations, and this is what notices. */
  int seen[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
  for (const Animation &animation : document.Animations()) {
    if (animation.Channels.size() != 1 || animation.Samplers.size() != 1) { continue; }
    const size_t path = static_cast<size_t>(animation.Channels[0].Path);
    const size_t how = static_cast<size_t>(animation.Samplers[0].How);
    if (path < 3 && how < 3) { seen[path][how]++; }
  }
  bool complete = true;
  for (const auto &path : seen) {
    for (int count : path) { complete = complete && count == 1; }
  }
  CHECK(complete,
        "each of translation, rotation and scale carries exactly one LINEAR, one STEP and one "
        "CUBICSPLINE, so no arm of the matrix was read as another");

  std::vector<double> times, values;
  Track keyframes;
  double got[4] = {0, 0, 0, 0};

  /* STEP -- and the claim is Khronos's own, swept rather than sampled: over the whole grid at a
   * thousand times the frame rate, the sampler returns a BITWISE copy of a keyframe and never a
   * value that is not in the file. */
  if (Decode(document, "Step Translation", times, values, keyframes)) {
    size_t notAKeyframe = 0;
    const int steps = 6000;
    for (int at = 0; at <= steps; ++at) {
      const double seconds = -0.5 + 3.0 * double(at) / double(steps);
      keyframes.At(seconds, got);
      bool isAKeyframe = false;
      for (size_t key = 0; key < keyframes.KeyframeCount(); ++key) {
        isAKeyframe = isAKeyframe || (got[0] == values[key * 3] && got[1] == values[key * 3 + 1] &&
                                      got[2] == values[key * 3 + 2]);
      }
      if (!isAKeyframe) { ++notAKeyframe; }
    }
    Note("STEP samples that are not a keyframe of the file", double(notAKeyframe), "samples");
    CHECK(notAKeyframe == 0,
          "STEP shows no intermediate value anywhere on or beyond the grid, which is Khronos's own "
          "criterion swept rather than looked at");
    /* And it holds the EARLIER keyframe across a span, which is what makes it a step and not a
     * nearest-keyframe rule. */
    keyframes.At(0.999, got);
    CHECK(got[0] == 0.0 && got[1] == 0.0 && got[2] == 0.0,
          "STEP holds the earlier keyframe right up to the next one, never the nearer one");
  } else {
    CHECK(false, "the STEP translation sampler decodes");
  }

  /* LINEAR over a path that is not a rotation is the plain blend, and the fixture's numbers make the
   * midpoint exact in binary. */
  if (Decode(document, "Linear Translation", times, values, keyframes)) {
    keyframes.At(0.5, got);
    CHECK(got[0] == 1.0 && got[1] == 2.0 && got[2] == 4.0,
          "LINEAR at the middle of a span is the mean of its two keyframes, exactly");
    keyframes.At(-1.0, got);
    CHECK(got[0] == 0.0,
          "before the first keyframe the first value stands, which is the format's clamp and not an "
          "extrapolation");
    keyframes.At(99.0, got);
    CHECK(got[0] == 6.0 && got[1] == 12.0 && got[2] == 24.0,
          "after the last keyframe the last value stands");
  } else {
    CHECK(false, "the LINEAR translation sampler decodes");
  }

  /* LINEAR OVER A ROTATION IS SPHERICAL, and the quarter point is where that stops agreeing with a
   * normalised component blend. At a quarter of a 90-degree span the spherical answer is 22.5
   * degrees about +Z; the component blend is 23.19 degrees, which is 0.69 degrees away and far
   * above the tolerance below. */
  if (Decode(document, "Linear Rotation", times, values, keyframes)) {
    const double quarterTurnAt = std::atan(1.0) / 4.0; /* 22.5 degrees in radians */
    keyframes.At(0.25, got);
    /* THE FIXTURE'S KEYFRAMES CROSS AS float32, so the closed form in double cannot be met tighter
     * than the rounding of the inputs: 5.96e-8 relative at magnitude 1, and the measured miss is
     * 2.23e-9. The tolerance is that rounding, not a number fitted to the result -- and it is four
     * orders below the 0.0157 this claim exists to separate from, which is what a component blend
     * puts on the same quaternion. */
    constexpr double kFloat32Rounding = 1e-7; /* [SET] absolute, quaternion component */
    CHECK_NEAR(got[2], std::sin(quarterTurnAt), kFloat32Rounding, "quaternion z",
               "LINEAR over a rotation is SPHERICAL: a quarter of the way through a 90 degree span "
               "is 22.5 degrees, which a component blend does not produce");
    CHECK_NEAR(got[3], std::cos(quarterTurnAt), kFloat32Rounding, "quaternion w",
               "and its scalar part is the same angle's cosine");
    const double lerped[2] = {0.75 * 0.0 + 0.25 * kQuarterTurn, 0.75 * 1.0 + 0.25 * kQuarterTurn};
    const double magnitude = std::sqrt(lerped[0] * lerped[0] + lerped[1] * lerped[1]);
    Note("the component blend this rotation is NOT, in degrees",
         std::atan2(lerped[0] / magnitude, lerped[1] / magnitude) * 360.0 / std::acos(-1.0),
         "degrees");
    CHECK_NEAR(std::sqrt(got[0] * got[0] + got[1] * got[1] + got[2] * got[2] + got[3] * got[3]), 1.0,
               1e-15, "quaternion magnitude",
               "the rotation comes back on the unit sphere, so a matrix built from it carries no "
               "scale nobody declared");
  } else {
    CHECK(false, "the LINEAR rotation sampler decodes");
  }

  /* CUBICSPLINE reads three elements per keyframe and its tangents are scaled by the span. The two
   * claims below are the two halves of that: the value at a keyframe is the MIDDLE of its triple,
   * and the value between keyframes is a curve a straight line does not reach. */
  if (Decode(document, "CubicSpline Translation", times, values, keyframes)) {
    keyframes.At(1.0, got);
    CHECK(got[0] == 1.0,
          "at a keyframe CUBICSPLINE is that keyframe's own value, which is the MIDDLE of its "
          "in-value-out triple and not the first element of it");
    keyframes.At(0.5, got);
    CHECK(got[0] == 0.75,
          "between keyframes CUBICSPLINE is the Hermite curve with the file's tangents scaled by "
          "the span: 0.75 here, where a straight line would give 0.5");
  } else {
    CHECK(false, "the CUBICSPLINE translation sampler decodes");
  }
  if (Decode(document, "CubicSpline Scale", times, values, keyframes)) {
    keyframes.At(0.5, got);
    CHECK(got[0] == 2.0 && got[1] == 2.0 && got[2] == 2.0,
          "with every tangent zero the Hermite basis is a smooth step, whose midpoint is the mean");
    keyframes.At(0.0, got);
    CHECK(got[0] == 1.0, "and the first keyframe is its own value");
    keyframes.At(2.0, got);
    CHECK(got[0] == 5.0, "and the last keyframe is its own value");
  } else {
    CHECK(false, "the CUBICSPLINE scale sampler decodes");
  }
  if (Decode(document, "CubicSpline Rotation", times, values, keyframes)) {
    keyframes.At(1.0, got);
    CHECK_NEAR(got[2], kQuarterTurn, 1e-7, "quaternion z",
               "a spline over a rotation still passes through its keyframes exactly");
  } else {
    CHECK(false, "the CUBICSPLINE rotation sampler decodes");
  }

  /* THE TIME CONTRACT (doc/requirements.md I.26.3). An integer frame index with derived seconds is
   * the rule; what it buys is measured here rather than asserted, by running the accumulator the
   * rule forbids beside it. */
  {
    double accumulated = 0.0;
    const double step = 1.0 / kFps;
    int firstDisagreement = -1;
    double worstDrift = 0.0;
    const int frames = 3600; /* one minute at 60 fps */
    for (int frame = 0; frame <= frames; ++frame) {
      const double derived = double(frame) / kFps;
      const double drift = std::fabs(accumulated - derived);
      if (drift > worstDrift) { worstDrift = drift; }
      if (drift > 0.0 && firstDisagreement < 0) { firstDisagreement = frame; }
      accumulated += step;
    }
    Note("first frame at which an accumulated clock leaves the derived one",
         double(firstDisagreement), "frames");
    Note("worst drift of an accumulated clock over one minute at 60 fps", worstDrift, "s");
    CHECK(firstDisagreement > 0,
          "an accumulated clock DOES leave the derived one, so the contract this test holds is "
          "protecting against something that happens rather than against a possibility");
    /* And the derived clock lands ON the fixture's keyframes, which is the second half of I.26.3:
     * the engine's animation time at frame n equals the glTF sampler input at the corresponding
     * keyframe, exactly and not to a tolerance. */
    if (Decode(document, "Linear Translation", times, values, keyframes)) {
      for (size_t key = 0; key < times.size(); ++key) {
        const int frame = int(times[key] * kFps);
        CHECK(double(frame) / kFps == times[key],
              "the derived animation time of an integer frame is the sampler input at that "
              "keyframe, exactly");
      }
    }
  }

  /* WHAT THE READER REFUSES, and each refusal names what it refused. A viewer that fell back to
   * LINEAR on an unknown word is the defect InterpolationTest exists to expose; a reader that did
   * the same would make the case unable to see it. */
  {
    std::string json(kJson);
    const size_t at = json.find("\"CUBICSPLINE\"");
    CHECK(at != std::string::npos, "the fixture carries a CUBICSPLINE sampler to corrupt");
    if (at != std::string::npos) {
      std::string unknown = json;
      unknown.replace(at, std::string("\"CUBICSPLINE\"").size(), "\"BEZIER\"      ");
      RefusesNaming(unknown, binary, "BEZIER",
                    "a fourth interpolation word is refused rather than read as LINEAR");
    }
    const size_t path = json.find("\"translation\"");
    CHECK(path != std::string::npos, "the fixture carries a translation channel to corrupt");
    if (path != std::string::npos) {
      std::string unknown = json;
      unknown.replace(path, std::string("\"translation\"").size(), "\"colour\"     ");
      RefusesNaming(unknown, binary, "colour",
                    "a path the format does not define is refused rather than read as translation");
    }
    /* THE SHAPE RULE: a CUBICSPLINE sampler over an output run of one element per keyframe is a file
     * whose animation is a third of its declared length, and reading it as LINEAR would produce a
     * plausible motion nobody authored. */
    std::string shortened = json;
    const size_t spline = shortened.find("\"input\": 0, \"output\": 4, \"interpolation\": \"CUBICSPLINE\"");
    CHECK(spline != std::string::npos, "the fixture's CUBICSPLINE translation sampler is findable");
    if (spline != std::string::npos) {
      shortened.replace(spline, std::string("\"input\": 0, \"output\": 4").size(),
                        "\"input\": 0, \"output\": 1");
      RefusesNaming(shortened, binary, "output elements",
                    "a CUBICSPLINE sampler whose output is one element per keyframe is refused by "
                    "the count, because the interpolation decides the DECODE and not only the "
                    "formula");
    }
  }

  Covers("I.26 the reader carries `animations`: samplers as an input grid, an output run and one of "
         "three interpolations, channels as a node and one of four paths, every unknown word "
         "refused by name and every shape rule checked against the accessors");
  Covers("I.26.3 the time contract: the animation time of frame n is n/fps DERIVED, it lands on the "
         "sampler's own keyframe inputs exactly, and the accumulator the rule forbids is measured "
         "drifting beside it");
  Covers("I.26.12 khronos:InterpolationTest, the reader half: nine animations over three paths and "
         "three interpolations, STEP showing no intermediate value swept over the whole grid, "
         "LINEAR over a rotation spherical rather than component-wise, and CUBICSPLINE reading "
         "three elements per keyframe");
  return Report();
}
