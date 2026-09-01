#include "GrowthForm.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>

namespace outshine::Generators {

namespace {

struct Profile {
  float A, B;
};

constexpr Profile kProfiles[] = {
    {.A = 0.00f, .B = 0.00f},
    {.A = 0.00f, .B = 1.00f},
    {.A = 0.10f, .B = 0.10f},
    {.A = 0.55f, .B = 0.95f},
    {.A = 0.30f, .B = 0.55f},
    {.A = 1.20f, .B = 0.25f},
    {.A = 0.15f, .B = 0.30f},
    {.A = 2.50f, .B = 0.30f},
    {.A = 1.60f, .B = 0.10f},
    {.A = 0.00f, .B = 0.00f},
};

constexpr float kFloor = 0.05f;

[[nodiscard]] bool Same(const char *a, const char *b) {
  return (a != nullptr) && (b != nullptr) && std::strcmp(a, b) == 0;
}

} // namespace

float GrowthForm::Reach(CrownEnvelope envelope, float t) {
  if (envelope == CrownEnvelope::Free) { return 1.0f; }
  if (t > 1.0f) { return 0.0f; }
  if (envelope == CrownEnvelope::Cut) { return 1.0f; }
  t = std::max(t, kFloor);
  const Profile p = kProfiles[static_cast<size_t>(envelope)];
  const float peak = p.A / (p.A + p.B);
  const float denom = std::pow(peak, p.A) * std::pow(1.0f - peak, p.B);
  if (denom <= 0.0f) { return 1.0f; }
  return std::pow(t, p.A) * std::pow(1.0f - t, p.B) / denom;
}

std::optional<Architecture> GrowthForm::ArchitectureOf(const char *name) {
  if (Same(name, "single_stem_tree")) { return Architecture::SingleStemTree; }
  if (Same(name, "multi_stem_tree")) { return Architecture::MultiStemTree; }
  if (Same(name, "multi_stem_shrub")) { return Architecture::MultiStemShrub; }
  if (Same(name, "bush")) { return Architecture::Bush; }
  if (Same(name, "hedge")) { return Architecture::Hedge; }
  if (Same(name, "snag")) { return Architecture::Snag; }
  if (Same(name, "stump")) { return Architecture::Stump; }
  if (Same(name, "fallen_log")) { return Architecture::FallenLog; }
  return std::nullopt;
}

std::optional<CrownEnvelope> GrowthForm::EnvelopeOf(const char *name) {
  if (Same(name, "free")) { return CrownEnvelope::Free; }
  if (Same(name, "conical")) { return CrownEnvelope::Conical; }
  if (Same(name, "columnar")) { return CrownEnvelope::Columnar; }
  if (Same(name, "ovoid")) { return CrownEnvelope::Ovoid; }
  if (Same(name, "domed")) { return CrownEnvelope::Domed; }
  if (Same(name, "vase")) { return CrownEnvelope::Vase; }
  if (Same(name, "weeping")) { return CrownEnvelope::Weeping; }
  if (Same(name, "umbrella")) { return CrownEnvelope::Umbrella; }
  if (Same(name, "flat_topped")) { return CrownEnvelope::FlatTopped; }
  if (Same(name, "cut")) { return CrownEnvelope::Cut; }
  return std::nullopt;
}

} // namespace outshine::Generators
