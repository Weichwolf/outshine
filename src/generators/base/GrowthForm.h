#ifndef OUTSHINE_WORLD_GENERATORS_GROWTHFORM_H
#define OUTSHINE_WORLD_GENERATORS_GROWTHFORM_H

#include <cstdint>
#include <optional>

namespace outshine::Generators {

enum class Architecture : uint8_t {
  SingleStemTree, MultiStemTree, MultiStemShrub, Bush, Hedge, Snag, Stump, FallenLog
};

enum class CrownEnvelope : uint8_t {
  Free, Conical, Columnar, Ovoid, Domed, Vase, Weeping, Umbrella, FlatTopped, Cut
};

struct GrowthForm {
  Architecture Arch = Architecture::SingleStemTree;
  CrownEnvelope Envelope = CrownEnvelope::Free;

  int Leaders = 1;
  float LeaderSplayDeg = 0.0f;

  float BoleFrac = 0.35f;

  float BreakFrac = 0.0f;

  float RunM = 0.0f;
  bool Foliate = true;

  static float Reach(CrownEnvelope envelope, float t);

  [[nodiscard]] static bool Lying(Architecture arch) { return arch == Architecture::FallenLog; }

  static std::optional<Architecture> ArchitectureOf(const char *name);
  static std::optional<CrownEnvelope> EnvelopeOf(const char *name);
};

}
#endif
