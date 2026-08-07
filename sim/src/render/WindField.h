/* THE FLOW A PLANT STANDS IN, as a declared field with published properties — not an animation.
 *
 * A scene declares ONE met wind (a bearing it comes from and a speed at the 10 m reference height the
 * Beaufort scale is defined at). Three things follow from it and nothing else is free:
 *
 *   1. the speed AT THE CANOPY TOP, from the logarithmic profile over a canopy of the declared height;
 *   2. the honami — the coherent wave that runs over a plant stand — whose FREQUENCY is the stand's own
 *      eigenfrequency (frequency lock-in) and whose PHASE SPEED is the canopy-top wind. Its wavelength
 *      is therefore not free either: lambda = c / f;
 *   3. the local speed at a place and a time, which is what a blade answers to.
 *
 * The answer of the plant is NOT here. It is the static solution of the blade's own bending equation
 * and it depends on one dimensionless number, the scaled Cauchy number, which this class supplies.
 *
 * WHY IT IS A FIELD AND NOT 1.4 MILLION BODIES: a stand of that size cannot be integrated, but nothing
 * about the flow needs to be. The flow is declared, its properties are published, and every blade's
 * response is the closed solution of its own equation evaluated at its own place — which is "driven",
 * not "animated", as long as no parameter below was chosen because it looked good.
 *
 * SOURCES, and what stayed [SET], are on each constant. The three that carry the picture:
 *   c ~= U_h                Py, de Langre & Moulia 2006, JFM 568:425-449, §4
 *   f  =  f0, lock-in       the same, §2.2 (f/f0 = 1.06 alfalfa, 0.81 wheat, both = 1 within CI)
 *   theta(Cy)               Gosselin, de Langre & Machado-Almeida 2010, JFM 650:319-341, eq. (5.5)
 */
#ifndef WINDFIELD_H
#define WINDFIELD_H

#include <cmath>

#include "Units.h"

namespace outshine::Render {

class WindField {
public:
  WindField(void) { Recompute(); }

  /* The met convention: the bearing the wind comes FROM, and the speed at kRefHeightM. */
  void SetDeclared(double fromDeg, double speedMs) {
    FromDeg_ = fromDeg;
    DeclaredMs_ = speedMs > 0.0 ? speedMs : 0.0;
    const double a = fromDeg * kDeg2Rad;
    DirE_ = -std::sin(a);
    DirN_ = -std::cos(a);
    Recompute();
  }

  /* `canopyHeightM` is the stand this field is read over — a sward and a forest do not see the same
   * wind at their own top. `bladeLenM` is the reference length the eigenfrequency was stated for. */
  void SetCanopy(double canopyHeightM, double bladeLenM) {
    CanopyH_ = canopyHeightM > 0.01 ? canopyHeightM : 0.01;
    RefLenM_ = bladeLenM > 0.01 ? bladeLenM : 0.01;
    Recompute();
  }

  double DirEast(void) const { return DirE_; }
  double DirNorth(void) const { return DirN_; }
  double DeclaredMs(void) const { return DeclaredMs_; }
  double CanopyMs(void) const { return CanopyMs_; }
  double EigenHz(void) const { return EigenHz_; }
  double WaveLengthM(void) const { return WaveLenM_; }
  double PhaseSpeedMs(void) const { return WaveLenM_ * EigenHz_; }
  double GustAmplitude(void) const { return kGustAmp; }
  /* rad/m, the wave vector's components in the (east, north) plane. */
  double WaveKEast(void) const { return WaveK_ * DirE_; }
  double WaveKNorth(void) const { return WaveK_ * DirN_; }

  /* The phase at a place and a time, folded into [0, 2pi) in DOUBLE. A blade forty metres from the eye
   * is a million cells east of Greenwich; f32 cannot carry that abscissa and a phase at once, so the
   * fold happens here and the shader only ever adds a local offset of a few tens of metres. */
  double FoldedPhase(double eastM, double northM, double timeS) const {
    const double p = WaveK_ * (DirE_ * eastM + DirN_ * northM) - kTwoPi * EigenHz_ * timeS;
    const double f = std::fmod(p, kTwoPi);
    return f < 0.0 ? f + kTwoPi : f;
  }

