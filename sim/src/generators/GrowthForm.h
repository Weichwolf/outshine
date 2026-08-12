/* WHAT SHAPE A PLANT IS AT ALL, before any of its numbers. A form says how many leaders leave the
 * base, whether there is a clear bole under the crown, and what silhouette the crown is allowed to
 * fill; everything the grower already does — taper, branch chance and angle, order length, wander,
 * leader bias, whorls, shade prune — is a parameter WITHIN a form and never the definition of one. */
#ifndef GROWTHFORM_H
#define GROWTHFORM_H

#include <cstdint>
#include <optional>

namespace outshine::Generators {

/* The base plan: how many axes leave the ground, and whether the thing is alive and standing. */
enum class Architecture : uint8_t {
  SingleStemTree, MultiStemTree, MultiStemShrub, Bush, Hedge, Snag, Stump, FallenLog
};

/* THE CROWN'S SILHOUETTE AS A NUMBER. Each names a profile of allowed half width over the crown's
 * own height; `Free` is the absence of a constraint and `Cut` is the managed box a hedge shear
 * leaves, which is a section and not a profile of revolution. */
enum class CrownEnvelope : uint8_t {
  Free, Conical, Columnar, Ovoid, Domed, Vase, Weeping, Umbrella, FlatTopped, Cut
};

struct GrowthForm {
  Architecture Arch = Architecture::SingleStemTree;
  CrownEnvelope Envelope = CrownEnvelope::Free;
  /* Stems leaving the base. Their radii follow the pipe model — the sum of the leaders' sections
   * equals the single stem's — so a five-stem hazel is not five trunks. */
  int Leaders = 1;
  float LeaderSplayDeg = 0.0f;
  /* The clear stem under the crown, as a fraction of the plant's height. It is where branching
   * starts, where foliage starts, and where the envelope's own t = 0 sits: one statement. */
  float BoleFrac = 0.35f;
  /* Where a dead leader ends, as a fraction of its live run. 0 leaves it whole. */
  float BreakFrac = 0.0f;
  /* Hedge only: the section's length along its own run axis, metres. */
  float RunM = 0.0f;
  bool Foliate = true;

  /* The allowed half width at `t` of the crown, as a fraction of the crown's own half width.
   * t = 0 is the crown base, t = 1 its top. */
  static float Reach(CrownEnvelope envelope, float t);
  /* `height_m` is the vertical extent of a standing plant and the LENGTH of a lying one, so the
   * grower cannot normalise both by the same axis. */
  [[nodiscard]] static bool Lying(Architecture arch) { return arch == Architecture::FallenLog; }
  /* Empty where the name is not one of the enumerators. A form the reader has to guess at is a
   * declaration error and never a default: the caller says so and refuses the file. */
  static std::optional<Architecture> ArchitectureOf(const char *name);
  static std::optional<CrownEnvelope> EnvelopeOf(const char *name);
};

} // namespace outshine::Generators
#endif
