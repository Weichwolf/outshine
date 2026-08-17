/* WHICH DETERMINATION PRODUCED A CASE'S CAMERA, MEASURED -- AND THE FRAMING RULE'S OWN OUTPUT
 * PRINTED AND HELD.
 *
 * THE QUOTED DERIVATION. Case manifests declare a camera and state its position, lens and clip range
 * as fifteen-digit doubles, with the framing rule named in prose beside them -- numbers no committed
 * instrument reproduced. A quoted derivation is a [SET] number wearing a derivation's clothes:
 * nothing fails when the bounds in the prose are wrong, because nothing recomputes them. This runs
 * the rule over each case's own subject and scores the declaration against it. The bounds, the radius
 * and the centre come from the file's vertices; the direction, the lens and the fill from Framing.h;
 * and the eye that falls out has to be the eye the manifest declares.
 *
 * THE PROSE IS NOT THE DISCRIMINATOR AND THAT WAS MEASURED THE HARD WAY. This test first read the
 * derivation for the string `src/gltf/Framing.h` and called that the claim; the run then showed eight
 * further cases taking the rule's camera to 0 m while naming it as "the framing rule of
 * board:0083" instead. A wording difference had become a red. **The separation in
 * metres is the discriminator** -- it is 0 m or it is 4.5 m and upwards, with nothing between.
 *
 * THE TWO DETERMINATIONS OF ONE QUANTITY, which is the defect underneath (`board/`
 * I.26.14): the framing rule and the exactness construction both decide the camera distance, and
 * nothing makes them agree. The framing rule wins by default because it runs first, so a case can
 * carry the rational roll and lose the lattice-offset condition without anything noticing. Held here:
 * every camera falls unambiguously to one determination and the band between them is empty. THAT AN
 * `exact` CASE MAY NOT TAKE THE RULE'S CAMERA IS NOT HELD HERE AND NEEDS NO RULE AT ALL: the runner
 * recomputes the lattice offset of every silhouette line, and a distance the framing rule chose
 * satisfies it only by coincidence -- so the measurement refuses the combination where a string
 * comparison would only have reported it.
 *
 * OWNERSHIP, COMPUTED RATHER THAN REMEMBERED. `2 + k >= E` counts the freedoms WE own, and ownership
 * is decided by whose camera it is and not by whose subject it is. `camera.source` alone is the wrong
 * discriminator: four cases carry `"gltf"` and two of those files are ours. The pair
 * (`camera.source`, `subjects[].source.kind`) decides it, so the trap has no spelling here instead of
 * a note asking someone to remember.
 *
 * THE SUBJECTS ARE FETCHED OR GENERATED, NEVER TRACKED (I.26.10), so on a fresh clone they are
 * absent. That is UNPREPARED and it is RED: a skip here could not be told from a pass that framed
 * nothing. */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

#include "Document.h"
#include "Json.h"
#include "Subject.h"

using outshine::Json;
using outshine::Gltf::Document;
using outshine::Gltf::Placement;
using outshine::Gltf::Subject;
using outshine::Gltf::Transform;
using outshine::Gltf::Viewport;

