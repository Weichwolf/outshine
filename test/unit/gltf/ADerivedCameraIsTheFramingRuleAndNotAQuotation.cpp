#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

#include "PreparedRoot.h"

#include <utility>
#include "Document.h"
#include "Json.h"
#include "Pose.h"
#include "Subject.h"

using outshine::Json;
using outshine::Gltf::Document;
using outshine::Gltf::Placement;
using outshine::Gltf::Subject;
using outshine::Gltf::Transform;
using outshine::Gltf::Viewport;

namespace {

const std::string kSuite = outshine::Test::PreparedRoot();

const char *const kGenerated = "outshine-generated";

constexpr double kLibmAgreement = 1e-12;

constexpr double kSameCameraM = 1e-3;
constexpr double kDistinctCameraM = 1.0;

enum class Freedoms { Ours, Upstreams };

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

    const std::string leaf = entry.path().parent_path().filename().string();
    const auto begins = [&leaf](const char *prefix) {
      return leaf.compare(0, std::string(prefix).size(), prefix) == 0;
    };
    if (!begins(outshine::Test::kPreparedKhronosPrefix) &&
        !begins(outshine::Test::kPreparedGrownPrefix)) {
      continue;
    }
    found.push_back(
        {directory.substr(std::string(kSuite).size() + 1), directory, entry.path().string()});
  }
  std::sort(found.begin(), found.end(), [](const Case &a, const Case &b) { return a.Id < b.Id; });
  return found;
}

struct Answer {

  bool Declined = false;
  bool Unfetched = false;
  Freedoms Owns = Freedoms::Ours;
  Determination Produced = Determination::Elsewhere;
  bool Judged = false;
};

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

  const double near = declared["clipStartM"].Num(0.0);
  const double far = declared["clipEndM"].Num(0.0);
  std::printf("NOTE   declared clip = [%.9g, %.9g] m, the rule's = [%.9g, %.9g] m\n", near, far,
              framed.ZNearM, framed.ZFarM);
  CHECK(near > 0.0 && near <= framed.ZNearM && far >= framed.ZFarM,
        "the declared clip range is positive and contains the rule's own, so no plane cuts the "
        "silhouette the case is scored on");

  const double roll = declared["rollRad"].Num(0.0);
  std::printf("NOTE   declared roll = %.12g rad, which the framing rule does not produce%s\n", roll,
              (roll == 0.0) ? " and does not contradict" : " -- condition (A) over a rule distance");

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

  answer.Owns = (cameraIsTheFiles && !subjectIsOurs) ? Freedoms::Upstreams : Freedoms::Ours;
  answer.Produced = cameraIsTheFiles ? Determination::TheFilesOwn : Determination::Elsewhere;

  const std::string entry = subject0["entry"].Str("scene.gltf");
  const std::string subjectPath = subjectCase.Directory + "/" + entry;
  // board:1799: this twin SURVEYS every case the tree declares, and the corpus behind them is
  // fetched into the system temp dir by a preparer the fast gate does not run. Saying
  // UNPREPARED once per absent subject turned one swept directory into 160 findings and made
  // the gate red for what it did not judge. board:1765 already settled the honest form: judge
  // what is there, and NAME what is not. A survey that judged nothing is unprepared; a survey
  // that judged something says how much of the declared corpus stood behind it.
  if (Slurp(subjectPath).empty()) {
    answer.Unfetched = true;
    return answer;
  }

  Document document;
  if (!document.ReadFile(subjectPath)) {

    if (root["criterion"]["kind"].StrEquals("limits-probe")) {
      std::printf("DECLINED %s -- %s\n", subjectCase.Id.c_str(), document.Error().c_str());
      answer.Declined = true;
      return answer;
    }
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

  const Json::Ref animation = root["scene"]["animation"];
  const int frameCount = (int)animation["frames"]["value"].Num(1.0);
  const double fps = animation["fps"]["value"].Num(0.0);

  if (frameCount >= 1 && fps > 0.0 && animation["animations"].Size() > 0) {
    std::vector<int> atZero;
    for (size_t at = 0; at < animation["animations"].Size(); ++at) {
      atZero.push_back((int)animation["animations"][at].Num(0.0));
    }
    outshine::Gltf::Pose pose;
    std::string poseError;
    std::vector<outshine::Gltf::Transform> locals;
    std::vector<double> weights;
    if (outshine::Gltf::Pose::Build(
            document, outshine::Span<const int>(atZero.data(), atZero.size()), pose, poseError)) {
      pose.At(0.0, locals, weights);
      Subject start;
      if (start.Build(document,
                      outshine::Span<const outshine::Gltf::Transform>(locals.data(), locals.size()),
                      outshine::Span<const double>(weights.data(), weights.size()))) {
        subject = std::move(start);
      }
    }
  }
  double sweptMin[3] = {subject.MinM()[0], subject.MinM()[1], subject.MinM()[2]};
  double sweptMax[3] = {subject.MaxM()[0], subject.MaxM()[1], subject.MaxM()[2]};
  if (frameCount > 1 && fps > 0.0) {
    std::vector<int> declaredAnimations;
    for (size_t at = 0; at < animation["animations"].Size(); ++at) {
      declaredAnimations.push_back((int)animation["animations"][at].Num(0.0));
    }
    outshine::Gltf::Pose pose;
    std::string poseError;
    std::vector<outshine::Gltf::Transform> locals;
    std::vector<double> weights;
    if (outshine::Gltf::Pose::Build(document,
                                    outshine::Span<const int>(declaredAnimations.data(),
                                                              declaredAnimations.size()),
                                    pose, poseError)) {
      for (int frame = 1; frame < frameCount; ++frame) {
        pose.At((double)frame / fps, locals, weights);
        Subject posed;
        if (!posed.Build(document,
                         outshine::Span<const outshine::Gltf::Transform>(locals.data(), locals.size()),
                         outshine::Span<const double>(weights.data(), weights.size()))) {
          continue;
        }
        for (int axis = 0; axis < 3; ++axis) {
          sweptMin[axis] = std::min(sweptMin[axis], posed.MinM()[axis]);
          sweptMax[axis] = std::max(sweptMax[axis], posed.MaxM()[axis]);
        }
      }
    }
  }

  double centre[3] = {0, 0, 0};
  for (int axis = 0; axis < 3; ++axis) { centre[axis] = 0.5 * (sweptMin[axis] + sweptMax[axis]); }
  Placement framed;
  const bool frames = outshine::Gltf::FramingFor(sweptMin, sweptMax, framed);
  CHECK(frames, "the framing rule resolves a camera from the subject's bounds over its own frame grid");
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

  std::printf("NOTE %-38s freedoms %-11s camera from %-22s r = %.9g m, %zu triangles\n",
              subjectCase.Id.c_str(), Spell(answer.Owns), Spell(answer.Produced),
              subject.RadiusM(), subject.TriangleCount());

  std::printf("NOTE   the framing rule frames this subject at eye (%.17g, %.17g, %.17g) m, aim "
              "(%.17g, %.17g, %.17g) m, yfov %.17g rad, clip [%.17g, %.17g] m\n",
              framed.EyeM[0], framed.EyeM[1], framed.EyeM[2], centre[0], centre[1], centre[2],
              framed.YfovRad, framed.ZNearM, framed.ZFarM);

  if (cameraIsTheFiles) { return answer; }

  std::printf("NOTE %-38s declared eye is %.9g m from the framing rule's\n", "", separation);

  CHECK(separation <= kSameCameraM || separation >= kDistinctCameraM,
        "the declared camera is the framing rule's answer or is a metre or more from it, never "
        "fitted near it, which is the placement neither determination could account for");

  if (answer.Produced == Determination::FramingRule) {
    HoldAgainstTheRule(root, subject, framed, centre);
  }
  return answer;
}

}

