/* THE WHITE FURNACE, on our own BRDF and on nothing else. `DirectionalLight` exists to catch an NDF
 * factor that ADDS light -- "it is important that this factor does not add light energy" -- and the
 * quantity that statement is about is the DIRECTIONAL ALBEDO, the fraction of the light arriving
 * from a uniform white environment that a surface sends back:
 *
 *     E(V) = integral over the hemisphere of f(V, L) * (N.L) dL,   and E(V) <= 1.
 *
 * IT IS NOT A PER-PIXEL CEILING ON OUTGOING RADIANCE. A microfacet lobe concentrates a fixed
 * irradiance into a small solid angle, so a highlight above the incident irradiance is what
 * "concentrated" means; the specification's own Appendix B returns f_spec * N.L = 4.857 at the
 * roughness-0.16 sphere's centre, so a ceiling of 0.9 there is a test the reference implementation
 * fails (board:0085). The integral is what an added factor actually breaks.
 *
 * THE SAMPLER AND ITS DENSITY ARE WRITTEN HERE AND NOT TAKEN FROM THE SUBJECT, and that is the whole
 * design of this instrument. The obvious white furnace importance-samples the BRDF's own
 * distribution, and then D appears in the numerator and in the pdf and CANCELS -- so a D scaled by
 * any constant, which is exactly the defect the asset is named for, returns the same number and the
 * test is blind to it. The visible-normal sampler and its density below are Heitz's (JCGT 7(4),
 * 2018), spelled independently, so a factor in `BrdfDistribution` survives the ratio and moves the
 * integral by that factor.
 *
 * F0 = 1 FOR THE CLAIM. A Fresnel below one scales the lobe down and would hide an excess under it;
 * a perfect reflector is the configuration in which the microfacet model has nowhere to hide, and it
 * is what "white furnace" names. The dielectric sweep beside it is reported and carries a finding of
 * its own -- see the note this test prints for the grazing angles. */
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "Check.h"

#include "MetalRoughBrdf.h"

using outshine::Render::BrdfGeometry;
using outshine::Render::BrdfTerms;
using outshine::Render::kBrdfPi;
using outshine::Render::kDielectricF0;
using outshine::Render::MetalRoughBrdf;

