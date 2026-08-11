#include "GrowthForm.h"

#include <cmath>
#include <cstring>

namespace outshine::Generators {

namespace {

struct Profile {
  float A, B;
};

/* ONE FAMILY FOR EIGHT SILHOUETTES: t^a (1-t)^b, normalised to peak 1. The pair moves the widest
 * point to a/(a+b) and decides how hard each end tapers, so a cone (a = 0), a column (a = b, both
 * small) and an umbrella (a >> b) are the same expression with different exponents rather than
 * eight special cases. Every pair is [SET] against the silhouette its name denotes; none is
 * measured, and each is falsifiable against a photograph of the habit. */
constexpr Profile kProfiles[] = {
    {0.00f, 0.00f},   /* Free — never evaluated */
    {0.00f, 1.00f},   /* Conical: widest at the crown base, straight to a point */
    {0.10f, 0.10f},   /* Columnar: flat flanks, both ends rounded off */
    {0.55f, 0.95f},   /* Ovoid: widest at 0.37, tapering top */
    {0.30f, 0.55f},   /* Domed: widest at 0.35, broad rounded top */
    {1.20f, 0.25f},   /* Vase: narrow at the fork, spreading upward */
    {0.15f, 0.30f},   /* Weeping: broad and round; the hang is branch_up_bias, not the envelope */
    {2.50f, 0.30f},   /* Umbrella: nothing wide below, a flat head at 0.89 */
    {1.60f, 0.10f},   /* FlatTopped: the pine plate, widest at 0.94 */
    {0.00f, 0.00f},   /* Cut — a section, not a profile */
};

/* No profile is evaluated at its own zero: an ovoid pinched to a point at the crown base would pull
 * every lowest shoot onto the axis, and a weeping crown hangs BELOW its base and must keep its
 * width there. */
constexpr float kFloor = 0.05f;

bool Same(const char *a, const char *b) { return a && b && std::strcmp(a, b) == 0; }

} // namespace

float GrowthForm::Reach(CrownEnvelope envelope, float t) {
  if (envelope == CrownEnvelope::Free) { return 1.0f; }
  if (t > 1.0f) { return 0.0f; }
  if (envelope == CrownEnvelope::Cut) { return 1.0f; }
  if (t < kFloor) { t = kFloor; }
  const Profile p = kProfiles[(size_t)envelope];
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
