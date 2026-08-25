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

constexpr size_t kBufferBytes = 492;

const double kQuarterTurn = std::sqrt(0.5);

std::vector<uint8_t> Buffer() {
  std::vector<uint8_t> bytes;
  const auto put = [&bytes](std::initializer_list<double> run) {
    for (double value : run) { Append(bytes, static_cast<float>(value)); }
  };
  put({0.0, 1.0, 2.0});

  put({0, 0, 0, 2, 4, 8, 6, 12, 24});
  put({1, 1, 1, 2, 2, 2, 4, 4, 4});
  put({0, 0, 0, 1, 0, 0, kQuarterTurn, kQuarterTurn, 0, 0, 1, 0});

  put({0, 0, 0,  0, 0, 0,  2, 0, 0});
  put({0, 0, 0,  1, 0, 0,  0, 0, 0});
  put({0, 0, 0,  2, 0, 0,  0, 0, 0});

  put({0, 0, 0,  1, 1, 1,  0, 0, 0});
  put({0, 0, 0,  3, 3, 3,  0, 0, 0});
  put({0, 0, 0,  5, 5, 5,  0, 0, 0});

  put({0, 0, 0, 0,  0, 0, 0, 1,  0, 0, 0, 0});
  put({0, 0, 0, 0,  0, 0, kQuarterTurn, kQuarterTurn,  0, 0, 0, 0});
  put({0, 0, 0, 0,  0, 0, 1, 0,  0, 0, 0, 0});
  return bytes;
}

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

constexpr double kFps = 60.0;

const Animation *Named(const Document &document, const char *name) {
  for (const Animation &animation : document.Animations()) {
    if (animation.Name == name) { return &animation; }
  }
  return nullptr;
}

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

}

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

    keyframes.At(0.999, got);
    CHECK(got[0] == 0.0 && got[1] == 0.0 && got[2] == 0.0,
          "STEP holds the earlier keyframe right up to the next one, never the nearer one");
  } else {
    CHECK(false, "the STEP translation sampler decodes");
  }

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

  if (Decode(document, "Linear Rotation", times, values, keyframes)) {
    const double quarterTurnAt = std::atan(1.0) / 4.0;
    keyframes.At(0.25, got);

    constexpr double kFloat32Rounding = 1e-7;
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

  {
    double accumulated = 0.0;
    const double step = 1.0 / kFps;
    int firstDisagreement = -1;
    double worstDrift = 0.0;
    const int frames = 3600;
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

    if (Decode(document, "Linear Translation", times, values, keyframes)) {
      for (size_t key = 0; key < times.size(); ++key) {
        const int frame = int(times[key] * kFps);
        CHECK(double(frame) / kFps == times[key],
              "the derived animation time of an integer frame is the sampler input at that "
              "keyframe, exactly");
      }
    }
  }

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

  Covers("I.57 the reader carries `animations`: samplers as an input grid, an output run and one of "
         "three interpolations, channels as a node and one of four paths, every unknown word "
         "refused by name and every shape rule checked against the accessors");
  Covers("I.26.3 the time contract: the animation time of frame n is n/fps DERIVED, it lands on the "
         "sampler's own keyframe inputs exactly, and the accumulator the rule forbids is measured "
         "drifting beside it");
  Covers("I.65 khronos:InterpolationTest, the reader half: nine animations over three paths and "
         "three interpolations, STEP showing no intermediate value swept over the whole grid, "
         "LINEAR over a rotation spherical rather than component-wise, and CUBICSPLINE reading "
         "three elements per keyframe");
  return Report();
}