namespace {

const char *const kSuite = "test/khronos/glTF";
/* What `subjects[].source.kind` says when the subject is one this tree produced. Anything else is
 * upstream's, and then whether the freedoms are ours turns on who placed the camera. */
const char *const kGenerated = "outshine-generated";

/* ONE ULP OF A DOUBLE NEAR 60 IS 7.1e-15, and the two sides of every comparison below are the same
 * arithmetic evaluated by two libm implementations -- ours here, CPython's when the manifest was
 * written. `sin`, `cos` and `atan` are correctly rounded in neither, so a few ulps of the largest
 * quantity involved is the floor of what an agreement can be asked for. This tolerance is RELATIVE,
 * and it is orders below the smallest thing it must be able to catch, which is a mistyped last digit
 * at 1e-11 relative. */
constexpr double kLibmAgreement = 1e-12; /* [SET] relative */
/* THE BAND THE TWO DETERMINATIONS ARE SEPARATED BY, and it is held rather than assumed. A camera
 * within a millimetre of the rule's eye took its distance from the rule; one a metre away or more
 * was determined somewhere else. Measured over the suite: the first arm is 0 m exactly at fourteen
 * cases and the second is 4.50 m at its closest, so the empty band is three orders wide on both
 * sides of these two numbers. A camera landing INSIDE it is the hazard this test exists for -- a
 * placement fitted near the rule's answer, belonging to neither determination and attributable to
 * neither. */
constexpr double kSameCameraM = 1e-3;     /* [SET] metres */
constexpr double kDistinctCameraM = 1.0;  /* [SET] metres */

/* WHOSE FREEDOMS THE PLACEMENT SPENDS. Not a boolean over `camera.source`: that string is `"gltf"`
 * for two files upstream authored and two we generate (Enum.2). */
enum class Freedoms { Ours, Upstreams };
/* WHAT PRODUCED THE DECLARED CAMERA, measured and never read off the prose. */
enum class Determination { FramingRule, Elsewhere, TheFilesOwn };
const char *Spell(Freedoms freedoms) {
  return (freedoms == Freedoms::Ours) ? "ours" : "upstream's";
}
const char *Spell(Determination determination) {
  switch (determination) {
    case Determination::FramingRule: return "the framing rule";
    case Determination::Elsewhere: return "elsewhere";
    case Determination::TheFilesOwn: return "the subject's own file";
  }
  return "";
}
/* THE RULING'S OWN FOUR CASES (I.26.14, 2026-08-13), which exist here because they are the trap:
 * all four declare `camera.source = "gltf"` and the freedoms belong to us in two of them. The table
 * is what turns "remember that the string is the wrong discriminator" into a claim that fails. */
struct OwnershipRuling {
  const char *Id;
  Freedoms Owns;
};
constexpr OwnershipRuling kRuledOwnership[] = {
    {"coverage/cameras-orthographic", Freedoms::Upstreams},
    {"coverage/cameras-perspective", Freedoms::Upstreams},
    {"coverage/orthographic-camera", Freedoms::Ours},
    {"coverage/perspective-camera", Freedoms::Ours},
};

std::string Slurp(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

/* `CHECK_NEAR` takes an absolute tolerance because a default one is an unstated acceptance; the
 * quantities here span 0.02 m to 59 m, so the scale each is judged at is stated per call from the
 * declared value itself. */
double AgreementFor(double declared) {
  const double magnitude = std::fabs(declared);
  return kLibmAgreement * ((magnitude > 1.0) ? magnitude : 1.0);
}

double Separation(const double a[3], const double b[3]) {
  const double d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
  return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}

struct Case {
  std::string Id;
  std::string Directory;
  std::string ManifestPath;
};

std::vector<Case> Cases() {
  std::vector<Case> found;
  std::error_code walking;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(kSuite, walking)) {
    if (entry.path().filename() != "manifest.json") { continue; }
    const std::string directory = entry.path().parent_path().string();
    found.push_back(
        {directory.substr(std::string(kSuite).size() + 1), directory, entry.path().string()});
  }
  std::sort(found.begin(), found.end(), [](const Case &a, const Case &b) { return a.Id < b.Id; });
  return found;
}

/* WHAT ONE CASE ANSWERED, so the run's summary is built from the values the lines printed rather
 * than from a second pass over the tree. */
struct Answer {
  Freedoms Owns = Freedoms::Ours;
  Determination Produced = Determination::Elsewhere;
  bool Judged = false;
};

/* THE AGREEMENT BLOCK, run over a case whose camera the measurement attributes to the framing rule --
 * and each quantity is held against the determination that actually produced it, which is the whole
 * point of separating them.
 *
 * WHAT THE RULE PRODUCES: the eye, from the bounds' centre, the radius, the fixed direction and the
 * fill; the aim, which IS that centre; and the lens, from the fixed focal length and sensor height.
 * The eye and the lens are held at the libm floor, nine orders tighter than the millimetre the
 * attribution was made on; the aim is held exactly, because it is a copy of a number and not the
 * output of a transcendental.
 *
 * WHAT A CASE SPENDS ITSELF: the roll and the clip range. The clip range must contain the subject;
 * the roll the rule does not produce and does not contradict. */
void HoldAgainstTheRule(const Json::Ref &root, const Subject &subject, const Placement &framed,
                        const double centre[3]) {
  const Json::Ref declared = root["scene"]["camera"];
  const Json::Ref recipe = root["renders"]["default"];
  const Viewport viewport{recipe["resolutionX"].Num(0.0), recipe["resolutionY"].Num(0.0)};

  for (int axis = 0; axis < 3; ++axis) {
    std::printf("NOTE   bounds axis %d = [%.17g, %.17g] m, centre %.17g m\n", axis,
                subject.MinM()[axis], subject.MaxM()[axis], centre[axis]);
  }
  std::printf("NOTE   derived eye = (%.17g, %.17g, %.17g) m\n", framed.EyeM[0], framed.EyeM[1],
              framed.EyeM[2]);
  std::printf("NOTE   derived yfov = %.17g rad, znear = %.17g m, zfar = %.17g m\n", framed.YfovRad,
              framed.ZNearM, framed.ZFarM);

  double aim[3] = {0, 0, 0};
  for (int axis = 0; axis < 3; ++axis) {
    const double position = declared["positionM"][size_t(axis)].Num(0.0);
    CHECK_NEAR(framed.EyeM[axis], position, AgreementFor(position), "m",
               "the eye the rule derives from the subject's bounds is the eye the manifest declares");
    aim[axis] = declared["lookAtM"][size_t(axis)].Num(0.0);
  }
  const double yfov = declared["yfovRad"].Num(0.0);
  CHECK_NEAR(framed.YfovRad, yfov, AgreementFor(yfov), "rad",
             "the lens is Framing.h's own 2*atan(12/50) and not a number retyped per case");
  CHECK(declared["sensorHeightMm"].Num(0.0) == 24.0,
        "the declared sensor height is the rule's own 2 * 12 mm, exactly and not to a tolerance");

  /* THE AIM IS THE RULE'S TOO, AND IT IS THE BOUNDS' CENTRE EXACTLY. A sub-pixel band here is what
   * let five cases carry an aim 3.5927e-3 m off the centre -- 0.435660418 px, identical to nine
   * digits across subjects at three scales, so a rule somebody applied in pixels -- while their own
   * derivation cited the framing rule, which aims at the centre. 0.4357 px is 87x the oracle's
   * 0.005 px filter half-width on cases whose acceptance is a sub-pixel distance to an edge, so
   * "sub-pixel" was never the right band; the right band is zero. THE OFFSET IS STILL PRINTED IN
   * PIXELS, because metres say nothing until they are divided by `2 * tan(yfov/2) * d / heightPx`,
   * and that is the number a reader needs when this line goes red. */
  const double toCentre = Separation(framed.EyeM, centre);
  const double pixelM = (viewport.HeightPx > 0)
                            ? 2.0 * std::tan(0.5 * framed.YfovRad) * toCentre / viewport.HeightPx
                            : 0.0;
  const double aimOffsetM = Separation(aim, centre);
  std::printf("NOTE   declared aim is %.9g m = %.9g px from the bounds' centre\n", aimOffsetM,
              (pixelM > 0) ? aimOffsetM / pixelM : 0.0);
  CHECK(aimOffsetM == 0.0,
        "the declared aim IS the bounds' centre, which is the point the framing rule aims at -- an "
        "aim beside it is a second determination of the camera wearing the rule's distance");

  /* THE CLIP RANGE IS THE CASE'S, and what it owes is the subject: a near plane in front of the
   * subject's nearest point or a far plane inside it would clip the very silhouette the comparison
   * scores. The rule's own [distance - r, distance + r] is what "the subject" means here. */
  const double near = declared["clipStartM"].Num(0.0);
  const double far = declared["clipEndM"].Num(0.0);
  std::printf("NOTE   declared clip = [%.9g, %.9g] m, the rule's = [%.9g, %.9g] m\n", near, far,
              framed.ZNearM, framed.ZFarM);
  CHECK(near > 0.0 && near <= framed.ZNearM && far >= framed.ZFarM,
        "the declared clip range is positive and contains the rule's own, so no plane cuts the "
        "silhouette the case is scored on");

  /* THE ROLL IS THE CASE'S AND NOT THE RULE'S. The rule produces zero; eight cases declare a roll of
   * -arctan(1/2) on top of it, which is condition (A) of the exactness construction sitting beside a
   * distance that knows nothing about condition (B) -- the mixed determination I.26.14 names. It is
   * PUBLISHED here rather than refused, because refusing it would decide the classification this
   * instrument was built to inform. */
  const double roll = declared["rollRad"].Num(0.0);
  std::printf("NOTE   declared roll = %.12g rad, which the framing rule does not produce%s\n", roll,
              (roll == 0.0) ? " and does not contradict" : " -- condition (A) over a rule distance");

  /* THE WHOLE CAMERA REBUILT: the rule's eye and lens, the case's aim, roll and clip. If that is
   * really what the manifest declares, the frame fraction it comes out at is the one the case states
   * -- which the runner computes independently from the declared numbers. */
  Placement asDeclared;
  const bool rolls = Placement::LookAt(framed.EyeM, aim, roll, asDeclared);
  CHECK(rolls, "the rule's eye with the case's own aim and roll resolve to a camera basis");
  if (!rolls) { return; }
  asDeclared.YfovRad = framed.YfovRad;
  asDeclared.ZNearM = near;
  asDeclared.ZFarM = far;

  CHECK(viewport.WidthPx > 0 && viewport.HeightPx > 0,
        "the case's default recipe states the frame the fraction is taken over");
  Transform clip;
  const bool projects = asDeclared.Clip(viewport.Aspect(), clip);
  CHECK(projects, "the derived placement and lens yield a projection");
  if (!projects || viewport.HeightPx <= 0) { return; }
  const double fraction =
      subject.ProjectedAreaPx(clip, viewport) / (viewport.WidthPx * viewport.HeightPx);
  const Json::Ref accepted = root["expected"]["subjectFrameFraction"];
  CHECK(accepted["value"].GetKind() == Json::Kind::Number,
        "the case states the projected frame fraction its boundary bound is applied under");
  /* Not the libm tolerance: this is a sum over every triangle of the subject -- 1 500 224 of them for
   * ABeautifulGame -- so it carries accumulated rounding as well as the camera's. 5e-7 is the
   * runner's own `frame_fraction_error` threshold, read from the same rule rather than restated as a
   * second opinion. */
  CHECK_NEAR(fraction, accepted["value"].Num(0.0), 5e-7, "dimensionless",
             "the frame fraction under the rule's own camera is the fraction the case declares");
  std::printf("NOTE   frame fraction under the rule = %.17g\n", fraction);
}

Answer Judge(const Case &subjectCase) {
  Answer answer;
  const std::string text = Slurp(subjectCase.ManifestPath);
  Json manifest;
  if (!manifest.Parse(text.c_str(), text.size())) {
    CHECK(false, "the case's manifest parses");
    return answer;
  }
  const Json::Ref root = manifest.Root();
  const Json::Ref declared = root["scene"]["camera"];
  const Json::Ref subject0 = root["subjects"][size_t(0)];

  const bool cameraIsTheFiles = declared["source"].StrEquals("gltf");
  const bool subjectIsOurs = subject0["source"]["kind"].StrEquals(kGenerated);
  /* The ruling's three ownership cases as one expression: the placement is ours unless BOTH the file
   * and the camera inside it are upstream's. */
  answer.Owns = (cameraIsTheFiles && !subjectIsOurs) ? Freedoms::Upstreams : Freedoms::Ours;
  answer.Produced = cameraIsTheFiles ? Determination::TheFilesOwn : Determination::Elsewhere;

  const std::string entry = subject0["entry"].Str("scene.gltf");
  const std::string subjectPath = subjectCase.Directory + "/" + entry;
  if (Slurp(subjectPath).empty()) {
    outshine::Test::Unprepared(subjectPath.c_str());
    return answer;
  }

  Document document;
  if (!document.ReadFile(subjectPath)) {
    CHECK(false, "the case's subject reads");
    std::printf("       %s\n", document.Error().c_str());
    return answer;
  }
  Subject subject;
  if (!subject.Build(document)) {
    CHECK(false, "the case's default scene flattens");
    std::printf("       %s\n", subject.Error().c_str());
    return answer;
  }

  double centre[3] = {0, 0, 0};
  subject.CentreM(centre);
  Placement framed;
  const bool frames = subject.Frame(framed);
  CHECK(frames, "the framing rule resolves a camera from the subject's own bounds");
  if (!frames) { return answer; }
  answer.Judged = true;

  double position[3] = {0, 0, 0};
  for (int axis = 0; axis < 3; ++axis) {
    position[axis] = declared["positionM"][size_t(axis)].Num(0.0);
  }
  const double separation = Separation(framed.EyeM, position);
  if (!cameraIsTheFiles && separation <= kSameCameraM) {
    answer.Produced = Determination::FramingRule;
  }

  /* THE LEDGER LINE. Everything a reader needs to attribute a residual to a determination is on it:
   * who owns the freedoms, what produced the camera, and the subject's own size. THE ACCEPTANCE
   * CLASS IS NOT ON IT and is not read here: the runner reads it, from the one reader that also
   * enforces it (test/shared/render/Acceptance.h), and a second spelling of the same word in a layer that
   * cannot include that file is a statement in two places. */
  std::printf("NOTE %-38s freedoms %-11s camera from %-22s r = %.9g m, %zu triangles\n",
              subjectCase.Id.c_str(), Spell(answer.Owns), Spell(answer.Produced),
              subject.RadiusM(), subject.TriangleCount());
  /* THE RULE'S ANSWER FOR EVERY CASE, WHETHER OR NOT THE CASE TOOK IT -- which is what makes this a
   * usable instrument rather than only a check. A case being written has no camera yet; it declares
   * its subject, this prints what the rule frames it at, and the manifest is filled from here. Two
   * cases with the same subject then get the same picture because they read the same line, not
   * because two people ran the same arithmetic twice. */
  std::printf("NOTE   the framing rule frames this subject at eye (%.17g, %.17g, %.17g) m, aim "
              "(%.17g, %.17g, %.17g) m, yfov %.17g rad, clip [%.17g, %.17g] m\n",
              framed.EyeM[0], framed.EyeM[1], framed.EyeM[2], centre[0], centre[1], centre[2],
              framed.YfovRad, framed.ZNearM, framed.ZFarM);

  if (cameraIsTheFiles) { return answer; }

  std::printf("NOTE %-38s declared eye is %.9g m from the framing rule's\n", "", separation);
  /* THE BAND BETWEEN THE TWO DETERMINATIONS IS EMPTY, and that is what makes them distinguishable at
   * all. A camera inside it belongs to neither and can be attributed to neither. */
  CHECK(separation <= kSameCameraM || separation >= kDistinctCameraM,
        "the declared camera is the framing rule's answer or is a metre or more from it, never "
        "fitted near it, which is the placement neither determination could account for");

  if (answer.Produced == Determination::FramingRule) {
    HoldAgainstTheRule(root, subject, framed, centre);
  }
  return answer;
}

} // namespace