  double SpeedAt(double eastM, double northM, double timeS) const {
    return CanopyMs_ * (1.0 + kGustAmp * std::cos(FoldedPhase(eastM, northM, timeS)));
  }

  /* The SCALED Cauchy number of Gosselin et al. — the ratio of the flow's dynamic pressure on the
   * blade to its bending rigidity, and the ONE dimensionless group the static problem has:
   *
   *   Cy = rho * Cd * L^3 * u^2 / (2 B),   B = mu_A * Vs^2
   *
   * B is the flexural rigidity per unit width; it is not declared anywhere, it FOLLOWS from the
   * eigenfrequency, because a cantilever's first mode fixes sqrt(EI/mu) = 2 pi f0 L^2 / beta^2. The
   * blade's WIDTH cancels out of the whole chain, which is why a template that declares a width does
   * not also get to declare a stiffness. */
  double CauchyCoefficient(void) const { return CauchyK_; }
  double CauchyAt(double speedMs, double bladeLenM) const {
    return CauchyK_ * bladeLenM * bladeLenM * bladeLenM * speedMs * speedMs;
  }

  /* The tip angle away from the blade's own rest tangent, radians. The closed form of the
   * boundary-value problem, solved and fitted in sim/tools/elastica.py: max |error| 0.2702 deg and
   * 0.65 % relative against the solved family over Cy 0.02..44.4. Both ENDS are derived and cannot be
   * moved by the three coefficients — slope Cy/6 at zero load is the linear cantilever, and pi/2 at
   * infinite load is a blade lying in the flow. */
  static double TipAngleRad(double cauchy) {
    const double x = cauchy * (1.0 / (3.0 * kPi));
    const double t = x / (1.0 + x);
    return 0.5 * kPi * (t + (1.0 - t) * t * t * (kKnee0 + t * (kKnee1 + t * kKnee2)));
  }
  /* theta(s)/Theta over the arc: the linear cantilever's own slope shape under a distributed load,
   * and it needs no fit — max |error| 0.0655 over the whole solved family. */
  static double BendShape(double s) {
    const double r = 1.0 - s;
    return 1.0 - r * r * r;
  }

  /* DEFINITION: the Beaufort scale and every met report state the wind at 10 m over open ground. */
  static constexpr double kRefHeightM = 10.0;
  /* [SET] within the published band for a dense canopy (d/h 0.6..0.7, z0/h 0.05..0.13). The pair
   * drives U_h/U_10 and nothing else; at the declared sward it is 0.1685, and the two ends of the
   * band give 0.155 and 0.190, i.e. +-10 % on the canopy-top wind. */
  static constexpr double kZeroPlaneRatio = 0.67;
  static constexpr double kRoughnessRatio = 0.13;
  /* [SET] 2.3 Hz at a 0.527 m blade, bracketed by the only two canopies whose eigenfrequency was
   * measured together with their wave motion (Py et al. 2006, Table 1): alfalfa 1.05 Hz at h = 0.69 m
   * and wheat 2.50 Hz at h = 0.68 m, carried to this blade by the cantilever's own f0 ~ L^-2, which
   * gives 1.80 and 4.16 Hz. No eigenfrequency of a meadow grass was found. The frequency of the wave
   * follows it directly (lock-in); the PHASE SPEED does not, and that is the anchor that is measured. */
  static constexpr double kEigenHzAtRefLen = 2.3;
  /* [SET] the amplitude of the coherent gust as a fraction of the canopy-top mean. The streamwise
   * turbulence intensity at a canopy top is of order 0.5-0.6, but only the phase-locked part of it
   * belongs on this ONE wave, and no measurement of that split was found. */
  static constexpr double kGustAmp = 0.5;
  /* [SET] a flat plate normal to the flow. 1.98 at aspect ratio -> infinity and 1.17 at 1 are the
   * textbook ends; this blade is 48:1. */
  static constexpr double kBladeCd = 1.9;
  /* [SET] the lamina's areal mass, kg/m^2: leaf mass per area over leaf dry-matter content for a
   * meadow grass. No trait-database value was fetched in the round that wrote this. */
  static constexpr double kLaminaKgM2 = 0.15;
  /* ISA sea level. The declared scene stands at 100.6 m, where the true value is 1.213 — 1.0 % on the
   * Cauchy number and 0.5 % on the tip angle at small load. */
  static constexpr double kAirRhoKgM3 = 1.225;
  /* The first root of the clamped-free beam's characteristic equation, cos(b) cosh(b) + 1 = 0. */
  static constexpr double kBeamRoot1 = 1.8751040687;

private:
  static constexpr double kTwoPi = 2.0 * kPi;
  static constexpr double kKnee0 = 1.09246, kKnee1 = -2.01180, kKnee2 = 0.19703;

