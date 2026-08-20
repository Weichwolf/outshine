/* WHAT THIS READER RESOLVES OF `KHR_animation_pointer` IS AN ENUMERATION AND NOT A CHAIN OF STRING
 * COMPARISONS (board:1392). The extension targets a JSON pointer into the whole asset object model,
 * which is every mutable property the format has -- so the honest statement is never *this reader
 * supports it* but *this reader resolves exactly these, and anything else is named*.
 *
 * A HIDDEN CHAIN ANSWERS THE WRONG QUESTION. `if (tail == "metallicFactor") ...` answers *does this
 * one pointer resolve*; it cannot answer *which pointers do*, so nothing could compare what is
 * honoured against what the extension defines, and the set would drift by addition with nobody able
 * to see it. `AnimatablePointers()` is walkable, so this test can close it in BOTH directions: no
 * `MaterialFactor` without a pointer that spells it, and no pointer naming a factor twice.
 *
 * THE ENUMERATOR SET IS DERIVED AND NOT RESTATED. A second list of `MaterialFactor` values here would
 * be exactly the drift this test exists against, so the walk asks `FactorComponents`, whose switch the
 * compiler already forces to cover the enumeration -- warnings are errors, so a new enumerator cannot
 * reach this test without going through it.
 *
 * AND A POINTER OUTSIDE THE SET IS NOT A REFUSAL. glTF 2.0 lets a client ignore animations outright,
 * so a channel this engine cannot drive is a shortfall of THIS engine and not a defect of the file --
 * counted, quoted, and separated into a grammar gap and a capability gap, which are answered by
 * different work. */
#include <cstdint>
#include <string>
#include <vector>

#include "Check.h"
#include "Glb.h"

#include "Document.h"
#include "Pose.h"
#include "Types.h"

using outshine::Span;
using outshine::Gltf::AnimatablePointer;
using outshine::Gltf::AnimatablePointers;
using outshine::Gltf::AnimationPath;
using outshine::Gltf::Document;
using outshine::Gltf::FactorComponents;
using outshine::Gltf::MaterialFactor;
using outshine::Gltf::Pose;
using outshine::Gltf::UndrivenReason;

namespace {

/* ONE SAMPLER SERVES EVERY CHANNEL, because what is under test is the TARGET and not the curve. The
 * output is a `VEC4` so the one channel that resolves -- `baseColorFactor` -- finds the four
 * components it must have; the channels that do not resolve never reach that check. */
const char *kJson = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_animation_pointer"],
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [{"name": "subject"}],
  "materials": [{"pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1]}}],
  "buffers": [{"byteLength": 40}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 8},
    {"buffer": 0, "byteOffset": 8, "byteLength": 32}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 2, "type": "SCALAR", "min": [0], "max": [1]},
    {"bufferView": 1, "componentType": 5126, "count": 2, "type": "VEC4"}
  ],
  "animations": [{
    "samplers": [{"input": 0, "interpolation": "LINEAR", "output": 1}],
    "channels": [
      {"sampler": 0, "target": {"path": "pointer", "extensions": {"KHR_animation_pointer":
        {"pointer": "/materials/0/pbrMetallicRoughness/baseColorFactor"}}}},
      {"sampler": 0, "target": {"path": "pointer", "extensions": {"KHR_animation_pointer":
        {"pointer": "/materials/0/extensions/KHR_materials_anisotropy/anisotropyStrength"}}}},
      {"sampler": 0, "target": {"path": "pointer", "extensions": {"KHR_animation_pointer":
        {"pointer": "/nodes/0/translation"}}}}
    ]
  }]
})";

std::vector<uint8_t> Binary() {
  std::vector<uint8_t> out;
  outshine::Test::Append(out, 0.f);
  outshine::Test::Append(out, 1.f);
  for (int at = 0; at < 8; ++at) { outshine::Test::Append(out, 0.5f); }
  return out;
}

} // namespace

