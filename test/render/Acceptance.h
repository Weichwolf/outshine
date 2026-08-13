/* THE THRESHOLDS, DECLARED ONCE FOR THE WHOLE SUITE, and a case's manifest carries only what it
 * OVERRIDES (doc/requirements.md I.26.10).
 *
 * ONE NUMBER IN ONE FILE IS STRONGER AGAINST TAMPERING THAN TWO HUNDRED SCATTERED ONES, and this
 * looks like the opposite until it is said out loud: editing the line below is a one-line diff that
 * moves every case at once and that nobody can miss, where two hundred per-case numbers are two
 * hundred places a threshold can be nudged to match a result and read as ordinary work.
 *
 * AN OVERRIDE EQUAL TO THE DEFAULT IS REFUSED AT PARSE. A manifest that restates a value it does not
 * change is a quotation that goes stale silently -- this case's own file carried `0.5 px` citing the
 * document that had tightened the number to `0.1 px` four months earlier, and the deciding instrument
 * on the first external check this repository ever had was five times too loose because of it
 * (`doc/bugs.md`). Restating is now a refusal rather than agreement.
 *
 * AND THERE IS NO `iou...Min` SPELLING AT ALL. I.26 rules twice that IoU is published and enforces
 * nothing; an `iouMin` inside a block named `acceptance` IS an enforcement whatever the prose beside
 * it says, so the key set below simply does not contain one and a manifest carrying it is refused by
 * name. */
#ifndef RENDER_ACCEPTANCE_H
#define RENDER_ACCEPTANCE_H

#include <string>
#include <vector>

#include "Json.h"