namespace {

struct Vector {
  double X = 0, Y = 0, Z = 0;
};

double Dot(const Vector &left, const Vector &right) {
  return left.X * right.X + left.Y * right.Y + left.Z * right.Z;
}

Vector Normalised(const Vector &of) {
  const double length = std::sqrt(Dot(of, of));
  return {of.X / length, of.Y / length, of.Z / length};
}

/* Hammersley: one stratified pair per index, so the sweep's numbers are the same on every run and a
 * difference between two rounds is a change in the subject rather than in a seed. */
struct SamplePair {
  double First = 0, Second = 0;
};

SamplePair Hammersley(uint32_t index, uint32_t count) {
  uint32_t bits = index;
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return {(index + 0.5) / count, bits * 2.3283064365386963e-10};
}

/* Heitz 2018, section 3.2: a half-vector drawn from the distribution of visible normals, by
 * projecting the hemisphere of the stretched view direction onto a disc. */
Vector SampleVisibleNormal(const Vector &view, double alpha, const SamplePair &at) {
  const Vector stretched = Normalised({alpha * view.X, alpha * view.Y, view.Z});
  const double planar = stretched.X * stretched.X + stretched.Y * stretched.Y;
  const Vector first = planar > 0 ? Vector{-stretched.Y / std::sqrt(planar),
                                           stretched.X / std::sqrt(planar), 0}
                                  : Vector{1, 0, 0};
  const Vector second{stretched.Y * first.Z - stretched.Z * first.Y,
                      stretched.Z * first.X - stretched.X * first.Z,
                      stretched.X * first.Y - stretched.Y * first.X};
  const double radius = std::sqrt(at.First);
  const double angle = 2.0 * kBrdfPi * at.Second;
  const double across = radius * std::cos(angle);
  double along = radius * std::sin(angle);
  const double lean = 0.5 * (1.0 + stretched.Z);
  along = (1.0 - lean) * std::sqrt(std::fmax(0.0, 1.0 - across * across)) + lean * along;
  const double up = std::sqrt(std::fmax(0.0, 1.0 - across * across - along * along));
  const Vector unstretched{across * first.X + along * second.X + up * stretched.X,
                           across * first.Y + along * second.Y + up * stretched.Y,
                           across * first.Z + along * second.Z + up * stretched.Z};
  return Normalised({alpha * unstretched.X, alpha * unstretched.Y, std::fmax(0.0, unstretched.Z)});
}

/* The density of that sampler over the LIGHT direction, spelled from the same paper and never from
 * the subject: p(L) = G1(V) * max(0, V.H) * D(H) / (N.V) / (4 V.H). Its own D and its own Smith
 * masking, so a factor in either of the subject's terms cannot cancel here. */
double VisibleNormalDensity(double alpha, double nv, double nh) {
  const double alphaSquared = alpha * alpha;
  const double denominator = nh * nh * (alphaSquared - 1.0) + 1.0;
  const double distribution = alphaSquared / (kBrdfPi * denominator * denominator);
  const double masking =
      2.0 * nv / (nv + std::sqrt(alphaSquared + (1.0 - alphaSquared) * nv * nv));
  return masking * distribution / (4.0 * nv);
}

/* WHAT ONE SWEEP POINT PRODUCES: the two halves of the directional albedo, separately, because the
 * microfacet lobe is what the claim is about and the diffuse term is coupled to it only through
 * `1 - F`. Both are dimensionless fractions of the incident irradiance. */
struct DirectionalAlbedo {
  double Specular = 0;
  double Diffuse = 0;
  double Total() const { return Specular + Diffuse; }
};

DirectionalAlbedo Integrate(double roughness, double nv, double metalness, uint32_t samples) {
  const double alpha = roughness * roughness;
  const double alphaSquared = alpha * alpha;
  /* Albedo 1 on every channel is what makes this a WHITE furnace, so one channel carries all three
   * and the sweep prints a scalar. */
  const std::array<double, 3> f0{metalness > 0.5 ? 1.0 : kDielectricF0,
                                 metalness > 0.5 ? 1.0 : kDielectricF0,
                                 metalness > 0.5 ? 1.0 : kDielectricF0};
  const std::array<double, 3> diffuseColour{1.0 - metalness, 1.0 - metalness, 1.0 - metalness};
  const Vector view{std::sqrt(std::fmax(0.0, 1.0 - nv * nv)), 0, nv};
  DirectionalAlbedo albedo;
  for (uint32_t index = 0; index < samples; ++index) {
    const SamplePair at = Hammersley(index, samples);
    if (alphaSquared > 0.0) {
      const Vector half = SampleVisibleNormal(view, alpha, at);
      const double vh = Dot(view, half);
      const Vector light{2.0 * vh * half.X - view.X, 2.0 * vh * half.Y - view.Y,
                         2.0 * vh * half.Z - view.Z};
      const double density = VisibleNormalDensity(alpha, nv, std::fmax(half.Z, 0.0));
      if (light.Z > 0.0 && vh > 0.0 && density > 0.0) {
        const BrdfGeometry geometry{light.Z, nv, std::fmax(half.Z, 0.0), vh};
        const BrdfTerms terms = MetalRoughBrdf(diffuseColour, f0, alphaSquared, geometry);
        albedo.Specular += terms.Specular[0] * light.Z / density;
      }
    }
    /* The diffuse half over a cosine-distributed light direction, whose density is cos/pi -- so the
     * estimator is the term times pi and the subject's own 1/pi is what it has to cancel. */
    const double radius = std::sqrt(at.First);
    const double angle = 2.0 * kBrdfPi * at.Second;
    const Vector light{radius * std::cos(angle), radius * std::sin(angle),
                       std::sqrt(std::fmax(0.0, 1.0 - at.First))};
    const Vector half = Normalised({light.X + view.X, light.Y + view.Y, light.Z + view.Z});
    const BrdfGeometry geometry{light.Z, nv, std::fmax(half.Z, 0.0),
                                std::fmax(Dot(view, half), 0.0)};
    const BrdfTerms terms = MetalRoughBrdf(diffuseColour, f0, alphaSquared, geometry);
    albedo.Diffuse += terms.Diffuse[0] * kBrdfPi;
  }
  albedo.Specular /= samples;
  albedo.Diffuse /= samples;
  return albedo;
}

/* The full range, both ends included: 0 is the delta-lobe arm the model states for itself and 1 is
 * glTF's maximum roughness. */
constexpr double kRoughnessSweep[] = {0.0, 0.05, 0.1, 0.16, 0.25, 0.33, 0.5, 0.7, 0.85, 1.0};
/* Head-on, and three angles down to 81.4 degrees off the normal, which is where a Schlick Fresnel
 * approaches one and where every energy defect of this model lives. */
constexpr double kViewCosineSweep[] = {1.0, 0.7, 0.4, 0.15};

constexpr uint32_t kSamples = 1u << 18;
/* The same integral at a quarter of the samples: the difference between the two IS this instrument's
 * own error, published rather than folded into the acceptance. */
constexpr uint32_t kCoarseSamples = 1u << 16;

/* The largest value the sweep found and where it found it, carried together so the report cannot
 * print a maximum next to another sweep point's coordinates. */
struct Extremum {
  double Value = 0;
  double Roughness = 0;
  double ViewCosine = 0;