  void Recompute(void) {
    const double d = kZeroPlaneRatio * CanopyH_;
    const double z0 = kRoughnessRatio * CanopyH_;
    const double top = std::log((CanopyH_ - d) / z0);
    const double ref = std::log((kRefHeightM - d) / z0);
    Profile_ = (ref > 1.0e-6 && top > 0.0) ? top / ref : 1.0;
    CanopyMs_ = DeclaredMs_ * Profile_;

    /* Frequency lock-in: the wave runs at the stand's own eigenfrequency, whatever the wind does.
     * The eigenfrequency itself scales as L^-2 off the reference blade. */
    const double r = RefLenM_ / kRefBladeLenM;
    EigenHz_ = kEigenHzAtRefLen / (r * r);

    /* c = U_h and f = f0 fix the wavelength; it is not a third declaration. A stand in dead calm has
     * no wave, so the wavenumber is clamped rather than divided by zero. */
    WaveLenM_ = CanopyMs_ > 1.0e-4 ? CanopyMs_ / EigenHz_ : 0.0;
    WaveK_ = WaveLenM_ > 1.0e-4 ? kTwoPi / WaveLenM_ : 0.0;

    /* sqrt(EI/mu) of the first mode, m^2/s — the ONE structural property of the species, and the
     * blade's width is not in it. */
    const double vs = kTwoPi * kEigenHzAtRefLen * kRefBladeLenM * kRefBladeLenM
                    / (kBeamRoot1 * kBeamRoot1);
    CauchyK_ = kAirRhoKgM3 * kBladeCd / (2.0 * kLaminaKgM2 * vs * vs);
  }

  /* The blade length the eigenfrequency above is stated at: the reference scene's own meadow, whose
   * length follows from its declared sward height and the population's mean tangent (0.30 / 0.5696). */
  static constexpr double kRefBladeLenM = 0.527;

  double FromDeg_ = 0.0, DeclaredMs_ = 0.0;
  double DirE_ = 0.0, DirN_ = 0.0;
  double CanopyH_ = 0.30, RefLenM_ = kRefBladeLenM;
  double Profile_ = 1.0, CanopyMs_ = 0.0, EigenHz_ = kEigenHzAtRefLen;
  double WaveLenM_ = 0.0, WaveK_ = 0.0, CauchyK_ = 0.0;
};

/* The same two closed forms in WGSL. Spliced, never compiled alone — the constants come from the
 * consumer's own `const` block so one stage cannot silently run a different blade than another.
 *
 * WHAT A TAA PASS NEEDS AND WHERE IT IS: the ONLY time-dependent quantity in the whole layer is the
 * phase `wave.z`. A vertex's position one frame earlier is this same code with `wave.w` in its place,
 * which is why the uniform carries both and why `windBend` takes the angle as an argument instead of
 * reading a clock. Nothing else has to be re-derived to get a motion vector. */
static const char *kWindFieldWGSL = R"(
/* Rodrigues about a HORIZONTAL axis (up x windDir): rotating `up` by +a tilts it downwind exactly. */
fn windBend(v : vec3f, axis : vec3f, a : f32) -> vec3f {
  let ca = cos(a);
  let sa = sin(a);
  return v * ca + cross(axis, v) * sa + axis * (dot(axis, v) * (1.0 - ca));
}

fn windTipAngle(cauchy : f32) -> f32 {
  let x = cauchy * (1.0 / (3.0 * 3.14159265358979));
  let t = x / (1.0 + x);
  return 1.57079632679 * (t + (1.0 - t) * t * t * (kKnee0 + t * (kKnee1 + t * kKnee2)));
}

fn windBendShape(s : f32) -> f32 {
  let r = 1.0 - s;
  return 1.0 - r * r * r;
}
)";

} // namespace outshine::Render
#endif