int main() {
  using namespace outshine::Test;

  // board:1798: this twin SURVEYS every prepared khronos and grown case. With none on disk it
  // has judged nothing, and nothing judged is UNPREPARED -- the word for "no corpus" -- rather
  // than FAIL, the word for "the code is wrong".
  const std::vector<Case> cases = Cases();
  if (cases.empty()) {
    Unprepared((kSuite + " holds no prepared khronos or grown case -- run python3 "
                         "test/harness/shared/corpus/prepare.py all --every-case")
                   .c_str());
    return Report();
  }
  CHECK(!cases.empty(), "the render suite's case directories are found and their manifests read");

  int ours = 0, upstreams = 0, byTheRule = 0, elsewhere = 0, theFiles = 0, unfetched = 0;
  for (const Case &subjectCase : cases) {
    const Answer answer = Judge(subjectCase);
    if (answer.Unfetched) { ++unfetched; }
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
  Note("cases the tree declares", double(cases.size()), "cases");
  Note("cases whose subject was never fetched", double(unfetched), "cases");
  if (ours + upstreams == 0) {
    Unprepared((kSuite + " holds " + std::to_string(cases.size()) +
                " declared cases and NOT ONE has a fetched subject -- run python3 "
                "test/harness/shared/corpus/prepare.py all --every-case")
                   .c_str());
    return Report();
  }
  Note("cases whose placement freedoms are ours", double(ours), "cases");
  Note("cases whose placement freedoms are upstream's", double(upstreams), "cases");
  Note("cameras produced by the framing rule", double(byTheRule), "cases");
  Note("cameras produced elsewhere", double(elsewhere), "cases");
  Note("cameras read from the subject's own file", double(theFiles), "cases");
  CHECK(byTheRule > 0,
        "at least one case's camera is measurably the framing rule's own output");

  Covers("I.62 the framing rule: bounds from min and max, centre off the extremes, radius, "
         "azimuth 35 deg, elevation 20 deg, 2*atan(12/50), fill 0.6 -- recomputed from each "
         "subject's own vertices and held against what its manifest declares");
  Covers("I.73 the two determinations of the camera distance are distinguishable per case: which "
         "one produced a camera is measured in metres rather than read off the prose, and the band "
         "between them is empty");
  Covers("I.74 the freedoms `2 + k` counts are the ones we own, computed from the camera's "
         "source and the subject's origin together, because `camera.source` is the same word for "
         "two files upstream authored and two we generate");
  return Report();
}