int main() {
  using namespace outshine::Test;

  const std::vector<Case> cases = Cases();
  /* A walk that found nothing would report a clean run over an empty set, which is the vacuous shape
   * this suite's whole guard exists to catch. */
  CHECK(!cases.empty(), "the render suite's case directories are found and their manifests read");

  int ours = 0, upstreams = 0, byTheRule = 0, elsewhere = 0, theFiles = 0;
  for (const Case &subjectCase : cases) {
    const Answer answer = Judge(subjectCase);
    if (!answer.Judged) { continue; }
    (answer.Owns == Freedoms::Ours ? ours : upstreams)++;
    switch (answer.Produced) {
      case Determination::FramingRule: ++byTheRule; break;
      case Determination::Elsewhere: ++elsewhere; break;
      case Determination::TheFilesOwn: ++theFiles; break;
    }
    for (const OwnershipRuling &ruled : kRuledOwnership) {
      if (subjectCase.Id != ruled.Id) { continue; }
      CHECK(answer.Owns == ruled.Owns,
            "the four cases the ruling names by hand are owned as it ruled, computed from the "
            "camera's source and the subject's origin together and never from either alone");
    }
  }
  Note("cases whose placement freedoms are ours", double(ours), "cases");
  Note("cases whose placement freedoms are upstream's", double(upstreams), "cases");
  Note("cameras produced by the framing rule", double(byTheRule), "cases");
  Note("cameras produced elsewhere", double(elsewhere), "cases");
  Note("cameras read from the subject's own file", double(theFiles), "cases");
  CHECK(byTheRule > 0,
        "at least one case's camera is measurably the framing rule's own output");

  Covers("I.26.10 the framing rule: bounds from min and max, centre off the extremes, radius, "
         "azimuth 35 deg, elevation 20 deg, 2*atan(12/50), fill 0.6 -- recomputed from each "
         "subject's own vertices and held against what its manifest declares");
  Covers("I.26.14 the two determinations of the camera distance are distinguishable per case: which "
         "one produced a camera is measured in metres rather than read off the prose, and the band "
         "between them is empty");
  Covers("I.26.14 the freedoms `2 + k` counts are the ones we own, computed from the camera's "
         "source and the subject's origin together, because `camera.source` is the same word for "
         "two files upstream authored and two we generate");
  return Report();
}