  void Offer(double value, double roughness, double nv) {
    if (value <= Value) { return; }
    Value = value;
    Roughness = roughness;
    ViewCosine = nv;
  }

  void Report(const char *what) const {
    outshine::Test::Note(what, Value, "dimensionless");
    outshine::Test::Note("at roughness", Roughness, "dimensionless");
    outshine::Test::Note("at N.V", ViewCosine, "dimensionless");
  }
};

struct Sweep {
  Extremum Microfacet;
  Extremum DielectricTotal;
  double EstimatorFloor = 0;
};

Sweep RunSweep(void) {
  Sweep found;
  for (const double roughness : kRoughnessSweep) {
    for (const double nv : kViewCosineSweep) {
      const DirectionalAlbedo metal = Integrate(roughness, nv, 1.0, kSamples);
      const DirectionalAlbedo coarse = Integrate(roughness, nv, 1.0, kCoarseSamples);
      const DirectionalAlbedo dielectric = Integrate(roughness, nv, 0.0, kSamples);
      std::printf("SWEEP roughness = %.2f, N.V = %.2f, metal E = %.9g, dielectric E = %.9g "
                  "(specular %.9g + diffuse %.9g)\n",
                  roughness, nv, metal.Specular, dielectric.Total(), dielectric.Specular,
                  dielectric.Diffuse);
      found.EstimatorFloor =
          std::fmax(found.EstimatorFloor, std::fabs(metal.Specular - coarse.Specular));
      found.Microfacet.Offer(metal.Specular, roughness, nv);
      found.DielectricTotal.Offer(dielectric.Total(), roughness, nv);
      CHECK(metal.Specular <= 1.0,
            "the microfacet lobe of a perfect reflector returns no more than it receives");
    }
  }
  return found;
}

} // namespace

int main() {
  const Sweep found = RunSweep();
  found.Microfacet.Report("worst microfacet directional albedo");
  outshine::Test::Note("estimator floor, 2^18 against 2^16 samples", found.EstimatorFloor,
                       "dimensionless");

  /* REPORTED AND NOT REFUSED, with its cause named. glTF's Appendix B couples the two halves by the
   * Fresnel of the LIGHT's own half-vector -- `f_diffuse = (1 - F(V.H)) * diffuse` -- and at a
   * grazing view F rises towards 1 in the specular lobe while the diffuse term's own V.H stays near
   * 0.76, so the two do not sum to the surface's albedo. The excess is the specification's model and
   * not a transcription error here, and refusing it would put this tree's renderer outside the model
   * every Khronos criterion is stated in. It is a requirement line, not a threshold. */
  found.DielectricTotal.Report(
      "worst dielectric directional albedo, diffuse and specular together");

  outshine::Test::Covers("I.26.12 DirectionalLight's energy half: the directional albedo of the "
                         "microfacet lobe, integrated over the hemisphere at a roughness sweep, "
                         "with no asset, no oracle and no punctual light");
  return outshine::Test::Report();
}