namespace outshine::Render::Parity {

/* THE SUBJECT CLASS IS THE RUNG'S OWN PROPERTY and it is what selects the boundary bound, so it is
 * stated in the manifest and never guessed from the picture (I.26). */
enum class SubjectClass { OpaqueAtLeastOnePixel, SubPixelPresent };

/* WHAT KIND OF THING THE ASSET SAYS CORRECT IS, and therefore which instrument decides this case
 * (doc/requirements.md I.26.12). It is the ASSET's property, declared in the manifest beside
 * Khronos's own words and the file they came from, so it cannot be changed to fit a result without
 * changing a quotation.
 *
 * `Numeric` -- the asset states a value or a relation, and the answer is the float. The image is
 * the verdict and every channel must agree.
 *
 * `SelfDescribing` -- the asset draws checkmarks, arrows or markers whose correctness is readable
 * FROM THE PICTURE. Then GEOMETRY is still exactly decidable and stays a threshold, while the last
 * bit of a texture filter is not: a path tracer's image interpolation and a rasteriser's are two
 * definitions of one word, and enforcing their agreement would be measuring the filter rather than
 * the criterion. The residual is printed in full and the picture is what is judged.
 *
 * `StatedInvariant` -- the asset states a RELATION ITS OWN RENDER MUST SATISFY, and says in the same
 * breath that an exact appearance match is not the criterion: `PointLightIntensityTest` -- "the exact
 * appearance of this model need not match the screenshot exactly. The appearance depends on the
 * brightness of the scene's IBL, renderer exposure settings, choice of tone mapping" --  and
 * `DirectionalLight` -- "this will of course not be an exact measure since values are read and
 * written with different precision". So the instrument is the relation, computed on the linear tap,
 * and the residual against the oracle is printed beside it and decides nothing. THIS IS NOT A LOOSER
 * `Numeric`: it is a STRONGER demand in a different currency -- an energy ceiling and a per-channel
 * identity are checks a pixel-for-pixel comparison with Cycles cannot make at all -- and it is the
 * only arm whose acceptance neither our renderer nor our oracle authored.
 *
 * `LimitsProbe` -- the asset states it is not expected to render correctly everywhere, so it has no
 * pass; what it produces is a measurement. */
enum class CriterionKind { Numeric, SelfDescribing, StatedInvariant, LimitsProbe };

/* WHAT THE BLENDER RENDER IS FOR ON A `self-describing` CASE, and it is asked because the answer
 * separates the two ways a case can arrive at that kind (doc/requirements.md I.26.12).
 *
 * `Reference` -- Khronos publishes markers and the oracle renders them correctly, so the reference
 * is a second opinion that happens not to be the acceptance.
 *
 * `CannotExpressTheCriterion` -- the case was RECLASSIFIED because the oracle draws a different
 * surface from the one the asset describes, and then the limitation is a property of the instrument
 * and has to be named and measured. A case reclassified without one is a case whose number was
 * inconvenient, so the two required fields below are what make that unspellable rather than
 * discouraged: you cannot say the oracle cannot express the criterion without saying which
 * limitation and without a measurement carrying its own origin. */
enum class OracleRole { Reference, CannotExpressTheCriterion };

/* 0.1 px is twenty times the 0.005 px instrument floor: room for float32 and for a tessellation
 * ordering, none at all for a raster-convention error, which is the half-pixel this rung exists to
 * catch. 0.5 px for a subject with sub-pixel geometry, where a rasteriser drops a triangle no sample
 * centre hits and a path tracer finds it -- a sampling-policy difference, reported and not enforced. */
constexpr double kBoundaryP95OpaquePx = 0.1;   /* [SET] doc/requirements.md I.26 */
constexpr double kBoundaryP95SubPixelPx = 0.5; /* [SET] doc/requirements.md I.26, reported */

/* THE EMPTY-IMAGE GUARD, and it is the one number that makes a generated suite of two hundred
 * directories mean anything: two empty masks have an IoU of 1.0 under most formulations and a
 * boundary displacement of 0, so a case that renders nothing scores perfectly having tested nothing.
 * 0.1 % of 1280x720 is 921.6 px -- about 30x30 -- below which no boundary statistic has a subject. */
constexpr double kCoverageFractionMin = 0.001; /* [SET] */

/* The frame fraction is declared to six decimals in a manifest, so half of the last one is all the
 * agreement that file can express. */
constexpr double kFrameFractionTolerance = 5e-7; /* [SET] */

inline double DefaultBoundaryP95Px(SubjectClass subject) {
  return subject == SubjectClass::OpaqueAtLeastOnePixel ? kBoundaryP95OpaquePx
                                                        : kBoundaryP95SubPixelPx;
}

struct Acceptance {
  SubjectClass Subject = SubjectClass::OpaqueAtLeastOnePixel;
  double BoundaryP95MaxPx = kBoundaryP95OpaquePx;
  double CoverageFractionMin = kCoverageFractionMin;
  double FrameFractionTolerance = kFrameFractionTolerance;
  /* Reported and never enforced: the boundary bound is the verdict, this is the literature's number
   * beside it. */
  bool EnforceBoundary = true;
};

/* Every entry is an object carrying its origin, so a bare float has no spelling in the file and
 * `CLAUDE.md`'s "every number carries its origin" is held by the shape rather than counted by a
 * checker. A `derived` origin without its `derivation` is a refusal. */
[[nodiscard]] inline bool ReadDeclaredNumber(const Json::Ref &entry, const char *name, double &out,
                                             std::string &error) {
  if (!entry.Valid() || entry.GetKind() != Json::Kind::Object) {
    error = std::string(name) + " is not an object carrying value, unit and origin";
    return false;
  }
  const Json::Ref value = entry["value"];
  if (!value.Valid() || value.GetKind() != Json::Kind::Number) {
    error = std::string(name) + " carries no numeric value";
    return false;
  }
  const std::string origin = entry["origin"].Str("");
  if (origin != "SET" && origin != "derived" && origin != "measured") {
    error = std::string(name) + " has origin '" + origin +
            "', and a number's origin is SET, derived or measured";
    return false;
  }
  if (origin == "derived" && entry["derivation"].Str("").empty()) {
    error = std::string(name) + " is derived and states no derivation";
    return false;
  }
  if (entry["unit"].Str("").empty()) {
    error = std::string(name) + " states no unit";
    return false;
  }
  out = value.Num(0.0);
  return true;
}

/* The closed key set. A key outside it is a refusal naming what may be overridden -- which is what
 * makes `iouMin` unspellable rather than merely discouraged. */
[[nodiscard]] inline bool ReadAcceptance(const Json::Ref &declared, Acceptance &out,
                                         std::string &error) {
  if (!declared.Valid()) { return true; }
  if (declared.GetKind() != Json::Kind::Object) {
    error = "acceptance is not an object";
    return false;
  }
  for (size_t entry = 0; entry < declared.Size(); ++entry) {
    const std::string key = declared.Key(entry);
    double value = 0;
    if (!ReadDeclaredNumber(declared[key.c_str()], key.c_str(), value, error)) { return false; }
    if (declared[key.c_str()]["reason"].Str("").empty()) {
      error = "acceptance." + key + " overrides a declared default and states no reason";
      return false;
    }
    double *slot = nullptr;
    double fallback = 0;
    if (key == "boundaryP95MaxPx") {
      slot = &out.BoundaryP95MaxPx;
      fallback = DefaultBoundaryP95Px(out.Subject);
    } else if (key == "coverageFractionMin") {
      slot = &out.CoverageFractionMin;
      fallback = kCoverageFractionMin;
    } else if (key == "frameFractionTolerance") {
      slot = &out.FrameFractionTolerance;
      fallback = kFrameFractionTolerance;
    } else {
      error = "acceptance." + key +
              " is not an overridable threshold; the set is boundaryP95MaxPx, coverageFractionMin, "
              "frameFractionTolerance -- and IoU has no threshold at all, because it is reported";
      return false;
    }
    if (value == fallback) {
      error = "acceptance." + key + " restates the declared default " + std::to_string(fallback) +
              " instead of overriding it, and a restated threshold is a quotation that goes stale";
      return false;
    }
    *slot = value;
  }
  return true;
}

[[nodiscard]] inline bool ReadCriterionKind(const std::string &spelling, CriterionKind &out,
                                            std::string &error) {
  if (spelling == "numeric") {
    out = CriterionKind::Numeric;
    return true;
  }
  if (spelling == "self-describing") {
    out = CriterionKind::SelfDescribing;
    return true;
  }
  if (spelling == "stated-invariant") {
    out = CriterionKind::StatedInvariant;
    return true;
  }
  if (spelling == "limits-probe") {
    out = CriterionKind::LimitsProbe;
    return true;
  }
  error = "criterion.kind '" + spelling +
          "' is none of numeric, self-describing, stated-invariant, limits-probe";
  return false;
}

/* The whole of what a `self-describing` case must say about its reference, refused where it says
 * nothing -- a silent default here would let the reclassification arrive by omission. */
[[nodiscard]] inline bool ReadOracleRole(const Json::Ref &criterion, OracleRole &out,
                                         std::string &error) {
  const std::string spelling = criterion["oracleRole"].Str("");
  if (spelling == "reference") {
    out = OracleRole::Reference;
    return true;
  }
  if (spelling != "cannot-express-the-criterion") {
    error = "criterion.oracleRole '" + spelling +
            "' is neither reference nor cannot-express-the-criterion, and a self-describing case "
            "states which of the two its Blender render is";
    return false;
  }
  out = OracleRole::CannotExpressTheCriterion;
  if (criterion["oracleLimitation"].Str("").empty()) {
    error = "criterion.oracleRole is cannot-express-the-criterion and no oracleLimitation names "
            "what the oracle cannot do";
    return false;
  }
  const Json::Ref measured = criterion["oracleLimitationMeasured"];
  if (measured.Size() == 0) {
    error = "criterion.oracleRole is cannot-express-the-criterion and no oracleLimitationMeasured "
            "carries the measurement that shows it";
    return false;
  }
  for (size_t entry = 0; entry < measured.Size(); ++entry) {
    double value = 0;
    if (!ReadDeclaredNumber(measured[entry], "criterion.oracleLimitationMeasured[]", value,
                            error)) {
      return false;
    }
    if (measured[entry]["of"].Str("").empty()) {
      error = "criterion.oracleLimitationMeasured[] states no `of`, so the number names no subject";
      return false;
    }
  }
  return true;
}

[[nodiscard]] inline bool ReadSubjectClass(const std::string &spelling, SubjectClass &out,
                                           std::string &error) {
  if (spelling == "opaque-min-1px") {
    out = SubjectClass::OpaqueAtLeastOnePixel;
    return true;
  }
  if (spelling == "sub-pixel-present") {
    out = SubjectClass::SubPixelPresent;
    return true;
  }
  error = "subjectClass '" + spelling + "' is neither opaque-min-1px nor sub-pixel-present";
  return false;
}

} // namespace outshine::Render::Parity
#endif