int main() {
  using namespace outshine::Test;

  const Span<const AnimatablePointer> published = AnimatablePointers();
  CHECK(published.Size() > 0, "this reader publishes the set of pointers it resolves");
  Note("pointers this reader resolves", (double)published.Size(), "pointers");

  /* NO TAIL TWICE, or one spelling would silently take the other's factor. */
  size_t repeated = 0;
  for (size_t one = 0; one < published.Size(); ++one) {
    for (size_t two = one + 1; two < published.Size(); ++two) {
      if (std::string(published.Data()[one].Tail) == published.Data()[two].Tail) { ++repeated; }
    }
  }
  CHECK(repeated == 0, "no pointer is spelled twice in the published set");

  /* CLOSED IN BOTH DIRECTIONS. `FactorComponents` answers non-zero for exactly the enumerators the
   * format gives a keyframe width to, and its switch is what the compiler already holds complete. */
  size_t factors = 0, unspelled = 0, spelledTwice = 0;
  for (int raw = 0; raw < 256; ++raw) {
    const MaterialFactor factor = (MaterialFactor)raw;
    if (FactorComponents(factor) == 0) { continue; }
    ++factors;
    size_t spellings = 0;
    for (const AnimatablePointer &known : published) {
      if (known.Factor == factor) { ++spellings; }
    }
    if (spellings == 0) { ++unspelled; }
    if (spellings > 1) { ++spelledTwice; }
  }
  Note("material factors this reader holds", (double)factors, "factors");
  CHECK(unspelled == 0, "every material factor this reader holds is named by a published pointer");
  CHECK(spelledTwice == 0, "no material factor is named by two published pointers");
  CHECK(factors == published.Size(),
        "the published pointers and the factors they drive are one set and not two");

  /* EVERY PUBLISHED TAIL RESOLVES WHEN IT IS SPELLED, which is what says the table is the grammar the
   * parser actually walks rather than a second list beside it. */
  for (const AnimatablePointer &known : published) {
    std::string json = kJson;
    const std::string was = "/materials/0/pbrMetallicRoughness/baseColorFactor";
    json.replace(json.find(was), was.size(), std::string("/materials/0/") + known.Tail);
    const std::vector<uint8_t> glb = Glb(json, Binary());
    Document file;
    if (!file.Read(Span<const uint8_t>(glb.data(), glb.size()), "pointers.glb")) {
      /* A factor whose keyframe is not four wide cannot ride a `VEC4` sampler, and that refusal is
       * the reader's arithmetic rather than the table's: it proves the tail was RESOLVED, because an
       * unresolved one is counted and never refuses. */
      CHECK(FactorComponents(known.Factor) != 4,
            "a published pointer whose factor is four wide reads against a VEC4 sampler");
      continue;
    }
    CHECK(file.Animations().size() == 1 && !file.Animations()[0].Channels.empty(),
          "a published pointer arrives as a channel this reader holds");
    const auto &channel = file.Animations()[0].Channels[0];
    CHECK(channel.Path == AnimationPath::MaterialFactor && channel.Material == 0 &&
              channel.Factor == known.Factor,
          "and it names the material and the factor its own row declares");
  }

  const std::vector<uint8_t> glb = Glb(kJson, Binary());
  Document file;
  CHECK(file.Read(Span<const uint8_t>(glb.data(), glb.size()), "pointers.glb"),
        "a file whose only animation drives material factors is read and not refused");
  CHECK(file.Animations().size() == 1, "and it carries its one animation");

  const auto &animation = file.Animations()[0];
  CHECK(animation.Channels.size() == 1,
        "the one channel this reader resolves is held, and the two it does not are not");
  CHECK(animation.Undriven.size() == 2, "and both undriven channels are counted rather than dropped");
  Note("channels this reader could not drive", (double)animation.Undriven.size(), "channels");

  size_t unheld = 0, unparsed = 0;
  for (const auto &lost : animation.Undriven) {
    CHECK(!lost.Pointer.empty(), "an undriven channel quotes the pointer it could not resolve");
    std::printf("NOTE undriven %s (%s)\n", lost.Pointer.c_str(),
                lost.Why == UndrivenReason::PointerUnheld ? "parses, no field held" : "not parsed");
    if (lost.Why == UndrivenReason::PointerUnheld) { ++unheld; }
    if (lost.Why == UndrivenReason::PointerUnparsed) { ++unparsed; }
  }
  /* THE TWO REASONS ARE DIFFERENT WORK AND THE TEST SAYS SO. `anisotropyStrength` is a well-formed
   * pointer at a material this reader holds no field for -- a capability, waiting on `board:1390`.
   * `/nodes/0/translation` is a shape this reader does not parse at all -- a grammar. Reporting both
   * as *unsupported* would put one number on two questions. */
  CHECK(unheld == 1, "a pointer that parses and names a property no field holds says so");
  CHECK(unparsed == 1, "and a pointer this reader does not parse at all is a different answer");

  /* **CARRIED IS NOT DRIVEN, AND BOTH HALVES ARE SAID HERE.** `Pose` answers what the channel holds
   * at an instant; nothing in this engine reads that answer yet, so a material is not animated and
   * this test does not pretend one is. What it does hold is that the resolved channel SURVIVES into
   * the pose and samples -- a capability published with no consumer is otherwise a claim nobody has
   * ever run. The sampler's two keyframes are 0.5 throughout, so the value is the same at either
   * end and what is under test is that the factor arrives keyed by its own material. */
  Pose pose;
  std::string why;
  const int animations[] = {0};
  CHECK(Pose::Build(file, Span<const int>(animations, 1), pose, why),
        "a pose builds from an animation whose only driven channel is a material factor");
  std::vector<Pose::FactorAt> driven;
  pose.FactorsAt(0.5, driven);
  CHECK(driven.size() == 1, "and it answers the one factor that animation drives");
  if (driven.size() == 1) {
    CHECK(driven[0].Material == 0 && driven[0].Factor == MaterialFactor::BaseColour,
          "keyed by the material and the factor the pointer named");
    size_t held = 0;
    for (size_t at = 0; at < FactorComponents(driven[0].Factor); ++at) {
      if (driven[0].Values[at] == 0.5) { ++held; }
    }
    CHECK(held == FactorComponents(driven[0].Factor),
          "carrying the factor's own width of values off its own sampler");
  }
  std::vector<outshine::Gltf::Transform> locals;
  std::vector<double> weights;
  pose.At(0.5, locals, weights);
  CHECK(locals.size() == 1, "and the node arm is untouched by it, because a factor is not a pose");

  Covers("I.26.11 KHR_animation_pointer: the set of pointers this reader resolves is published and "
         "closed against the factors it holds, and a pointer outside it is counted and quoted with "
         "the reason it could not be driven");
  return Report();
}
