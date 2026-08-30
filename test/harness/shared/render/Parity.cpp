#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <optional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Check.h"

#include "RenderCase.h"
#include "Drives.h"
#include "Handed.h"
#include <Loaded.h>

#include "Shaped.h"
#include "Surfacing.h"

#include "Acceptance.h"
#include "Attribution.h"
#include "Exactness.h"
#include "Invariant.h"
#include "ManifestSchema.h"
#include "Mask.h"
#include "OracleProduct.h"
#include "Metric.h"
#include "RawF32.h"
#include "PictureBound.h"
#include "Pictures.h"
#include "Radiance.h"
#include "SurfaceIdentity.h"
#include "Ties.h"

#include "SubjectProxy.h"
#include "Wgs84.h"
#include "Log.h"
#include "Pose.h"
#include "SceneRenderer.h"

using outshine::Json;
using outshine::Gltf::Viewpoint;
using outshine::Gltf::Transform;
using outshine::Gltf::Viewport;
using namespace outshine::Render::Parity;
using Handed = outshine::Test::Handed;
using Document = outshine::Gltf::Document;
using Subject = outshine::Gltf::Subject;

namespace {
using outshine::Render::SurfaceTable;
using outshine::Render::SurfaceRasters;
using outshine::Render::ColourFrom;
using outshine::Render::ColourCarrier;

enum class SceneLights { None, FromFile, DeclaredSun };

constexpr double kFactoryWorldRadiance = 0.05087608844041824;

struct Case {
  std::string Directory;
  Json Manifest;
  outshine::Loaded Held;
  Handed Model;


  // WHAT THIS CASE SAYS TO THE DOOR. The `Document` above is still read, and that is not a reach
  // past the door: a conformance harness parses the VENDOR'S FILE to know what the answer should
  // be, which is reading a FORMAT rather than driving an engine. What goes through `include/` is
  // everything that renders -- the declaration, the frame, the readback -- because that is the
  // submission path, and a gate blind to it is CLAUDE.md's first named trap.
  outshine::Test::Drives Driving;

  // THE RENDERER'S OWN VIEW OF THAT GEOMETRY, AND ITS STORE. The proxy holds a POINTER to the
  // shape it stands on, so both have to outlive it -- and a `Case` is what outlives everything
  // here. Re-forming is what a re-posed subject means, so this is a method rather than a field
  // filled once.
  mutable outshine::Render::ShapeStore Store;
  mutable outshine::Render::Shape Shape;
  const outshine::Render::Shape &Shaped() const {
    Shape = outshine::Gltf::Shaped(Held.geometry(), Store);
    return Shape;
  }
  Viewpoint Eye;
  Viewport Frame;
  Acceptance Accepted;
  ExactnessClass Placement = ExactnessClass::GeneralPosition;

  double OracleFloorPx = 0;
  CriterionKind Criterion = CriterionKind::Numeric;
  OracleRole Oracle = OracleRole::Reference;
  std::string CameraSource;

  std::vector<std::array<float, 3>> Emitted;
  std::string MaterialKind;

  int TransmissionBounces = 0;

  int OracleSamples = 1;

  std::map<std::string, std::string> Reductions;

  ColourFrom Colour = ColourFrom::Declared;
  ColourCarrier Carrier = ColourCarrier::Texture;
  bool MaterialFromFile() const { return Colour != ColourFrom::Declared; }

  bool ShadedByLights() const { return Colour == ColourFrom::Row; }
  SceneLights Lights = SceneLights::None;

  outshine::PunctualLight Sun;

  bool DeltaLit = false;

  double WorldRadiance[3] = {kFactoryWorldRadiance, kFactoryWorldRadiance, kFactoryWorldRadiance};

  std::vector<Invariant> Invariants;

  std::string VariantName;
  SurfaceTable Surfaces;

  int Frames = 1;
  double Fps = 0;

  std::vector<int> Animations;
  outshine::Gltf::Pose Animation;

  std::vector<Transform> Locals;

  std::vector<double> Weights;

  std::vector<double> RestPositions;

  std::vector<double> PreviousPositions;

  double MovedPx = 0;

  std::set<std::string> MetricsReported;

  [[nodiscard]] bool Posed() const { return !Animations.empty(); }

  [[nodiscard]] bool Animated() const { return Frames > 1; }

  // WHETHER A CASE IS A SEQUENCE IS DECLARED, NOT COUNTED. `test/harness/shared/corpus/prep/
  // manifest.py:95` suffixes every product with its frame when `scene.animation` is present --
  // `if self.scene.animation is None: return [None]` -- and it does that whether the grid holds
  // one frame or forty. This reader used to ask `Frames > 1` instead, which is a SECOND spelling
  // of the same question and disagrees with the first on exactly one input: a declared animation
  // sampled at a single frame. The preparer wrote `oracle.f0000.exr`, the reader asked for
  // `oracle.exr`, and both generator cases with `frames: 1` sat UNPREPARED in every gate run --
  // green nowhere, red in a count nobody could name.
  bool Sequenced = false;

  [[nodiscard]] std::optional<int> ProductFrame(int frame) const {
    return Sequenced ? std::optional<int>(frame) : std::nullopt;
  }

  PathContents Path;
};

std::string Slurp(const std::string &path) {
  std::FILE *file = std::fopen(path.c_str(), "rb");
  if (!file) { return std::string(); }
  std::string text;
  char block[1 << 16];
  for (size_t read = std::fread(block, 1, sizeof block, file); read > 0;
       read = std::fread(block, 1, sizeof block, file)) {
    text.append(block, read);
  }
  std::fclose(file);
  return text;
}

void Refused(const std::string &why) { std::printf("REFUSED %s\n", why.c_str()); }

bool Reduced(const Case &subject) {
  return subject.MaterialKind == "emission" || subject.MaterialKind == "emission-per-material" ||
         subject.MaterialKind == "emission-by-material-index" ||
         subject.DeltaLit;
}

[[nodiscard]] bool ReadColourFrom(const Json::Ref &declared, ColourFrom &out, std::string &error) {
  if (declared.StrEquals("gltf-base-colour")) {
    out = ColourFrom::BaseColour;
    return true;
  }
  if (declared.StrEquals("gltf-emissive")) {
    out = ColourFrom::Emissive;
    return true;
  }
  if (declared.StrEquals("gltf")) {
    out = ColourFrom::Row;
    return true;
  }
  if (declared.StrEquals("manifest") || !declared.Valid()) {
    out = ColourFrom::Declared;
    return true;
  }
  error = "scene.material.source is '" + declared.Str("") +
          "', and this runner has no arm for it";
  return false;
}

[[nodiscard]] bool ReadColourCarrier(const Json::Ref &declared, ColourCarrier &out,
                                         std::string &error) {
  if (declared.StrEquals("texture")) {
    out = ColourCarrier::Texture;
    return true;
  }
  if (declared.StrEquals("factor")) {
    out = ColourCarrier::Factor;
    return true;
  }
  if (declared.StrEquals("vertex-colour")) {
    out = ColourCarrier::VertexColour;
    return true;
  }
  error = "scene.material.carriedBy is '" + declared.Str("") + "', and this runner has no arm for it";
  return false;
}

[[nodiscard]] bool ReadSceneLights(const Json::Ref &declared, SceneLights &out,
                                   std::string &error) {
  if (declared.StrEquals("gltf")) {
    out = SceneLights::FromFile;
    return true;
  }
  if (declared.StrEquals("none") || !declared.Valid()) {
    out = SceneLights::None;
    return true;
  }

  if (declared.StrEquals("sun")) {
    out = SceneLights::DeclaredSun;
    return true;
  }
  error = "scene.light.kind is '" + declared.Str("") +
          "', and this runner builds the file's own lights ('gltf'), a declared 'sun', or none -- "
          "a 'point' declared beside the asset reaches the oracle and has no path into the studio";
  return false;
}

[[nodiscard]] bool ReadDeclaredSun(const Json::Ref &declared, outshine::PunctualLight &out,
                                   std::string &error) {
  const double angle = declared["angleRad"].Num(-1.0);
  if (angle != 0.0) {
    error = "scene.light is a sun of angular diameter " + std::to_string(angle) +
            " rad, and this runner builds a light with no area -- an angle above zero is a disc, "
            "which is an integral in the oracle and a terminator no punctual light draws";
    return false;
  }
  double beam[3] = {0, 0, 0};
  double length = 0;
  for (size_t axis = 0; axis < 3; ++axis) {
    beam[axis] = declared["directionM"][axis].Num(0.0);
    length += beam[axis] * beam[axis];
  }
  length = std::sqrt(length);
  if (!(length > 0)) {
    error = "scene.light declares a sun whose direction has zero length";
    return false;
  }
  const double irradiance = declared["irradianceWPerM2"].Num(-1.0);
  if (!(irradiance > 0)) {
    error = "scene.light declares a sun of irradiance " + std::to_string(irradiance) +
            " W/m^2, and a light that delivers nothing lights nothing";
    return false;
  }
  out = outshine::PunctualLight{};
  out.Kind = outshine::LightKind::Directional;
  out.Intensity = (float)irradiance;
  for (size_t channel = 0; channel < 3; ++channel) {
    out.Colour[channel] = (float)declared["colourLinear"][channel].Num(1.0);
  }
  for (size_t axis = 0; axis < 3; ++axis) { out.Direction[axis] = (float)(beam[axis] / length); }
  return true;
}

class RunnerLog : public outshine::LogSink {
public:
  void Write(double, outshine::LogLevel level, const char *, const char *tag,
             const char *event,
             std::span<const outshine::LogField> fields) override {
    if (level < outshine::LogLevel::Info) { return; }
    std::printf("LOG %s %s", tag, event);
    for (const outshine::LogField &field : fields) {
      std::printf(" %s=%s", field.Key, field.Value.c_str());
    }
    std::printf("\n");
  }
};

// THE DOOR'S CAMERA, READ BACK INTO THE SHAPE THIS RUNNER PROJECTS WITH. The two say the same
// standing -- an eye, a point it looks at, which way is up, and a projection -- and the door's is
// the one a client holds.
[[nodiscard]] bool ViewpointOf(const outshine::Camera &from, Viewpoint &out) {
  const double aim[3] = {from.LookAtM[0], from.LookAtM[1], from.LookAtM[2]};
  if (!Viewpoint::LookAt(from.Stands.AtM, aim, from.UpM, out)) { return false; }
  out.ZNearM = from.NearM;
  out.ZFarM = from.FarM;
  if (from.Orthographic) {
    out.Kind = outshine::Render::CameraKind::Orthographic;
    out.XMagM = from.XMagM;
    out.YMagM = from.YMagM;
  } else {
    out.YfovRad = from.FovDeg * std::numbers::pi / 180.0;
  }
  return true;
}

[[nodiscard]] bool ResolveCamera(Case &subject, std::string &error) {
  const Json::Ref declared = subject.Manifest.Root()["scene"]["camera"];
  if (declared["source"].StrEquals("manifest")) {
    double eye[3] = {0, 0, 0}, aim[3] = {0, 0, 0};
    for (size_t axis = 0; axis < 3; ++axis) {
      eye[axis] = declared["positionM"][axis].Num(0.0);
      aim[axis] = declared["lookAtM"][axis].Num(0.0);
    }
    if (!Viewpoint::LookAt(eye, aim, declared["rollRad"].Num(0.0), subject.Eye)) {
      error = "the manifest's camera aims at its own eye or straight up";
      return false;
    }
    subject.Eye.ZNearM = declared["clipStartM"].Num(0.0);
    subject.Eye.ZFarM = declared["clipEndM"].Num(0.0);
    subject.CameraSource = "manifest";

    if (declared["projection"].StrEquals("orthographic")) {
      subject.Eye.Kind = outshine::Render::CameraKind::Orthographic;
      subject.Eye.YMagM = declared["yMagM"].Num(0.0);

      subject.Eye.XMagM = subject.Eye.YMagM * subject.Frame.Aspect();
      return subject.Eye.YMagM > 0;
    }
    if (declared["projection"].Valid() && !declared["projection"].StrEquals("perspective")) {
      error = "the manifest's camera declares projection '" + declared["projection"].Str("") +
              "', and glTF has two";
      return false;
    }
    subject.Eye.YfovRad = declared["yfovRad"].Num(0.0);
    return subject.Eye.YfovRad > 0;
  }

  if (declared["source"].StrEquals("derived")) {
    const std::string provenanceText = Slurp(subject.Directory + "provenance.json");
    Json provenance;
    if (provenanceText.empty() ||
        !provenance.Parse(provenanceText.c_str(), provenanceText.size())) {
      error = subject.Directory +
              "provenance.json is absent or does not parse, and a derived camera is what the "
              "preparer published there";
      return false;
    }

    Json::Ref from;
    const Json::Ref rows = provenance.Root()["report"]["render"];
    for (size_t row = 0; row < rows.Size(); ++row) {
      const Json::Ref candidate = rows[row]["provenance"]["camera"]["derivedFrom"];
      if (candidate.Valid()) {
        from = candidate;
        break;
      }
    }
    if (!from.Valid()) {
      error = subject.Directory +
              "provenance.json carries no camera.derivedFrom, so this case was prepared before the "
              "framing rule was derived where the bounds are";
      return false;
    }
    double eye[3] = {0, 0, 0}, aim[3] = {0, 0, 0};
    for (size_t axis = 0; axis < 3; ++axis) {
      eye[axis] = from["positionM"][axis].Num(0.0);
      aim[axis] = from["lookAtM"][axis].Num(0.0);
    }
    if (!Viewpoint::LookAt(eye, aim, from["rollRad"].Num(0.0), subject.Eye)) {
      error = "the derived camera aims at its own eye or straight up";
      return false;
    }
    subject.Eye.ZNearM = from["clipStartM"].Num(0.0);
    subject.Eye.ZFarM = from["clipEndM"].Num(0.0);
    subject.Eye.YfovRad = from["yfovRad"].Num(0.0);
    subject.CameraSource = "derived";
    return subject.Eye.YfovRad > 0 && subject.Eye.ZFarM > subject.Eye.ZNearM;
  }

  if (declared["source"].StrEquals("gltf")) {
    if (declared["index"].GetKind() != Json::Kind::Number) {
      error = "the manifest names the glTF as the camera's source and declares no `index` into its "
              "`cameras`, and a file may carry more than one";
      return false;
    }
    outshine::Camera shipped;
    if (!subject.Held.camera((int)declared["index"].Num(-1), shipped) ||
        !ViewpointOf(shipped, subject.Eye)) {
      error = "the manifest names a camera of the file that it does not carry, or one that aims "
              "at its own eye";
      return false;
    }
    subject.CameraSource = "gltf";
    return true;
  }

  outshine::Camera framed;
  if (subject.Held.frames(framed) && ViewpointOf(framed, subject.Eye)) {
    subject.CameraSource = "framing-rule";
    return true;
  }
  error = "the subject has degenerate bounds, so the framing rule refuses -- a fallback camera here "
          "would manufacture exactly the empty picture the guard exists to catch";
  return false;
}

[[nodiscard]] bool ReadOraclePath(const Json::Ref &light, PathContents &path, std::string &error) {
  path.OracleEstimates = light["estimator"].StrEquals("selected");
  path.OracleIsHostIrreproducible = light["estimator"].StrEquals("not-bit-reproducible");
  if (!path.OracleIsHostIrreproducible) { return true; }
  if (!ReadDeclaredNumber(light["hostResidueRelative"], "scene.light.hostResidueRelative",
                          path.OracleHostResidueRelative, error)) {
    return false;
  }
  if (light["hostResidueRelative"]["origin"].Str("") != "measured") {
    error = "scene.light.hostResidueRelative is not measured, and the host's own irreproducibility "
            "is not a quantity anything can derive";
    return false;
  }
  return true;
}

[[nodiscard]] bool ReadDisplayTransfer(const Json::Ref &recipe, std::string &error) {
  const Json::Ref colour = recipe["colourManagement"];
  const bool standard = colour["viewTransform"].StrEquals("Standard") &&
                        colour["displayDevice"].StrEquals("sRGB") &&
                        colour["look"].StrEquals("None") && colour["exposure"].Num(1.0) == 0.0 &&
                        colour["gamma"].Num(0.0) == 1.0 && recipe["filmExposure"].Num(0.0) == 1.0;
  if (standard) { return true; }
  error = "the recipe's colour management is not the sRGB transfer at unit exposure, and the "
          "picture bound is computed on the transfer the case declares -- this runner implements "
          "that one only";
  return false;
}

[[nodiscard]] bool ReadManifest(Case &subject, std::string &error) {
  const std::string manifestText = Slurp(subject.Directory + "manifest.json");
  if (manifestText.empty()) {
    error = subject.Directory + "manifest.json is absent or empty";
    return false;
  }
  if (!subject.Manifest.Parse(manifestText.c_str(), manifestText.size())) {
    error = subject.Directory + "manifest.json stopped parsing at byte " +
            std::to_string(subject.Manifest.StoppedAt());
    return false;
  }

  const std::string schemaPath = SchemaPathBesideCase(subject.Directory);
  const std::string schemaText = Slurp(schemaPath);
  if (schemaText.empty()) {
    error = schemaPath + " is absent or empty, and it is what says whether this manifest is one";
    return false;
  }
  ManifestSchema schema;
  if (!schema.Load(schemaText, error)) { return false; }
  if (!schema.Check(subject.Manifest.Root(), error)) { return false; }
  const Json::Ref root = subject.Manifest.Root();
  if (!ReadSubjectClass(root["subjectClass"].Str(""), subject.Accepted.Subject, error)) {
    return false;
  }
  if (!ReadExactnessClass(root, subject.Placement, error)) { return false; }

  subject.TransmissionBounces =
      root["renders"]["default"]["bounces"]["transmission"].Int(0);
  subject.OracleSamples = root["renders"]["default"]["samples"].Int(1);
  const Json::Ref reductions = root["reductions"];
  for (size_t at = 0; at < reductions.Size(); ++at) {
    const std::string metric = reductions[at]["metric"].Str("");
    const std::string because = reductions[at]["because"].Str("");
    if (metric.empty() || because.empty()) {
      error = "reductions[" + std::to_string(at) +
              "] names a metric and a reason, and a reduction with no reason is a disqualification "
              "wearing a softer word";
      return false;
    }
    subject.Reductions[metric] = because;
  }
  const Json::Ref filterWidth = root["renders"]["default"]["pixelFilter"]["widthPx"];
  if (filterWidth.GetKind() != Json::Kind::Number || filterWidth.Num() <= 0.0) {
    error = "renders.default.pixelFilter.widthPx is absent or not positive, and half of it is the "
            "oracle's own sub-pixel resolution -- the floor every near-tie here is judged against";
    return false;
  }
  subject.OracleFloorPx = 0.5 * filterWidth.Num();

  if (!ReadCriterionKind(root["criterion"]["kind"].Str(""), subject.Criterion, error)) {
    return false;
  }
  if (root["criterion"]["says"].Str("").empty() ||
      root["criterion"]["statedAt"].Str("").empty()) {
    error = "criterion states no `says` or no `statedAt`, so the acceptance is ours and not the "
            "asset's";
    return false;
  }

  const bool statesInvariants = root["statedInvariants"].Size() > 0;
  const bool mayStateInvariants = subject.Criterion == CriterionKind::StatedInvariant ||
                                  subject.Criterion == CriterionKind::SelfDescribing;
  if (statesInvariants && !mayStateInvariants) {
    error = "the manifest declares statedInvariants and its criterion.kind is neither "
            "stated-invariant nor self-describing";
    return false;
  }
  if (!statesInvariants && subject.Criterion == CriterionKind::StatedInvariant) {
    error = "criterion.kind is stated-invariant and the manifest declares no statedInvariants";
    return false;
  }
  if (statesInvariants && !ReadInvariants(root["statedInvariants"], subject.Invariants, error)) {
    return false;
  }
  if (subject.Criterion == CriterionKind::SelfDescribing &&
      !ReadOracleRole(root["criterion"], subject.Oracle, error)) {
    return false;
  }
  subject.Accepted.BoundaryP95MaxPx = DefaultBoundaryP95Px(subject.Accepted.Subject);
  subject.Accepted.EnforceBoundary = subject.Accepted.Subject == SubjectClass::OpaqueAtLeastOnePixel;
  if (!ReadAcceptance(root["acceptance"], subject.Accepted, error)) { return false; }

  const Json::Ref material = root["scene"]["material"];
  if (!ReadColourFrom(material["source"], subject.Colour, error)) { return false; }
  subject.MaterialKind = material["kind"].Str("");
  if ((subject.Colour == ColourFrom::BaseColour || subject.Colour == ColourFrom::Emissive) &&
      !ReadColourCarrier(material["carriedBy"], subject.Carrier, error)) {
    return false;
  }

  const Json::Ref variant = root["scene"]["materialVariant"];
  if (variant.Valid()) { subject.VariantName = variant.Str(""); }

  const Json::Ref animation = root["scene"]["animation"];
  if (animation.Valid()) {
    const Json::Ref which = animation["animations"];
    for (size_t at = 0; at < which.Size(); ++at) { subject.Animations.push_back(which[at].Int(0)); }
    double fps = 0, frames = 0;
    if (!ReadDeclaredNumber(animation["fps"], "scene.animation.fps", fps, error)) { return false; }
    if (!ReadDeclaredNumber(animation["frames"], "scene.animation.frames", frames, error)) {
      return false;
    }

    if (!(fps > 0) || !(frames >= 1)) {
      error = "scene.animation declares " + std::to_string(frames) + " frames at " +
              std::to_string(fps) + " fps, and a grid is at least one frame on a positive rate";
      return false;
    }
    subject.Fps = fps;
    subject.Frames = (int)frames;
    subject.Sequenced = true;

    if (!root["scene"]["camera"]["source"].StrEquals("manifest") &&
        !root["scene"]["camera"]["source"].StrEquals("derived") &&
        !root["scene"]["camera"]["source"].StrEquals("gltf")) {
      error = "scene.animation declares a sequence and scene.camera.source is '" +
              root["scene"]["camera"]["source"].Str("") +
              "', so the framing rule would re-derive the camera from a different bounds at every "
              "frame and the camera would move with the subject";
      return false;
    }
  }
  const Json::Ref light = root["scene"]["light"];
  if (!ReadSceneLights(light["kind"], subject.Lights, error)) { return false; }
  if (subject.Lights == SceneLights::DeclaredSun && !ReadDeclaredSun(light, subject.Sun, error)) {
    return false;
  }

  subject.DeltaLit = light["estimator"].StrEquals("delta");
  if (!ReadOraclePath(light, subject.Path, error)) { return false; }
  if (!ReadDisplayTransfer(root["renders"]["default"], error)) { return false; }

  const Json::Ref world = root["scene"]["world"];
  if (world["kind"].StrEquals("uniform")) {
    const double strength = world["strength"].Num(0.0);
    for (size_t channel = 0; channel < 3; ++channel) {
      subject.WorldRadiance[channel] = world["colourLinear"][channel].Num(0.0) * strength;
    }
  } else if (world["kind"].StrEquals("factory") || world["kind"].Str("").empty()) {
    for (double &channel : subject.WorldRadiance) { channel = kFactoryWorldRadiance; }
  } else {
    error = "scene.world.kind is '" + world["kind"].Str("") +
            "', and this runner knows 'factory' and 'uniform'";
    return false;
  }

  const Json::Ref recipe = root["renders"]["default"];
  subject.Frame.WidthPx = recipe["resolutionX"].Num(0.0);
  subject.Frame.HeightPx = recipe["resolutionY"].Num(0.0);
  if (!(subject.Frame.WidthPx > 0) || !(subject.Frame.HeightPx > 0)) {
    error = "the manifest's default render recipe declares no resolution";
    return false;
  }

  return true;
}

bool Present(const std::string &path) {
  std::FILE *file = std::fopen(path.c_str(), "rb");
  if (!file) { return false; }
  std::fclose(file);
  return true;
}

std::string MissingInputs(const Case &subject) {
  std::vector<std::string> owed;
  const Json::Ref subjects = subject.Manifest.Root()["subjects"];
  for (size_t which = 0; which < subjects.Size(); ++which) {
    const Json::Ref files = subjects[which]["files"];
    for (size_t file = 0; file < files.Size(); ++file) {
      const std::string as = files[file]["as"].Str("");
      if (!as.empty() && !Present(subject.Directory + as)) { owed.push_back(as); }
    }
    const std::string entry = subjects[which]["entry"].Str("");
    if (!entry.empty() && !Present(subject.Directory + entry)) { owed.push_back(entry); }
  }
  const Json::Ref recipes = subject.Manifest.Root()["renders"];
  for (size_t which = 0; which < recipes.Size(); ++which) {
    for (int frame = 0; frame < subject.Frames; ++frame) {
      const std::string product =
          OracleProduct{"", recipes.Key(which), subject.ProductFrame(frame)}.Exr();
      if (!Present(subject.Directory + product)) { owed.push_back(product); }
    }
  }
  std::string missing;
  for (const std::string &name : owed) {
    if (missing.find(name) != std::string::npos) { continue; }
    if (!missing.empty()) { missing += ", "; }
    missing += name;
  }
  return missing;
}

[[nodiscard]] bool ResolveEmission(const Case &subject, const Handed &geometry,
                                   std::vector<std::array<float, 3>> &out, std::string &error) {
  const Json::Ref material = subject.Manifest.Root()["scene"]["material"];
  const size_t parts = geometry.Parts().size();
  out.assign(parts, {0.0f, 0.0f, 0.0f});

  if (subject.ShadedByLights()) {
    for (size_t part = 0; part < parts; ++part) {
      const int index = geometry.Parts()[part].Material;
      if (index < 0 || (size_t)index >= geometry.Surfaces().size()) { continue; }
      const outshine::Material &surface = geometry.Surfaces()[(size_t)index];
      if (!surface.Unlit) { continue; }
      for (size_t channel = 0; channel < 3; ++channel) {
        out[part][channel] = surface.BaseColour[channel];
      }
    }
    return true;
  }

  if (subject.MaterialFromFile()) {

    const bool emits = subject.MaterialKind == "emission";
    for (size_t part = 0; part < parts; ++part) {
      const int index = geometry.Parts()[part].Material;

      const outshine::Material surface = index >= 0 && (size_t)index < geometry.Surfaces().size()
                                             ? geometry.Surfaces()[(size_t)index]
                                             : outshine::Gltf::DefaultMaterial();
      for (size_t channel = 0; channel < 3; ++channel) {
        const double factor = subject.Colour == ColourFrom::Emissive
                                  ? (double)surface.Emission[channel]
                                  : (double)surface.BaseColour[channel];
        out[part][channel] =
            (float)(factor * (emits ? 1.0 : subject.WorldRadiance[channel]));
      }
    }
    return true;
  }

  if (subject.MaterialKind == "emission-per-material") {
    const Json::Ref declared = material["colourLinearPerMaterial"];
    std::vector<std::string> matched;
    for (size_t part = 0; part < parts; ++part) {
      const int index = geometry.Parts()[part].Material;

      static const std::string kDefaultMaterial = "<default>";
      if (index >= (int)geometry.Surfaces().size()) {
        error = "part " + std::to_string(part) + " names material " + std::to_string(index) +
                " and the file declares " + std::to_string(geometry.Surfaces().size());
        return false;
      }

      std::string name;
      if (index < 0) {
        name = kDefaultMaterial;
      } else if (!geometry.SurfaceNames()[(size_t)index].empty()) {
        name = geometry.SurfaceNames()[(size_t)index];
      } else {
        name = "Material_" + std::to_string(index);

        for (size_t other = 0; other < geometry.Surfaces().size(); ++other) {
          if (geometry.SurfaceNames()[other] == name) {
            error = "material " + std::to_string(index) + " names itself nothing and material " +
                    std::to_string(other) + " is spelled '" + name +
                    "', which is the key the importer gives the first";
            return false;
          }
        }
      }
      const Json::Ref colour = declared[name.c_str()];
      if (colour.Size() != 3) {
        error = "scene.material.colourLinearPerMaterial declares no colour for material '" + name +
                "'" + (name == kDefaultMaterial
                           ? " -- a primitive of this subject names none, and the format's default "
                             "material is keyed by that reserved name"
                           : "");
        return false;
      }
      for (size_t channel = 0; channel < 3; ++channel) {
        out[part][channel] = (float)colour[channel].Num(0.0);
      }
      if (std::find(matched.begin(), matched.end(), name) == matched.end()) {
        matched.push_back(name);
      }
    }
    if (declared.Size() != matched.size()) {
      error = "scene.material.colourLinearPerMaterial declares " + std::to_string(declared.Size()) +
              " colours over a subject that draws " + std::to_string(matched.size()) +
              " materials, so at least one names a material this subject does not draw";
      return false;
    }
    return true;
  }

  if (subject.MaterialKind == "emission-by-material-index") {
    static const double kStep[3] = {0.6180339887498949, 0.4142135623730951, 0.3027756377319946};
    for (size_t part = 0; part < parts; ++part) {
      const int index = geometry.Parts()[part].Material;
      if (index >= (int)geometry.Surfaces().size()) {
        error = "part " + std::to_string(part) + " names material " + std::to_string(index) +
                " and the file declares " + std::to_string(geometry.Surfaces().size());
        return false;
      }
      const double slot = index < 0 ? (double)geometry.Surfaces().size() : (double)index;
      for (size_t channel = 0; channel < 3; ++channel) {
        const double walked = slot * kStep[channel];
        out[part][channel] = (float)(0.15 + 0.70 * (walked - std::floor(walked)));
      }
    }
    return true;
  }

  if (subject.MaterialKind == "diffuse") {
    if (parts != 1) {
      error = "scene.material.kind is 'diffuse' over a subject of " + std::to_string(parts) +
              " mesh-bearing nodes, and the closed form rho*L holds only where no surface can see "
              "another -- a subject of several bodies is an emission case";
      return false;
    }
    for (size_t channel = 0; channel < 3; ++channel) {
      out[0][channel] =
          (float)(material["colourLinear"][channel].Num(0.0) * subject.WorldRadiance[channel]);
    }
    return true;
  }

  if (subject.MaterialKind != "emission") {
    error = "scene.material.kind is '" + subject.MaterialKind +
            "', and this runner knows 'diffuse', 'emission', 'emission-per-material' and 'emission-by-material-index'";
    return false;
  }

  const Json::Ref declared = material["colourLinearPerNode"];
  std::vector<std::string> matched;
  for (size_t part = 0; part < parts; ++part) {
    const std::string &name = geometry.Parts()[part].NodeName;
    if (name.empty()) {
      error = "the subject's part " + std::to_string(part) +
              " carries no node name, so a per-node colour has nothing to key on";
      return false;
    }
    const Json::Ref colour = declared[name.c_str()];
    if (colour.Size() != 3) {
      error = "scene.material.colourLinearPerNode declares no colour for node '" + name + "'";
      return false;
    }
    for (size_t channel = 0; channel < 3; ++channel) {
      out[part][channel] = (float)colour[channel].Num(0.0);
    }
    if (std::find(matched.begin(), matched.end(), name) == matched.end()) {
      matched.push_back(name);
    }
  }
  if (declared.Size() != matched.size()) {
    error = "scene.material.colourLinearPerNode declares " + std::to_string(declared.Size()) +
            " colours over a subject of " + std::to_string(matched.size()) +
            " named nodes, so at least one names a node this subject does not draw";
    return false;
  }
  return true;
}

// THE RUNNER BUILDS ITS OWN SURFACE TABLE, FROM THE DOOR. `ResolveSurfaceTable` and
// `ResolveFileSurface` are the engine's, take a `Gltf::Document` and a `Gltf::Subject`, and were
// the last thing keeping this file on those two names. What they decide is now readable: a
// `Material` NAMES its maps and a `Geometry` carries the images, so the table below is this
// runner PREDICTING what the engine will do -- which is what it already does for the slot split
// -- rather than borrowing the engine's answer and calling the agreement a measurement.
void SurfacesOf(const Handed &model, const outshine::Geometry &handed, bool carriesTransmission,
                bool ownMaterials, SurfaceTable &out) {
  out.Slots.clear();
  out.Material.clear();
  out.Decoded.clear();
  out.PartSlot.assign(model.Parts().size(), 0);
  const auto bound = [&handed](const outshine::SurfaceMap &map,
                               outshine::Render::SubjectTexture &into) {
    if (!map.bound()) { return; }
    const outshine::ImageView held = handed.imageAt(map.Image);
    if (!held.stands()) { return; }
    into.Rgba = held.Rgba.data();
    into.Width = (uint32_t)held.WidthPx;
    into.Height = (uint32_t)held.HeightPx;
    into.Set = map.Set;
    into.Uv = outshine::UvTransformOf(map.Uv);
    into.Magnify = map.Samples.Magnify == outshine::Filter::Nearest
                       ? outshine::Render::SubjectFilter::Nearest
                       : outshine::Render::SubjectFilter::Linear;
    into.Minify = map.Samples.Minify == outshine::Filter::Nearest
                      ? outshine::Render::SubjectFilter::Nearest
                      : outshine::Render::SubjectFilter::Linear;
    into.WrapU = map.Samples.WrapU == outshine::Wrap::ClampToEdge
                     ? outshine::Render::SubjectWrap::ClampToEdge
                 : map.Samples.WrapU == outshine::Wrap::MirroredRepeat
                     ? outshine::Render::SubjectWrap::MirroredRepeat
                     : outshine::Render::SubjectWrap::Repeat;
    into.WrapV = map.Samples.WrapV == outshine::Wrap::ClampToEdge
                     ? outshine::Render::SubjectWrap::ClampToEdge
                 : map.Samples.WrapV == outshine::Wrap::MirroredRepeat
                     ? outshine::Render::SubjectWrap::MirroredRepeat
                     : outshine::Render::SubjectWrap::Repeat;
  };
  for (size_t part = 0; part < model.Parts().size(); ++part) {
    const int material = model.Parts()[part].Material;
    size_t slot = out.Material.size();
    for (size_t at = 0; at < out.Material.size(); ++at) {
      if (out.Material[at] == material) {
        slot = at;
        break;
      }
    }
    if (slot == out.Material.size()) {
      outshine::Render::SubjectMaterial surface;
      if (material >= 0 && (size_t)material < model.Surfaces().size()) {
        surface.Row = model.Surfaces()[(size_t)material];
        if (!carriesTransmission) {
          surface.Row.Transmission = 0.0f;
          surface.Row.Thickness = 0.0f;
        }
        if (!ownMaterials) { surface.Row.Alpha = outshine::AlphaMode::Opaque; }
        bound(surface.Row.BaseColourMap, surface.Colour);
        bound(surface.Row.NormalMap, surface.Normal);
        bound(surface.Row.MetalRoughMap, surface.MetalRough);
        bound(surface.Row.EmissiveMap, surface.Emissive);
        bound(surface.Row.SpecularStrengthMap, surface.SpecularStrength);
        bound(surface.Row.SpecularTintMap, surface.SpecularTint);
      }
      out.Material.push_back(material);
      out.Slots.push_back(surface);
    }
    out.PartSlot[part] = (uint32_t)slot;
  }
  out.Decoded.assign(out.Slots.size(), SurfaceRasters{});
}

[[nodiscard]] bool AnyLinearFilteredImage(const SurfaceTable &surfaces) {
  const auto interpolates = [](const outshine::Render::SubjectTexture &image) {
    return image.Rgba != nullptr && (image.Width > 1u || image.Height > 1u) &&
           image.Magnify == outshine::Render::SubjectFilter::Linear;
  };
  for (const outshine::Render::SubjectMaterial &slot : surfaces.Slots) {
    if (interpolates(slot.SpecularStrength) || interpolates(slot.SpecularTint) ||
        interpolates(slot.Colour) || interpolates(slot.Normal) || interpolates(slot.MetalRough) ||
        interpolates(slot.Emissive)) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool PoseGeometry(Case &subject, int frame, std::string &error) {
  const double seconds = subject.Posed() && subject.Fps > 0.0 ? (double)frame / subject.Fps : 0.0;
  if (!subject.Held.poses(seconds)) {
    error = subject.Held.error();
    return false;
  }
  subject.Model.Reads(subject.Held.geometry());
  return true;
}

[[nodiscard]] bool BuildSubject(Case &subject, std::string &error) {
  const std::string entry =
      subject.Manifest.Root()["subjects"][size_t{0}]["entry"].Str("scene.gltf");
  if (!subject.Held.reads(subject.Directory + entry)) {
    error = subject.Held.error();
    return false;
  }
  if (!subject.VariantName.empty() && !subject.Held.wears(subject.VariantName)) {
    error = subject.Held.error();
    return false;
  }
  if (subject.Posed() && !subject.Held.plays(std::span<const int>(subject.Animations.data(),
                                                                 subject.Animations.size()))) {
    error = subject.Held.error();
    return false;
  }
  if (!PoseGeometry(subject, 0, error)) { return false; }
  subject.RestPositions = subject.Model.PositionsM();

  SurfacesOf(subject.Model, subject.Held.geometry(), subject.TransmissionBounces > 0,
             subject.MaterialFromFile(), subject.Surfaces);
  if (!ResolveEmission(subject, subject.Model, subject.Emitted, error)) {
    return false;
  }
  subject.Path.LinearFilteredSampler = AnyLinearFilteredImage(subject.Surfaces);
  return ResolveCamera(subject, error);
}

struct Picture {
  std::vector<float> Depth;
  std::vector<uint8_t> Rgba;

  std::vector<float> Linear;

  std::vector<float> ShadingNormal;

  std::vector<float> SurfaceIdentity;

  std::vector<float> Velocity;
};

outshine::Render::Eye MakeView(const Case &subject) {
  return outshine::Render::Eye{subject.Eye, false, 0};
}

// THE CAPTURE, THROUGH THE DOOR AND NOTHING ELSE. It brackets the frame the way Filament does and
// asks for each picture by name -- which is why `Buffer` and `readPixels(Buffer, ...)` went into
// `include/`: a conformance case states a claim about the DEPTH or the SHADING NORMAL, and a
// harness that could only read the displayed colour would have to reach past the door to say it.
//
// A PICTURE THE PLAN DOES NOT KEEP IS EMPTY AND NOT A FAILURE. The case declares which ones it
// wants through `Keeps`; asking for one it did not declare is the client's own error and reads as
// a refusal, so this only takes what was asked for.
[[nodiscard]] bool Capture(outshine::Engine &engine, const outshine::Test::Drives &drives,
                           Picture &out, std::string &why) {
  if (!drives.Renders(engine, why)) { return false; }
  outshine::Renderer draws = engine.renderer();
  const auto keeps = [&drives](const char *named) {
    for (const std::string &one : drives.Keeps) {
      if (one == named) { return true; }
    }
    return false;
  };
  const auto reads = [&](outshine::Buffer which, std::vector<float> &into) {
    if (!draws.readPixels(which, into)) {
      why = engine.error();
      return false;
    }
    return true;
  };
  if (!draws.readPixels(out.Rgba)) {
    why = engine.error();
    return false;
  }
  if (!reads(outshine::Buffer::Linear, out.Linear)) { return false; }
  if (keeps("sceneDepth") && !reads(outshine::Buffer::Depth, out.Depth)) { return false; }
  if (keeps("sceneShadingNormal") &&
      !reads(outshine::Buffer::ShadingNormal, out.ShadingNormal)) {
    return false;
  }
  if (keeps("sceneSurfaceIdentity") &&
      !reads(outshine::Buffer::SurfaceIdentity, out.SurfaceIdentity)) {
    return false;
  }
  if (keeps("sceneVelocity") && !reads(outshine::Buffer::Velocity, out.Velocity)) { return false; }
  return true;
}

Mask FromDepth(const std::vector<float> &depth, const std::vector<float> &linear, int width,
               int height) {
  Mask mask;
  mask.Width = width;
  mask.Height = height;
  mask.In.resize(depth.size());
  const bool carriesAlpha = linear.size() >= depth.size() * 4u;
  for (size_t pixel = 0; pixel < depth.size(); ++pixel) {
    const bool blended = carriesAlpha && linear[pixel * 4u + 3u] > 0.0f;
    mask.In[pixel] = depth[pixel] > 0.0f || blended;
  }
  return mask;
}

Mask FromOracle(const RawF32 &oracle) {
  Mask mask;
  mask.Width = oracle.Width();
  mask.Height = oracle.Height();
  mask.In.resize((size_t)mask.Width * (size_t)mask.Height);
  for (int y = 0; y < mask.Height; ++y) {
    for (int x = 0; x < mask.Width; ++x) {
      mask.In[(size_t)y * (size_t)mask.Width + (size_t)x] =
          oracle.At(x, y, oracle.Channels() - 1) >= 0.5f;
    }
  }
  return mask;
}

uint8_t Srgb8(double linear) {
  const double clamped = linear < 0.0 ? 0.0 : (linear > 1.0 ? 1.0 : linear);
  const double encoded = clamped <= 0.0031308 ? 12.92 * clamped
                                              : 1.055 * std::pow(clamped, 1.0 / 2.4) - 0.055;
  const double level = encoded * 255.0 + 0.5;
  return (uint8_t)(level > 255.0 ? 255.0 : level);
}

std::vector<uint8_t> Encoded(const RawF32 &oracle) {
  std::vector<uint8_t> rgba((size_t)oracle.Width() * (size_t)oracle.Height() * 4u);
  for (int y = 0; y < oracle.Height(); ++y) {
    for (int x = 0; x < oracle.Width(); ++x) {
      const size_t at = ((size_t)y * (size_t)oracle.Width() + (size_t)x) * 4u;
      for (int channel = 0; channel < 3; ++channel) {
        rgba[at + (size_t)channel] = Srgb8(oracle.At(x, y, channel));
      }
      const double coverage = oracle.At(x, y, oracle.Channels() - 1);
      rgba[at + 3u] = (uint8_t)(coverage <= 0.0 ? 0.0 : (coverage >= 1.0 ? 255.0 : coverage * 255.0 + 0.5));
    }
  }
  return rgba;
}

void ScoreDeterminism(const Case &subject, outshine::Engine &engine, const Picture &picture,
                      std::vector<Metric> &metrics) {
  using namespace outshine::Test;
  Picture again;
  std::string trouble;
  const bool twice = Capture(engine, subject.Driving, again, trouble);
  CHECK(twice, "the same declaration renders a second time in the same process");
  size_t apart = 0;
  int64_t worst = 0;
  size_t firstAt = again.Linear.size();
  if (twice && again.Linear.size() == picture.Linear.size()) {
    for (size_t at = 0; at < again.Linear.size(); ++at) {
      if (again.Linear[at] == picture.Linear[at]) { continue; }
      const int64_t off = UlpsApart(again.Linear[at], picture.Linear[at]);
      if (off > worst) { worst = off; }
      if (apart == 0) { firstAt = at; }
      ++apart;
    }
  }
  metrics.push_back({"linear_channels_differing_between_renders", (double)apart, 0.0, "channels",
                     Direction::AtMost});
  if (apart > 0) {
    Note("first differing channel, at index", (double)firstAt, "index");
    Note("widest disagreement between two renders", (double)worst, "f32 ulps");
    Picture third;
    if (Capture(engine, subject.Driving, third, trouble) &&
        third.Linear.size() == picture.Linear.size()) {
      size_t stable = 0;
      for (size_t at = 0; at < third.Linear.size(); ++at) {
        if (third.Linear[at] != picture.Linear[at]) { ++stable; }
      }
      Note("a third render differs from the first in", (double)stable, "halves");
    }
  }
}

void ScoreRadianceResidual(const Case &subject, const Picture &picture, const RawF32 &oracle,
                           std::vector<Metric> &metrics) {
  using namespace outshine::Test;
  const RadianceResidual radiance = Radiance(picture.Linear, oracle);
  metrics.push_back({"linear_channels_differing", (double)radiance.Differing, 0.0, "channels",
                     subject.MaterialFromFile() && subject.Criterion == CriterionKind::Numeric
                         ? Direction::AtMost
                         : Direction::Reported});
  metrics.push_back({"linear_channels_compared", (double)radiance.Compared, 0.0, "channels",
                     Direction::Reported});
  metrics.push_back({"linear_channels_beyond_one_ulp", (double)radiance.BeyondOneUlp, 0.0,
                     "channels", Direction::Reported});
  metrics.push_back({"linear_channels_below_the_oracle", (double)radiance.BelowOracle, 0.0,
                     "channels", Direction::Reported});
  metrics.push_back({"linear_worst_ulps", (double)radiance.WorstUlps, 0.0, "f32 ulps",
                     Direction::Reported});
  metrics.push_back({"linear_p50_relative", radiance.P50Relative, 0.0, "dimensionless",
                     Direction::Reported});
  metrics.push_back({"linear_p95_relative", radiance.P95Relative, 0.0, "dimensionless",
                     Direction::Reported});
  metrics.push_back({"linear_p99_relative", radiance.P99Relative, 0.0, "dimensionless",
                     Direction::Reported});
  if (radiance.Differing > 0) {
    Note("worst radiance disagreement, ours", radiance.WorstOurs, "linear, scene-referred");
    Note("worst radiance disagreement, oracle", radiance.WorstTheirs, "linear, scene-referred");
    Note("worst radiance disagreement, relative", radiance.WorstRelative, "dimensionless");
    Note("worst radiance disagreement, at x", (double)radiance.WorstX, "px");
    Note("worst radiance disagreement, at y", (double)radiance.WorstY, "px");
    Note("worst radiance disagreement, channel", (double)radiance.WorstChannel, "index");
  }
}

enum class Prepared { Yes, No };

void DeclarePlan(const Case &subject, outshine::Render::PlanSpec &declaration) {

  declaration.Outputs = {outshine::Render::Resource::SceneDepth,
                         outshine::Render::Resource::SceneShadingNormal,
                         outshine::Render::Resource::SceneSurfaceIdentity,
                         outshine::Render::Resource::FrameTex};

  if (subject.Animated()) {
    declaration.Outputs.push_back(outshine::Render::Resource::SceneVelocity);
  }
  declaration.Content = {outshine::Render::Stage::Subjects};

  bool carriesGlass = false;
  for (const outshine::Material &material : subject.Model.Surfaces()) {
    const outshine::SurfaceKind kind = outshine::StateOf(material).Kind();
    carriesGlass = carriesGlass || kind == outshine::SurfaceKind::ThinTransmissive ||
                   kind == outshine::SurfaceKind::Refractive;
  }
  carriesGlass = carriesGlass && subject.TransmissionBounces > 0;
  if (carriesGlass) {
    declaration.Content.push_back(outshine::Render::Stage::SubjectsTransmissive);
    declaration.Content.push_back(outshine::Render::Stage::CompositeTransmission);
  }
  declaration.Display =
      outshine::Render::Declared<outshine::Render::Transfer>(outshine::Render::Transfer::Linear);
  declaration.Exposure = outshine::Render::Declared<float>(1.0f);

  declaration.Precision = outshine::Render::Declared<outshine::Render::ScenePrecision>(
      outshine::Render::ScenePrecision::Float);
}

[[nodiscard]] bool ReadOracle(const Case &subject, int frame, RawF32 &oracle, size_t &seedApart) {
  using namespace outshine::Test;
  const std::optional<int> which = subject.ProductFrame(frame);
  const std::string picture = OracleProduct{"", "default", which}.Exr();
  const bool haveOracle = oracle.ReadExrFile(subject.Directory + picture);
  CHECK(haveOracle, "the cached oracle is present and decodes as the float image of this frame");
  if (!haveOracle) {
    Refused(oracle.Error());
    return false;
  }
  const bool sameFrame = oracle.Width() == (int)subject.Frame.WidthPx &&
                         oracle.Height() == (int)subject.Frame.HeightPx;
  CHECK(sameFrame, "the oracle was rendered at the resolution the manifest's recipe declares");
  if (!sameFrame) {
    Refused(picture + " is " + std::to_string(oracle.Width()) + "x" +
            std::to_string(oracle.Height()) + " and the recipe declares " +
            std::to_string((int)subject.Frame.WidthPx) + "x" +
            std::to_string((int)subject.Frame.HeightPx));
    return false;
  }

  seedApart = 0;
  if (!Reduced(subject)) { return true; }
  const std::string second = OracleProduct{"", "seed-shift", which}.Exr();
  RawF32 shifted;
  const bool haveShift = shifted.ReadExrFile(subject.Directory + second);
  CHECK(haveShift, "the emission case carries a second oracle rendered at another seed");
  if (!haveShift) {
    Refused(shifted.Error());
    return false;
  }
  const bool sameShape = shifted.Width() == oracle.Width() &&
                         shifted.Height() == oracle.Height() &&
                         shifted.Channels() == oracle.Channels();
  CHECK(sameShape, "the two seeds were rendered into the same frame");
  if (!sameShape) {
    Refused(second + " is not the shape " + picture + " is");
    return false;
  }
  for (int y = 0; y < oracle.Height(); ++y) {
    for (int x = 0; x < oracle.Width(); ++x) {
      for (int channel = 0; channel < oracle.Channels(); ++channel) {
        if (oracle.At(x, y, channel) != shifted.At(x, y, channel)) { ++seedApart; }
      }
    }
  }
  return true;
}

// THE CASE, SAID TO THE DOOR. Every number here was already read out of the manifest; this is
// where it stops being an argument to an internal call and becomes a DECLARATION. What the engine
// is told is exactly what a client could tell it, which is the whole of board:2038.
void DeclareDrives(Case &subject) {
  outshine::Test::Drives &says = subject.Driving;
  says.Path = subject.Manifest.Root()["subjects"][size_t{0}]["entry"].Str("scene.gltf");
  says.Frame = outshine::Extent{(int)subject.Frame.WidthPx, (int)subject.Frame.HeightPx};
  says.Animated = subject.Animated();
  says.Fps = subject.Fps;

  for (int axis = 0; axis < 3; ++axis) {
    says.AtM[axis] = subject.Eye.EyeM[axis];
    says.LookAtM[axis] = subject.Eye.EyeM[axis] + subject.Eye.Forward[axis];
  }
  says.YfovRad = subject.Eye.YfovRad;
  says.NearM = subject.Eye.ZNearM;
  says.FarM = subject.Eye.ZFarM;

  // AND WHICH WAY IS UP, HANDED OVER RATHER THAN RECOVERED. An eye and a look-at point say where
  // a camera points and NOTHING about which way is up, so a rolled camera came through the door
  // upright. The door takes the vector now, the way Filament's `Camera::lookAt` does, so there is
  // no angle to recover and no sense to guess -- the guess came out negated, and Khronos's
  // Triangle shared 48% of its pixels with the oracle until this.
  for (int axis = 0; axis < 3; ++axis) { says.UpM[axis] = subject.Eye.Up[axis]; }

  // AND THE PROJECTION. glTF has two cameras and so does this door; a case that declares an
  // orthographic one was being drawn in perspective, which is the largest coverage difference
  // available.
  says.Orthographic = subject.Eye.Kind == outshine::Render::CameraKind::Orthographic;
  says.XMagM = subject.Eye.XMagM;
  says.YMagM = subject.Eye.YMagM;

  // THE CASE'S OWN STAGES, NAMED. Leaving the list empty means "the engine's own default", and the
  // engine's default is a WORLD -- subjects, overlay, sky, aerial perspective, shadows. A corpus
  // case renders a subject against a reference with none of that, and an unnamed default drew all
  // of it: measured, `plan_passes` over its bound and the picture 104.7 px away from the oracle.
  says.Keeps = {"sceneDepth", "sceneShadingNormal", "sceneSurfaceIdentity"};
  if (subject.Animated()) { says.Keeps.push_back("sceneVelocity"); }
  says.Transfer = "linear";
  says.Precision = "float";
  says.Exposure = 1.0;

  // THE KEY LIGHT'S DIRECTION, TRANSLATED ONCE AT THE BOUNDARY. A case declares its sun as a
  // glTF-shaped VECTOR and the door spells the key as an ELEVATION and a BEARING, because that is
  // what a sun placed by a clock and a place has. Handing over only the intensity left every such
  // case lit from elevation 0, bearing 0 -- six of the corpus's `light: sun` cases, all red on the
  // picture. The engine builds the vector as (-cos el sin b, -sin el, -cos el cos b); this is that
  // read backwards, and nothing else here needs to know either spelling.
  if (subject.Lights == SceneLights::DeclaredSun) {
    says.Key.Lux = (double)subject.Sun.Intensity;
    const double dx = (double)subject.Sun.Direction[0];
    const double dy = (double)subject.Sun.Direction[1];
    const double dz = (double)subject.Sun.Direction[2];
    const double span = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (span > 1.0e-12) {
      says.Key.ElevationDeg = std::asin(std::clamp(-dy / span, -1.0, 1.0)) * 180.0 /
                              std::numbers::pi;
      says.Key.BearingDeg = std::atan2(-dx, -dz) * 180.0 / std::numbers::pi;
    }
  }
  // ONLY A CASE SHADED BY LIGHTS DECLARES AN AMBIENT. A case whose oracle is a Diffuse BSDF under
  // a uniform world has its rho*L computed HERE and handed over as the part's emission, so an
  // ambient on top of it would light the same energy twice -- measured on BoxInterleaved, whose
  // red face read 0.04326 against the oracle's 0.04070 with a grey 0.01279 in G and B that a
  // diffuse-only oracle cannot have.
  if (subject.ShadedByLights()) {
    for (int channel = 0; channel < 3; ++channel) {
      says.IndirectLight[channel] = subject.WorldRadiance[channel];
    }
  }

  // THE EMITTED RADIANCE, SAID PER PART, which is what a flat-shaded case's appearance IS.
  // ONLY A CASE THIS RUNNER SHADES ITSELF DECLARES ONE. An override REPLACES the surface, maps
  // and all, so quoting the file's own row back at the engine would strip the very textures a
  // light-shaded case is being scored on.
  for (size_t part = 0; !subject.ShadedByLights() && part < subject.Emitted.size() &&
                        part < subject.Model.Parts().size();
       ++part) {
    outshine::SurfaceOverride said;
    said.Node = subject.Model.Parts()[part].NodeName;
    said.Part = (int)part;
    // WHAT IS DECLARED IS A SHADING, AND ONLY WHAT DECIDES COVERAGE SURVIVES THE FILE. The
    // preparer replaces the material with a Diffuse or Emission BSDF, so a row carried over from
    // the asset kept a transmission the oracle does not render -- 21 cases whose manifest calls
    // them `opaque-min-1px` drew glass. Alpha mode, its cut and double-sidedness decide WHICH
    // PIXELS are covered rather than what colour they wear, and coverage agrees to five digits
    // today, so those three come across and nothing else does.
    const int index = subject.Model.Parts()[part].Material;
    const outshine::Material carried =
        index >= 0 && (size_t)index < subject.Model.Surfaces().size()
            ? subject.Model.Surfaces()[(size_t)index]
            : outshine::Material{};
    // A CASE WHOSE COLOUR IS THE FILE'S KEEPS THE FILE'S MAPS. `gltf-base-colour` and
    // `gltf-emissive` say "the asset states this surface and the oracle shades it flat", so the
    // base-colour map IS the picture and dropping it renders a flat average of a photograph. A
    // case whose colour the MANIFEST states keeps nothing: the preparer replaced the material with
    // a Diffuse or Emission BSDF and there is no map left on that side to agree with.
    said.KeepsMaps = subject.MaterialFromFile();
    said.Row = said.KeepsMaps ? carried : outshine::Material{};
    said.Row.Alpha = carried.Alpha;
    said.Row.CoverageCut = carried.CoverageCut;
    said.Row.DoubleSided = carried.DoubleSided;
    said.Row.BaseColour[3] = carried.BaseColour[3];
    said.Row.Unlit = !subject.ShadedByLights();
    for (int channel = 0; channel < 3; ++channel) {
      said.Row.Emission[channel] = subject.Emitted[part][(size_t)channel];
    }
    says.Surfaces.push_back(said);
  }

  // A DECLARATION THAT SPLITS SLOTS HAS TO MODEL THE SPLIT. A surface declared per PART gives that
  // part its own slot, so the engine's table grows by one for each -- and the identity attachment
  // then names slots this harness's own table has never heard of. It declared them; it models
  // them, in the order the parts are walked, which is the order they are appended in. Every part
  // is declared, because `SurfaceOverride::Part` keys by ORDINAL and needs no name to match.
  std::vector<uint32_t> wearers(subject.Surfaces.Slots.size(), 0u);
  for (const uint32_t worn : subject.Surfaces.PartSlot) {
    if (worn < wearers.size()) { wearers[worn] += 1u; }
  }
  for (size_t part = 0; !subject.ShadedByLights() && part < subject.Emitted.size() &&
                        part < subject.Model.Parts().size();
       ++part) {
    const uint32_t slot = part < subject.Surfaces.PartSlot.size() ? subject.Surfaces.PartSlot[part]
                                                                  : 0u;
    // AND THE SPLIT IS THE ENGINE'S RULE, MIRRORED: only a SHARED slot gains one. A part that is
    // its slot's only wearer takes the row in place, so modelling a new slot there would count a
    // surface the engine never made.
    if (slot < wearers.size() && wearers[slot] == 1u) { continue; }
    if (slot < wearers.size()) { wearers[slot] -= 1u; }
    const int carried = slot < subject.Surfaces.Material.size()
                            ? subject.Surfaces.Material[slot]
                            : -1;
    subject.Surfaces.Slots.push_back(subject.Surfaces.Slots[slot < subject.Surfaces.Slots.size()
                                                                ? slot
                                                                : 0u]);
    subject.Surfaces.Material.push_back(carried);
    subject.Surfaces.PartSlot[part] = (uint32_t)(subject.Surfaces.Slots.size() - 1u);
  }

  // THE STAGES ARE DECIDED LAST, because whether the scene carries glass is a question about the
  // surfaces that will be DRAWN, and those are declared above. Asking it first read an empty list
  // and fell back to the file -- which is the very answer an override replaces.
  says.Stages = {"subjects"};
  // GLASS IS WHAT THE DECLARATION LEAVES STANDING. An override replaces the surface, so a case
  // that states a flat emission over a glass body draws no glass -- and its own manifest says so,
  // calling itself `opaque-min-1px`. Asking the FILE here made 21 such cases run two passes the
  // oracle never ran.
  bool carriesGlass = false;
  if (says.Surfaces.empty()) {
    for (const outshine::Material &material : subject.Model.Surfaces()) {
      const outshine::SurfaceKind kind = outshine::StateOf(material).Kind();
      carriesGlass = carriesGlass || kind == outshine::SurfaceKind::ThinTransmissive ||
                     kind == outshine::SurfaceKind::Refractive;
    }
  } else {
    for (const outshine::SurfaceOverride &said : says.Surfaces) {
      const outshine::SurfaceKind kind = outshine::StateOf(said.Row).Kind();
      carriesGlass = carriesGlass || kind == outshine::SurfaceKind::ThinTransmissive ||
                     kind == outshine::SurfaceKind::Refractive;
    }
  }
  // A TRANSMISSIVE SURFACE NEEDS ITS PASS WHETHER OR NOT THE CASE COUNTS BOUNCES. The engine
  // refuses a plan whose surface is a thin transmissive sheet or a refracting volume and whose
  // passes do not draw it -- and rightly: what is transparent would otherwise be drawn opaque and
  // nothing would say so. Measured, gating this on the case's own bounce count kept 99 of the
  // corpus's cases from standing at all.
  if (carriesGlass) {
    says.Stages.push_back("subjectsTransmissive");
    says.Stages.push_back("compositeTransmission");
  }

  std::printf("SAYS stages=%zu first=%s transfer=%s\n", says.Stages.size(),
              says.Stages.empty() ? "-" : says.Stages.front().c_str(), "linear");
}

Prepared Prepare(Case &subject, outshine::Engine &engine) {
  using namespace outshine::Test;
  std::string why;
  const bool declared = ReadManifest(subject, why);
  CHECK(declared, "the case's manifest parses and its acceptance block resolves");
  if (!declared) {
    Refused(why);
    return Prepared::No;
  }

  const std::string owed = MissingInputs(subject);
  if (!owed.empty()) {
    outshine::Test::Unprepared((subject.Directory + " is missing " + owed +
                                " -- run test/harness/shared/corpus/prepare.py").c_str());
    return Prepared::No;
  }

  const bool loaded = BuildSubject(subject, why);

  if (!loaded && subject.Criterion == CriterionKind::LimitsProbe) {
    const std::string names = subject.Manifest.Root()["criterion"]["declines"].Str("");
    const bool named = !names.empty() && why.find(names) != std::string::npos;
    CHECK(named,
          "the limits probe's refusal names what the case says this engine declines, so it cannot "
          "pass on a refusal about something else");
    if (named) { std::printf("DECLINED %s -- %s\n", names.c_str(), why.c_str()); }
    Refused(why);
    return Prepared::No;
  }
  CHECK(loaded, "the case's subject and its camera both resolve");
  if (!loaded) {
    Refused(why);
    return Prepared::No;
  }
  std::printf("CAMERA %s\n", subject.CameraSource.c_str());
  std::printf("CRITERION %s -- %s [%s]\n",
              subject.Manifest.Root()["criterion"]["kind"].Str("").c_str(),
              subject.Manifest.Root()["criterion"]["says"].Str("").c_str(),
              subject.Manifest.Root()["criterion"]["statedAt"].Str("").c_str());

  if (subject.Oracle == OracleRole::CannotExpressTheCriterion) {
    std::printf("ORACLE NOT-DECIDING -- %s\n",
                subject.Manifest.Root()["criterion"]["oracleLimitation"].Str("").c_str());
  }

  DeclareDrives(subject);
  std::printf("DRIVES surfaces=%zu parts=%zu emitted=%zu\n", subject.Driving.Surfaces.size(),
              subject.Model.Parts().size(), subject.Emitted.size());

  engine.setRoots(outshine::Roots{subject.Directory, subject.Directory,
                                  "/tmp/outshine-corpus-cache", true});
  const bool canvas = engine.drawsInto(subject.Driving.Frame).has_value();
  CHECK(canvas, "the device came up, so the case can be rendered at all");
  if (!canvas) {
    Refused(engine.error());
    return Prepared::No;
  }
  const bool stood = subject.Driving.Stands(engine, why);
  CHECK(stood, "the case stands through the door, which is the path a client has");
  if (!stood) {
    Refused(why);
    return Prepared::No;
  }
  for (const outshine::Measure &held : engine.measures()) {
    std::printf("PROBE %-40s %.6f %s\n", held.What.c_str(), held.How, held.Unit.c_str());
  }
  return Prepared::Yes;
}

void ScoreExactnessConstruction(const Case &subject, const EdgeSet &silhouette, double tieMarginPx,
                                std::vector<Metric> &metrics) {
  const Exactness measured = Measure(silhouette);
  const bool claimed = subject.Placement == ExactnessClass::Exact;
  metrics.push_back({"silhouette_lines", (double)measured.LineCount(), 0.0, "lines",
                     Direction::Reported});
  metrics.push_back({"silhouette_edges", (double)measured.SilhouetteEdges, 0.0, "edges",
                     Direction::Reported});

  metrics.push_back({"exactness_slope_residual_px", measured.SlopeResidualPx, subject.OracleFloorPx,
                     "px", claimed ? Direction::AtMost : Direction::Reported});

  metrics.push_back({"exactness_margin_px", measured.MarginPx, kMarginFloorPx, "px",
                     claimed ? Direction::AtLeast : Direction::Reported});

  metrics.push_back({"exactness_margin_ceiling_px", measured.CeilingPx, 0.0, "px",
                     Direction::Reported});

  constexpr double kOneQuantityPx = 1e-9;
  metrics.push_back({"exactness_margin_agreement_px",
                     std::fabs(tieMarginPx - measured.MarginPx), kOneQuantityPx, "px",
                     claimed ? Direction::AtMost : Direction::Reported});

  constexpr size_t kLinesPrinted = 16;
  for (size_t line = 0; line < measured.Lines.size() && line < kLinesPrinted; ++line) {
    const LatticeLine &fit = measured.Lines[line];
    std::printf("NOTE   silhouette line %ld x - %ld y = %.9f over %zu edges: margin %.9g px of "
                "%.9g px, slope residual %.3g px\n",
                fit.P, fit.Q, fit.C, fit.Edges, fit.MarginPx, fit.CeilingPx, fit.SlopeResidualPx);
  }
}

void ScoreVisibilityTerm(const Case &subject, const Transform &clip, double biasM,
                         std::vector<Metric> &metrics) {
  double centre[3];
  subject.Model.CentreM(centre);
  double at[2];
  double ndc[3];
  clip.Point(centre, ndc);
  subject.Frame.Raster(ndc, at);

  double jacobian[3][2] = {{0, 0}, {0, 0}, {0, 0}};
  for (int axis = 0; axis < 3; ++axis) {
    double moved[3] = {centre[0], centre[1], centre[2]};
    moved[axis] += biasM;
    double movedNdc[3];
    double movedAt[2];
    clip.Point(moved, movedNdc);
    subject.Frame.Raster(movedNdc, movedAt);
    jacobian[axis][0] = movedAt[0] - at[0];
    jacobian[axis][1] = movedAt[1] - at[1];
  }

  double gram[3] = {0, 0, 0};
  for (int axis = 0; axis < 3; ++axis) {
    gram[0] += jacobian[axis][0] * jacobian[axis][0];
    gram[1] += jacobian[axis][0] * jacobian[axis][1];
    gram[2] += jacobian[axis][1] * jacobian[axis][1];
  }
  const double half = 0.5 * (gram[0] + gram[2]);
  const double gap = std::sqrt(std::max(0.0, half * half - (gram[0] * gram[2] - gram[1] * gram[1])));
  metrics.push_back({"shadow_ray_bias_m", biasM, 0.0, "m, subject frame", Direction::Reported});
  metrics.push_back({"shadow_ray_bias_px", std::sqrt(std::max(0.0, half + gap)), 0.0, "px",
                     Direction::Reported});
}

void ScoreAlternateSpellings(const Case &subject, outshine::Engine &engine, const Mask &ours,
                             std::vector<Metric> &metrics) {
  // AN ALTERNATE SPELLING IS THE SAME SUBJECT WRITTEN ANOTHER WAY -- .glb against .gltf, embedded
  // against external -- and the claim is that it COVERS the same pixels. Through the door that is
  // one declaration replacing another and a second capture; the case is stood again afterwards, so
  // whatever follows sees the subject it came in with. A scenario is a STREAM, which is what makes
  // re-declaring the honest way to say this rather than a second engine beside the first.
  const Json::Ref identical = subject.Manifest.Root()["identicalCoverage"];
  for (size_t which = 0; which < identical.Size(); ++which) {
    const std::string name = identical[which].Str("");
    Picture again;
    std::string trouble;
    outshine::Test::Drives spelling = subject.Driving;
    spelling.Path = name;
    bool built = spelling.Stands(engine, trouble) && Capture(engine, spelling, again, trouble);
    CHECK(built, ("the alternate spelling " + name + " reads, builds and renders").c_str());
    const bool restored = subject.Driving.Stands(engine, trouble);
    CHECK(restored, "the case stands again after the alternate spelling was drawn");
    if (!built || !restored) {
      Refused(trouble);
      continue;
    }
    const Mask other = FromDepth(again.Depth, again.Linear, ours.Width, ours.Height);
    metrics.push_back({"differs_from_" + name, (double)Disagreeing(ours, other), 0.0, "px",
                       Direction::AtMost});
  }
}

std::vector<float> OracleAsRgba(const RawF32 &oracle) {
  std::vector<float> samples((size_t)oracle.Width() * (size_t)oracle.Height() * 4u, 0.0f);
  for (int y = 0; y < oracle.Height(); ++y) {
    for (int x = 0; x < oracle.Width(); ++x) {
      const size_t at = ((size_t)y * (size_t)oracle.Width() + (size_t)x) * 4u;
      for (int channel = 0; channel < 3; ++channel) { samples[at + (size_t)channel] = oracle.At(x, y, channel); }
      samples[at + 3u] = oracle.Channels() > 3 ? oracle.At(x, y, 3) : 1.0f;
    }
  }
  return samples;
}

void ScoreStatedInvariants(const Case &subject, const Picture &picture, const RawF32 &oracle,
                           std::vector<Metric> &metrics) {
  LinearFrame tap;
  tap.Samples = &picture.Linear;
  tap.Width = (int)subject.Frame.WidthPx;
  tap.Height = (int)subject.Frame.HeightPx;
  const bool tapHolds = tap.Holds() || subject.Invariants.empty();
  CHECK(tapHolds, "the linear tap the stated invariants are computed on covers the frame");

  const bool oracleFits = oracle.Width() == tap.Width && oracle.Height() == tap.Height;
  std::vector<float> theirSamples;
  LinearFrame theirs;
  if (oracleFits) {
    theirSamples = OracleAsRgba(oracle);
    theirs.Samples = &theirSamples;
    theirs.Width = oracle.Width();
    theirs.Height = oracle.Height();
  }

  for (const Invariant &check : subject.Invariants) {
    if (!tap.Holds()) { break; }
    std::printf("INVARIANT %s -- %s\n", check.Name.c_str(),
                check.Kind == InvariantKind::HueOfBrightest ? "hue-of-brightest" : "region-compare");
    Evaluate(check, tap, metrics);
    if (!oracleFits || !theirs.Holds()) { continue; }
    std::vector<Metric> theirMetrics;
    Evaluate(check, theirs, theirMetrics);
    for (Metric &metric : theirMetrics) {
      metric.Name = "oracle_" + metric.Name;
      metric.Against = Direction::Reported;
      metric.Threshold = 0.0;
      metrics.push_back(metric);
    }
  }
}

outshine::Render::SubjectProxy MakeStudio(const Case &subject) {
  outshine::Render::SubjectProxy studio;
  const double anchorEcefM[3] = {outshine::Data::kWgs84A, 0.0, 0.0};
  studio.Stands(subject.Shaped(), anchorEcefM);
  if (subject.Animated()) { studio.Posed(&subject.PreviousPositions); }
  for (size_t part = 0; part < subject.Emitted.size(); ++part) {
    (void)studio.Emits(part, subject.Emitted[part]);
  }
  std::string why;
  (void)studio.Wears(subject.Surfaces.PartSlot, subject.Surfaces.Slots, why);
  if (subject.Lights == SceneLights::FromFile) {
    for (const Handed::Lamp &placed : subject.Model.Lights()) {
      studio.Lit(placed.Light);
    }
  }
  if (subject.Lights == SceneLights::DeclaredSun) { studio.Lit(subject.Sun); }

  if (subject.ShadedByLights()) {
    outshine::Render::SubjectEnvironment environment;
    for (int channel = 0; channel < 3; ++channel) {
      environment.RadianceLinear[channel] = subject.WorldRadiance[channel];
    }
    studio.Around(environment);
  }
  return studio;
}

[[nodiscard]] bool RangeAt(const outshine::Gltf::Viewpoint &eye, const outshine::Gltf::Viewport &frame,
                           const std::vector<float> &depth, int column, int row, double &out,
                           std::string &error) {
  if (eye.Kind != outshine::Render::CameraKind::Perspective) {
    error = "a depth probe states a range along a view ray, and this case's camera is orthographic";
    return false;
  }
  if (column < 0 || row < 0 || (double)column >= frame.WidthPx || (double)row >= frame.HeightPx) {
    error = "the probe's pixel is outside the " + std::to_string((long)frame.WidthPx) + "x" +
            std::to_string((long)frame.HeightPx) + " frame";
    return false;
  }
  const size_t at = (size_t)row * (size_t)frame.WidthPx + (size_t)column;
  if (at >= depth.size() || !(depth[at] > 0.0f)) {
    error = "nothing is drawn at the probe's pixel, so there is no surface to state a range for";
    return false;
  }
  const double halfHeight = std::tan(eye.YfovRad * 0.5);
  const double acrossNdc = 2.0 * ((double)column + 0.5) / frame.WidthPx - 1.0;
  const double downNdc = 1.0 - 2.0 * ((double)row + 0.5) / frame.HeightPx;
  const double across = acrossNdc * halfHeight * frame.Aspect();
  const double down = downNdc * halfHeight;
  const double secant = std::sqrt(across * across + down * down + 1.0);

  const double plane = eye.ZNearM > 0.0 ? eye.ZNearM : (double)outshine::Render::SceneRenderer::kNearM;
  out = plane / (double)depth[at] * secant;
  return true;
}

void ScoreDepthProbes(const Case &subject, const outshine::Render::SubjectProxy &studio,
                      const std::vector<float> &depth, std::vector<Metric> &metrics) {
  const Json::Ref probes = subject.Manifest.Root()["depthProbes"];
  for (size_t which = 0; which < probes.Size(); ++which) {
    const Json::Ref probe = probes[which];
    const std::string name = probe["name"].Str("");
    const std::string where = "depthProbes[" + std::to_string(which) + "]";
    double declared = 0, tolerance = 0;
    std::string why;
    if (!ReadDeclaredNumber(probe["rangeM"], (where + ".rangeM").c_str(), declared, why) ||
        !ReadDeclaredNumber(probe["toleranceM"], (where + ".toleranceM").c_str(), tolerance, why)) {
      Refused(why);
      metrics.push_back({name + "_range_error_m", std::nan(""), 0.0, "m", Direction::AtMost});
      continue;
    }
    double measured = 0;
    if (!RangeAt(subject.Eye, subject.Frame, depth, probe["atPx"][(size_t)0].Int(-1),
                 probe["atPx"][(size_t)1].Int(-1), measured, why)) {
      Refused(where + ": " + why);
      metrics.push_back({name + "_range_error_m", std::nan(""), tolerance, "m", Direction::AtMost});
      continue;
    }
    outshine::Test::Note((name + " range measured").c_str(), measured, "m");
    outshine::Test::Note((name + " range declared").c_str(), declared, "m");
    metrics.push_back(
        {name + "_range_error_m", std::fabs(measured - declared), tolerance, "m", Direction::AtMost});
  }
}

void NoteWhatTheStudioCarries(const Case &subject, const outshine::Render::SubjectProxy &studio) {
  if (subject.Lights == SceneLights::FromFile) {
    for (const Handed::Lamp &placed : subject.Model.Lights()) {
      outshine::Test::Note(
          ("light '" + placed.NodeName + "' on node '" + placed.NodeName + "', intensity").c_str(),
          (double)placed.Light.Intensity,
          placed.Light.Kind == outshine::LightKind::Directional ? "lux" : "candela");
    }
  }
  outshine::Test::Note("punctual lights the studio declares", (double)studio.Lights().size(),
                       "lights");

  outshine::Test::Note("uv sets the subject carries", subject.Model.HasUv1() ? 2.0 : 1.0, "sets");
  for (size_t part = 0; part < subject.Model.Parts().size(); ++part) {
    outshine::Test::Note(
        ("declared radiance of node '" + subject.Model.Parts()[part].NodeName + "', red").c_str(),
        (double)subject.Emitted[part][0], "linear, scene-referred");
  }
  if (!subject.MaterialFromFile()) { return; }
  for (size_t slot = 0; slot < subject.Surfaces.Slots.size(); ++slot) {
    outshine::Test::Note(("colour image texels across, surface slot " + std::to_string(slot)).c_str(),
                         (double)subject.Surfaces.Slots[slot].Colour.Width, "texels");
    outshine::Test::Note(("colour image texels down, surface slot " + std::to_string(slot)).c_str(),
                         (double)subject.Surfaces.Slots[slot].Colour.Height, "texels");
    outshine::Test::Note(("declared coverage factor, surface slot " + std::to_string(slot)).c_str(),
                         (double)subject.Surfaces.Slots[slot].Coverage(), "dimensionless");

    outshine::Test::Note(("colour image uv set, surface slot " + std::to_string(slot)).c_str(),
                         subject.Surfaces.Slots[slot].Colour.Set == outshine::UvSet::Second ? 1.0
                                                                                            : 0.0,
                         "index");
  }
}

struct DeclaredNormals {
  std::vector<float> Xyz;
  std::vector<float> Depth;
};

DeclaredNormals RasteriseDeclaredNormals(const Handed &geometry, const Transform &clip,
                                         const Viewport &viewport, int width, int height) {
  DeclaredNormals out;
  out.Xyz.assign((size_t)width * (size_t)height * 3u, 0.0f);
  out.Depth.assign((size_t)width * (size_t)height, 2.0f);
  if (geometry.Normals().size() < geometry.PositionsM().size()) { return out; }
  const std::vector<uint32_t> &indices = geometry.Indices();

  for (const Handed::Part &part : geometry.Parts()) {
  for (size_t triangle = 0; triangle * 3u + 2u < part.IndexCount; ++triangle) {
    double corner[3][2];
    double depth[3];
    const double *normal[3];
    bool projects = true;
    for (int which = 0; which < 3; ++which) {
      const size_t vertex = indices[part.FirstIndex + triangle * 3u + (size_t)which];
      const double point[3] = {geometry.PositionsM()[vertex * 3],
                               geometry.PositionsM()[vertex * 3 + 1],
                               geometry.PositionsM()[vertex * 3 + 2]};
      double ndc[3];
      clip.Point(point, ndc);
      if (!(ndc[2] >= -1.0 && ndc[2] <= 1.0)) {
        projects = false;
        break;
      }
      viewport.Raster(ndc, corner[which]);
      depth[which] = ndc[2];
      normal[which] = &geometry.Normals()[vertex * 3];
    }
    if (!projects) { continue; }
    int fromX = 0, toX = 0, fromY = 0, toY = 0;
    Detail::Span(corner, width, 0, fromX, toX);
    Detail::Span(corner, height, 1, fromY, toY);
    const double area = (corner[1][0] - corner[0][0]) * (corner[2][1] - corner[0][1]) -
                        (corner[2][0] - corner[0][0]) * (corner[1][1] - corner[0][1]);
    if (area == 0.0) { continue; }
    for (int y = fromY; y <= toY; ++y) {
      for (int x = fromX; x <= toX; ++x) {
        if (!Detail::Inside(corner, (double)x, (double)y)) { continue; }
        const double w0 = ((corner[1][0] - (double)x) * (corner[2][1] - (double)y) -
                           (corner[2][0] - (double)x) * (corner[1][1] - (double)y)) / area;
        const double w1 = ((corner[2][0] - (double)x) * (corner[0][1] - (double)y) -
                           (corner[0][0] - (double)x) * (corner[2][1] - (double)y)) / area;
        const double w2 = 1.0 - w0 - w1;
        const double z = w0 * depth[0] + w1 * depth[1] + w2 * depth[2];
        const size_t at = (size_t)y * (size_t)width + (size_t)x;

        if (z >= (double)out.Depth[at]) { continue; }
        out.Depth[at] = (float)z;
        for (int axis = 0; axis < 3; ++axis) {
          out.Xyz[at * 3u + (size_t)axis] =
              (float)(w0 * normal[0][axis] + w1 * normal[1][axis] + w2 * normal[2][axis]);
        }
      }
    }
  }
  }
  return out;
}

[[nodiscard]] std::vector<std::string> FileMaterialNames(const Handed &model) {
  std::vector<std::string> names;
  names.reserve(model.SurfaceNames().size());
  for (size_t at = 0; at < model.SurfaceNames().size(); ++at) {
    const std::string &declared = model.SurfaceNames()[at];
    names.push_back(declared.empty() ? "Material_" + std::to_string(at) : declared);
  }
  return names;
}

[[nodiscard]] std::vector<uint8_t> BlendedFileMaterials(const Handed &model) {
  std::vector<uint8_t> blended;
  blended.reserve(model.Surfaces().size());
  for (const outshine::Material &surface : model.Surfaces()) {
    blended.push_back(surface.Alpha == outshine::AlphaMode::Blended ? 1u : 0u);
  }
  return blended;
}

void NoteDisagreements(const outshine::Render::Parity::IdentityReading &reading) {
  for (const outshine::Render::Parity::Disagreement &where : reading.Disagreements) {
    std::printf("SURFACE-AT %d,%d oracle=%s ours=%s\n", where.X, where.Y,
                where.Oracle.Name.c_str(), where.Ours.Name.c_str());
  }
  for (const outshine::Render::Parity::Disagreement &where : reading.Splits) {
    std::printf("SURFACE-ORACLE-SPLIT %d,%d its index says %s and its picture does not\n", where.X,
                where.Y, where.Oracle.Name.c_str());
  }

  for (const outshine::Render::Parity::Swap &row : reading.Swaps) {
    std::printf("SURFACE-SWAP oracle=%s ours=%s over %zu px\n", row.OracleName.c_str(),
                row.OursName.c_str(), row.Pixels);
  }
}

[[nodiscard]] outshine::Render::Parity::DeclaredColours ColoursPerFileMaterial(const Case &subject) {
  outshine::Render::Parity::DeclaredColours out;
  if (subject.MaterialFromFile() || subject.ShadedByLights()) {
    out.Why = "the case takes its appearance from the file's own materials, so the oracle's picture "
              "is not one colour per material and its index pass cannot be held against it";
    return out;
  }
  const size_t materials = subject.Model.Surfaces().size();
  out.ByFileMaterial.assign(materials, {0.0f, 0.0f, 0.0f});
  out.Known.assign(materials, 0u);
  for (size_t part = 0; part < subject.Model.Parts().size() && part < subject.Emitted.size();
       ++part) {
    const int material = subject.Model.Parts()[part].Material;
    if (material < 0 || (size_t)material >= materials) { continue; }
    const std::array<float, 3> &radiance = subject.Emitted[part];
    if (out.Known[(size_t)material] && out.ByFileMaterial[(size_t)material] != radiance) {
      out.Why = "two parts wearing material " + std::to_string(material) +
                " were declared different radiance, so this case has no one colour per material";
      out.Known.assign(materials, 0u);
      return out;
    }
    out.ByFileMaterial[(size_t)material] = radiance;
    out.Known[(size_t)material] = 1u;
  }
  out.Computable = true;
  return out;
}

[[nodiscard]] Mask ScoreSurfaceIdentity(const Case &subject, const Picture &picture,
                                        const RawF32 &oraclePicture, const Mask &ours,
                                        const Mask &theirs, int frame,
                                        std::vector<Metric> &metrics) {
  using namespace outshine::Test;
  using namespace outshine::Render::Parity;

  const std::vector<std::string> names = FileMaterialNames(subject.Model);
  OracleSurfaces oracle;
  if (!oracle.Read(subject.Directory, IndexPass::Material, subject.ProductFrame(frame), names)) {
    Refused(oracle.Error());
    return Mask{};
  }
  if (oracle.Width() != theirs.Width || oracle.Height() != theirs.Height) {
    Refused("the oracle's material-index pass is not the shape its picture is");
    return Mask{};
  }
  if (picture.SurfaceIdentity.size() < (size_t)ours.Width * (size_t)ours.Height * 4u) {
    Refused("the surface-identity attachment does not cover the frame we rendered");
    return Mask{};
  }

  const OurSurfaces mine(picture.SurfaceIdentity, ours.Width, subject.Surfaces.Material, names);
  const DeclaredColours declared = ColoursPerFileMaterial(subject);
  const std::vector<uint8_t> blended = BlendedFileMaterials(subject.Model);
  const IdentityQuestion asked{oracle, mine, theirs, ours, oraclePicture, declared, blended};
  const IdentityReading reading = ReadSurfaceIdentity(asked);

  const bool namesOneSurface = subject.Accepted.Subject != SubjectClass::Transmissive;
  CHECK(!namesOneSurface || reading.OursNamingNoSlot == 0,
        "every pixel we drew names a surface slot of this subject's own table, so the identity "
        "attachment carries what the encoder bound and not what the target was cleared to");
  if (reading.OursNamingNoSlot > 0) {
    Note("pixels we drew whose identity names no slot", (double)reading.OursNamingNoSlot, "px");
  }

  metrics.push_back({"surface_oracle_distinct_materials", (double)reading.OracleDistinct, 0.0,
                     "indices", Direction::Reported});
  metrics.push_back({"surface_ours_distinct_slots", (double)reading.OursDistinct, 0.0, "slots",
                     Direction::Reported});
  metrics.push_back({"surface_identity_compared", (double)reading.Compared, 0.0, "px",
                     Direction::Reported});
  metrics.push_back({"surface_identity_agreeing", (double)reading.Agreeing, 0.0, "px",
                     Direction::Reported});
  metrics.push_back({"surface_identity_disagreeing", (double)reading.Disagreeing, 0.0, "px",
                     Direction::Reported});

  metrics.push_back({"surface_oracle_index_unlike_its_own_picture",
                     declared.Computable ? (double)reading.OracleSplit : std::nan(""), 0.0, "px",
                     Direction::Reported});

  metrics.push_back({"surface_identity_disagreeing_composite", (double)reading.Composite, 0.0, "px",
                     Direction::Reported});
  metrics.push_back({"surface_identity_disagreeing_attributable",
                     reading.AttributionKnown ? (double)reading.Attributable : std::nan(""), 0.0,
                     "px", Direction::Reported});

  if (!reading.AttributionKnown) { Refused("surface identity: " + declared.Why); }
  if (!reading.Adjudicated) { Refused("surface identity: " + reading.Refusal); }
  NoteDisagreements(reading);

  OracleSurfaces objects;
  if (!objects.Read(subject.Directory, IndexPass::Object, subject.ProductFrame(frame), names)) {
    Refused(objects.Error());
    return reading.AttributableAt;
  }
  metrics.push_back({"surface_oracle_distinct_objects",
                     (double)DistinctOracleIndices(objects, theirs), 0.0, "indices",
                     Direction::Reported});
  return reading.AttributableAt;
}

void ScoreShadingNormal(const Case &subject, const Picture &picture, const Mask &ours,
                        const Transform &clip, int frame, std::vector<Metric> &metrics) {
  using namespace outshine::Test;
  RawF32 cycles;
  const std::string path =
      subject.Directory + OracleProduct{"normal", "default", subject.ProductFrame(frame)}.Raw();
  if (!cycles.ReadFile(path)) {
    Refused(cycles.Error());
    return;
  }
  const size_t width = (size_t)ours.Width, height = (size_t)ours.Height;
  if (picture.ShadingNormal.size() < width * height * 4u ||
      (size_t)cycles.Width() != width || (size_t)cycles.Height() != height) {
    Refused("the shading-normal readback and the oracle's normal pass do not cover one frame");
    return;
  }

  const DeclaredNormals declared = RasteriseDeclaredNormals(subject.Model, clip, subject.Frame,
                                                            ours.Width, ours.Height);

  {
    std::vector<float> rgba((size_t)ours.Width * (size_t)ours.Height * 4u, 0.0f);
    for (size_t pixel = 0; pixel * 3u + 2u < declared.Xyz.size(); ++pixel) {
      for (int axis = 0; axis < 3; ++axis) {
        rgba[pixel * 4u + (size_t)axis] = declared.Xyz[pixel * 3u + (size_t)axis];
      }
    }
    std::string unwritten;
    (void)WriteRawF32(subject.Directory + "file.normal.raw", rgba, ours.Width, ours.Height, 4,
                      unwritten);
  }

  constexpr double kNormalSignalDeg = 0.4;
  size_t shaded = 0, noLobe = 0, uncovered = 0, adjudicated = 0;
  size_t disputed = 0, oursNearer = 0, cyclesNearer = 0;
  std::vector<double> disputedMargin;

  size_t bandDisputed[4] = {0, 0, 0, 0};
  size_t shadedBack = 0, disputedBack = 0;
  double worstDeg = 0, sumDeg = 0;
  std::vector<double> degrees, oursVsFile, cyclesVsFile;
  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      if (!ours.At((int)x, (int)y)) { ++uncovered; continue; }
      const size_t at = (y * width + x) * 4u;

      const double ex = picture.ShadingNormal[at], ey = picture.ShadingNormal[at + 1],
                   ez = picture.ShadingNormal[at + 2];
      const double ox = ey, oy = ex, oz = -ez;
      const double oursLength = std::sqrt(ox * ox + oy * oy + oz * oz);

      if (oursLength <= 0.0) { ++noLobe; continue; }
      const bool backFacing = picture.ShadingNormal[at + 3] < 0.0f;
      if (backFacing) { ++shadedBack; }

      const double bx = (double)cycles.At((int)x, (int)y, 0);
      const double by = (double)cycles.At((int)x, (int)y, 1);
      const double bz = (double)cycles.At((int)x, (int)y, 2);
      const double cx = bx, cy = bz, cz = -by;
      const double theirsLength = std::sqrt(cx * cx + cy * cy + cz * cz);
      if (theirsLength <= 0.0) { ++noLobe; continue; }
      ++shaded;
      double cosine = (ox * cx + oy * cy + oz * cz) / (oursLength * theirsLength);
      cosine = cosine > 1.0 ? 1.0 : (cosine < -1.0 ? -1.0 : cosine);
      const double deg = std::acos(cosine) * 180.0 / 3.14159265358979323846;
      degrees.push_back(deg);
      sumDeg += deg;
      if (deg > worstDeg) { worstDeg = deg; }

      const double fx = declared.Xyz[(y * width + x) * 3u];
      const double fy = declared.Xyz[(y * width + x) * 3u + 1];
      const double fz = declared.Xyz[(y * width + x) * 3u + 2];
      const double fileLength = std::sqrt(fx * fx + fy * fy + fz * fz);
      if (fileLength <= 0.0) { continue; }
      ++adjudicated;
      const auto against = [&](double ax, double ay, double az, double alen) {
        double c = (ax * fx + ay * fy + az * fz) / (alen * fileLength);
        c = c > 1.0 ? 1.0 : (c < -1.0 ? -1.0 : c);
        return std::acos(c) * 180.0 / 3.14159265358979323846;
      };
      const double oursFile = against(ox, oy, oz, oursLength);
      const double cyclesFile = against(cx, cy, cz, theirsLength);
      oursVsFile.push_back(oursFile);
      cyclesVsFile.push_back(cyclesFile);

      if (deg <= kNormalSignalDeg) { continue; }
      ++disputed;
      if (oursFile < cyclesFile) {
        ++oursNearer;
      } else if (cyclesFile < oursFile) {
        ++cyclesNearer;
      }
      disputedMargin.push_back(cyclesFile - oursFile);
      if (backFacing) { ++disputedBack; }
      const int band = (int)((double)x * 4.0 / (double)ours.Width);
      ++bandDisputed[band < 0 ? 0 : (band > 3 ? 3 : band)];
    }
  }
  std::sort(degrees.begin(), degrees.end());
  Note("shading normal, pixels compared", (double)shaded, "px");
  Note("shading normal, pixels excluded as no-lobe (zero vector, by predicate)", (double)noLobe,
       "px");
  Note("shading normal, pixels outside our coverage", (double)uncovered, "px");
  if (shaded == 0) {
    Refused("no covered pixel carries a shading normal on both sides, so nothing was compared");
    return;
  }
  metrics.push_back({"shading_normal_p50_deg", Percentile(degrees, 0.50), 0.0, "degrees",
                     Direction::Reported});
  metrics.push_back({"shading_normal_p95_deg", Percentile(degrees, 0.95), 0.0, "degrees",
                     Direction::Reported});
  metrics.push_back({"shading_normal_max_deg", worstDeg, 0.0, "degrees", Direction::Reported});
  metrics.push_back({"shading_normal_mean_deg", sumDeg / (double)shaded, 0.0, "degrees",
                    Direction::Reported});

  std::sort(oursVsFile.begin(), oursVsFile.end());
  std::sort(cyclesVsFile.begin(), cyclesVsFile.end());
  Note("shading normal, pixels the file adjudicates", (double)adjudicated, "px");

  Note("shading normal, pixels where the two legs disagree beyond what the texture can express", (double)disputed,
       "px");
  if (disputed > 0) {
    std::sort(disputedMargin.begin(), disputedMargin.end());
    Note("of those, the file is nearer OURS", (double)oursNearer, "px");
    Note("of those, the file is nearer CYCLES", (double)cyclesNearer, "px");
    Note("shaded fragments that are back-facing", (double)shadedBack, "px");
    Note("of the disputed, back-facing", (double)disputedBack, "px");
    Note("disputed in band 0 of 4 across the frame", (double)bandDisputed[0], "px");
    Note("disputed in band 1 of 4 across the frame", (double)bandDisputed[1], "px");
    Note("disputed in band 2 of 4 across the frame", (double)bandDisputed[2], "px");
    Note("disputed in band 3 of 4 across the frame", (double)bandDisputed[3], "px");
    metrics.push_back({"disputed_ours_nearer_fraction",
                       (double)oursNearer / (double)disputed, 0.0, "dimensionless",
                       Direction::Reported});

    metrics.push_back({"disputed_margin_p50_deg", Percentile(disputedMargin, 0.50), 0.0, "degrees",
                       Direction::Reported});
  }
  if (!oursVsFile.empty()) {
    metrics.push_back({"ours_vs_file_p50_deg", Percentile(oursVsFile, 0.50), 0.0, "degrees",
                       Direction::Reported});
    metrics.push_back({"ours_vs_file_p95_deg", Percentile(oursVsFile, 0.95), 0.0, "degrees",
                       Direction::Reported});
    metrics.push_back({"cycles_vs_file_p50_deg", Percentile(cyclesVsFile, 0.50), 0.0, "degrees",
                       Direction::Reported});
    metrics.push_back({"cycles_vs_file_p95_deg", Percentile(cyclesVsFile, 0.95), 0.0, "degrees",
                       Direction::Reported});
  }
}

void SayWhereItWas(const char *kind, const Excursion &worst) {
  using namespace outshine::Test;
  if (worst.Code <= 0.0) { return; }
  std::printf("NOTE   worst %s disagreement: %.9g codes at (%zu, %zu) channel %zu, ours %.9g "
              "against %.9g, over %zu px\n",
              kind, worst.Code, worst.X, worst.Y, worst.Channel, worst.Ours, worst.Theirs,
              worst.Pixels);
}

void ScorePictureBound(const PictureDelta &picture, const Tail &bound, int oracleSamples,
                       std::vector<Metric> &metrics) {
  for (const BoundTerm &term : bound.Terms) {
    std::printf("BOUND  %-56s %14.9g codes\n", term.Mechanism.c_str(), term.Codes);
  }
  if (!bound.Enforced) {
    std::printf("BOUND  %-56s %14s\n",
                "the oracle still estimates, so no tail bound may be enforced", "--");
  }

  const double atFraction =
      PercentileCode(picture.Buckets, picture.ChannelsCompared, kBoundFraction);
  metrics.push_back({"picture_p99_delta_code", atFraction, bound.Codes, "codes",
                     bound.Enforced ? Direction::AtMost : Direction::Reported, Count::Picture});
  metrics.push_back({"picture_max_delta_code", picture.Appearance.Code, bound.Codes, "codes",
                     Direction::Reported, Count::Picture});

  metrics.push_back({"picture_max_delta_code_alpha", picture.Predicate.Code,
                     outshine::Render::Parity::kPerceptualFloorCodes, "codes",
                     oracleSamples > 1 ? Direction::Reported :
                     Direction::AtMost, Count::Picture});
  metrics.push_back({"picture_max_delta_code_routed", picture.Routed.Code, 0.0, "codes",
                     Direction::Reported, Count::Picture});

  metrics.push_back({"picture_oracle_black_channels", (double)picture.OracleBlackChannels, 0.0,
                     "channels", Direction::Reported, Count::Picture});
  metrics.push_back({"picture_oracle_black_we_lit", (double)picture.OracleBlackWeLit, 0.0, "channels",
                     Direction::Reported, Count::Picture});
  metrics.push_back({"picture_oracle_black_worst_code", picture.OracleBlackWorstCode, 0.0, "codes",
                     Direction::Reported, Count::Picture});

  size_t shown = 0;
  for (const Excursion &channel : picture.Worst) {
    if (channel.Code <= 0.0) { break; }
    std::printf("NOTE   worst %zu: %11.6f codes at (%zu, %zu) channel %zu, ours %.9g against %.9g\n",
                ++shown, channel.Code, channel.X, channel.Y, channel.Channel, channel.Ours,
                channel.Theirs);
  }
  if (picture.Appearance.Pixels > shown) {
    std::printf("NOTE   and %zu further pixels carry an appearance disagreement, not listed\n",
                picture.Appearance.Pixels - shown);
  }
  metrics.push_back({"picture_pixels_routed", (double)picture.Routed.Pixels, 0.0, "px",
                     Direction::Reported, Count::Picture});
  metrics.push_back({"picture_pixels_differing", (double)picture.PixelsDiffering, 0.0, "px",
                     Direction::Reported, Count::Picture});
  metrics.push_back({"picture_channels_compared", (double)picture.ChannelsCompared, 0.0, "channels",
                     Direction::Reported, Count::Picture});
  size_t occupied = 0;
  for (size_t bucket = 0; bucket < kCodeBuckets; ++bucket) {
    if (picture.Buckets[bucket] == 0) { continue; }
    ++occupied;
    std::printf("HIST   delta_code in [%zu, %zu): %zu channels\n", bucket, bucket + 1,
                picture.Buckets[bucket]);
  }
  if (occupied == 0) {
    std::printf("HIST   every colour channel the two sides agree to cover agrees to the last bit of "
                "the transfer\n");
  }
  SayWhereItWas("appearance", picture.Appearance);
  SayWhereItWas("alpha-predicate", picture.Predicate);
  SayWhereItWas("routed-to-coverage", picture.Routed);
}

void SayBothVerdicts(const std::vector<Metric> &metrics, const Tail &bound) {
  bool criterionMet = true;
  bool withinPicture = true;
  for (const Metric &metric : metrics) {
    if (metric.Against == Direction::Reported || metric.Held()) { continue; }
    (metric.Counts == Count::Picture ? withinPicture : criterionMet) = false;
  }
  std::printf("KHRONOS-CRITERION %s\n", criterionMet ? "met" : "red");

  std::printf("PICTURE-BOUND %s\n",
              !bound.Enforced ? "not-enforced" : (withinPicture ? "within" : "outside"));
}

std::string Argument(int argc, char **argv) {
  if (argc < 2 || argv[1][0] == '\0') { return std::string(); }
  std::string directory = argv[1];
  if (directory.back() != '/') { directory += '/'; }
  return directory;
}

}

void ScoreVelocity(const Case &subject, const Picture &picture, const Mask &ours, int frame,
                   std::vector<Metric> &metrics) {
  using namespace outshine::Test;
  if (picture.Velocity.empty()) { return; }
  const size_t pixels = (size_t)ours.Width * (size_t)ours.Height;
  if (picture.Velocity.size() < pixels * 2u) {
    Refused("the velocity readback does not cover the frame");
    return;
  }
  size_t covered = 0, sentinel = 0, moving = 0;
  double furthestNdc = 0, furthestPx = 0;

  const double toPxX = 0.5 * subject.Frame.WidthPx, toPxY = 0.5 * subject.Frame.HeightPx;
  for (size_t pixel = 0; pixel < pixels; ++pixel) {
    if (!ours.In[pixel]) { continue; }
    ++covered;
    const double x = picture.Velocity[pixel * 2], y = picture.Velocity[pixel * 2 + 1];
    if (x <= -1.0e3 || y <= -1.0e3) {
      ++sentinel;
      continue;
    }
    const double moved = std::sqrt(x * x + y * y);
    if (moved > 0) { ++moving; }
    furthestNdc = std::fmax(furthestNdc, moved);
    const double acrossPx = x * toPxX, downPx = y * toPxY;
    furthestPx = std::fmax(furthestPx, std::sqrt(acrossPx * acrossPx + downPx * downPx));
  }
  metrics.push_back({"velocity_pixels_covered", (double)covered, 0.0, "px", Direction::Reported});
  metrics.push_back({"velocity_pixels_carrying_the_static_sentinel", (double)sentinel, 0.0, "px",
                     Direction::AtMost});

  const bool poseMoved = frame > 0 && subject.MovedPx > 0.0;
  metrics.push_back({"velocity_pixels_moving", (double)moving, poseMoved ? 1.0 : 0.0, "px",
                     poseMoved ? Direction::AtLeast : Direction::AtMost});
  Note("velocity, furthest a covered pixel moved since the previous frame", furthestNdc,
       "ndc per frame");
  Note("velocity, furthest a covered pixel moved since the previous frame", furthestPx,
       "px per frame");
}

struct Motion {
  bool Measurable = false;
  double MovedPx = 0;

  double MovedSincePreviousPx = 0;
};

[[nodiscard]] Motion ScoreMotion(const Case &subject, int frame) {
  using namespace outshine::Test;
  if (frame == 0) { return Motion{true, 0}; }
  Transform clip;
  if (!outshine::Gltf::ClipOf(subject.Eye, subject.Frame.Aspect(), clip)) {
    CHECK(false, "the resolved camera yields a projection, so the motion is measurable in pixels");
    return Motion{false, 0};
  }
  const std::vector<double> &now = subject.Model.PositionsM();
  const std::vector<double> &rest = subject.RestPositions;
  const std::vector<double> &before = subject.PreviousPositions;

  if (now.size() != rest.size() || now.empty()) {
    CHECK(false, "the posed subject carries the same vertices at every frame of the grid");
    return Motion{false, 0};
  }
  double furthestM = 0, furthestPx = 0, furthestSincePreviousPx = 0;
  for (size_t vertex = 0; vertex * 3 + 2 < now.size(); ++vertex) {
    double moved = 0;
    for (size_t axis = 0; axis < 3; ++axis) {
      const double off = now[vertex * 3 + axis] - rest[vertex * 3 + axis];
      moved += off * off;
    }
    furthestM = std::fmax(furthestM, std::sqrt(moved));
    const double here[3] = {now[vertex * 3], now[vertex * 3 + 1], now[vertex * 3 + 2]};
    const double there[3] = {rest[vertex * 3], rest[vertex * 3 + 1], rest[vertex * 3 + 2]};
    double hereNdc[3], thereNdc[3], herePx[2], therePx[2];
    clip.Point(here, hereNdc);
    clip.Point(there, thereNdc);
    subject.Frame.Raster(hereNdc, herePx);
    subject.Frame.Raster(thereNdc, therePx);
    const double dx = herePx[0] - therePx[0], dy = herePx[1] - therePx[1];
    furthestPx = std::fmax(furthestPx, std::sqrt(dx * dx + dy * dy));
    if (before.size() == now.size()) {
      const double was[3] = {before[vertex * 3], before[vertex * 3 + 1], before[vertex * 3 + 2]};
      double wasNdc[3], wasPx[2];
      clip.Point(was, wasNdc);
      subject.Frame.Raster(wasNdc, wasPx);
      const double sx = herePx[0] - wasPx[0], sy = herePx[1] - wasPx[1];
      furthestSincePreviousPx = std::fmax(furthestSincePreviousPx, std::sqrt(sx * sx + sy * sy));
    }
  }
  Note("subject motion from frame 0, furthest vertex", furthestM, "m");
  Note("subject motion from frame 0, furthest vertex projected", furthestPx, "px");
  Note("subject motion from the previous frame, furthest vertex projected", furthestSincePreviousPx,
       "px");
  Note("the floor it is held against, the oracle's own sub-pixel resolution",
       subject.OracleFloorPx, "px");
  return Motion{true, furthestPx, furthestSincePreviousPx};
}

[[nodiscard]] std::vector<float> WriteProducts(const Case &subject, const Picture &picture,
                                               const RawF32 &oracle, const Mask &ours,
                                               const Mask &theirs) {
  using namespace outshine::Test;
  const std::vector<uint8_t> reference = Encoded(oracle);
  const Pictures products(subject.Directory);
  std::string unwritten;
  CHECK(products.Png("0-reference.png", reference, theirs.Width, theirs.Height, unwritten),
        "0-reference.png is written from the same floats the score is computed on");
  CHECK(products.Png("1-outshine.png", picture.Rgba, ours.Width, ours.Height, unwritten),
        "1-outshine.png is written beside the reference, pass or fail");
  if (!unwritten.empty()) { Refused(unwritten); }

  std::string unwrittenNormal;
  if (!picture.ShadingNormal.empty()) {
    (void)WriteRawF32(subject.Directory + "outshine.normal.raw", picture.ShadingNormal, ours.Width,
                      ours.Height, 4, unwrittenNormal);
  }
  const std::vector<float> scored = ScoredFrame(picture.Linear, ours);
  const bool wroteFloats =
      !scored.empty() &&
      WriteRawF32(subject.Directory + "outshine.raw", scored, ours.Width, ours.Height, 4, unwritten);
  CHECK(wroteFloats, "outshine.raw is written beside oracle.raw, in the same OSRAWF32 layout and "
                     "from the samples the picture bound is computed on");
  RawF32 stored;
  bool storedIsScored = wroteFloats && stored.ReadFile(subject.Directory + "outshine.raw");
  for (size_t sample = 0; storedIsScored && sample < scored.size(); ++sample) {
    const int channels = 4;
    const int at = (int)(sample / (size_t)channels);
    storedIsScored = stored.At(at % ours.Width, at / ours.Width,
                               (int)(sample % (size_t)channels)) == scored[sample];
  }
  CHECK(storedIsScored, "outshine.raw reads back through the oracle's own reader as the samples "
                        "that were scored, so the file on disk IS the frame the number came from");
  if (!unwritten.empty()) { Refused(unwritten); }

  return scored;
}

[[nodiscard]] std::uint64_t Digest(const RawF32 &oracle) {
  std::uint64_t hash = 1469598103934665603ull;
  for (int y = 0; y < oracle.Height(); ++y) {
    for (int x = 0; x < oracle.Width(); ++x) {
      for (int channel = 0; channel < oracle.Channels(); ++channel) {
        const float value = oracle.At(x, y, channel);
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof bits);
        for (int byte = 0; byte < 4; ++byte) {
          hash ^= (std::uint64_t)((bits >> (byte * 8)) & 0xffu);
          hash *= 1099511628211ull;
        }
      }
    }
  }
  return hash;
}

constexpr char kGridChangesThePicture[] = "frames_whose_picture_differs_from_frame_0";

struct FrameVerdict {
  bool Compared = false;
  bool Held = false;

  std::uint64_t OracleDigest = 0;
};

FrameVerdict ScoreFrame(Case &subject, outshine::Engine &engine, int frame) {
  using namespace outshine::Test;

  RawF32 oracle;
  size_t seedApart = 0;
  if (!ReadOracle(subject, frame, oracle, seedApart)) { return FrameVerdict{}; }
  const std::uint64_t oracleDigest = Digest(oracle);

  const outshine::Render::SubjectProxy studio = MakeStudio(subject);
  NoteWhatTheStudioCarries(subject, studio);

  std::string why;
  Picture picture;
  std::printf("LAMPS file=%zu\n", subject.Model.Lights().size());
  // AND THE ENGINE IS STEPPED, NOT JUST THE COPY THIS RUNNER SCORES AGAINST. `Engine::advance` is
  // the door's verb for "time has passed"; posing the harness's own geometry and rendering without
  // it left the subject standing at frame 0 for the whole sequence, and the velocity buffer said
  // so -- nothing moved on a frame whose oracle had moved.
  //
  // ONE STEP, NOT A DURATION. `advance(elapsedS)` owes the simulation that much time and runs as
  // many FIXED steps as it covers -- at one frame per second that is sixty of them, each moving
  // the asset's animation on by a frame. The grid here is a frame INDEX, so the verb it wants is
  // the one that steps once. Measured: 15 animated cases whose frame 1 stood 87.6 px from the
  // oracle's, and frame 0 exactly on it.
  if (frame > 0 && !engine.advance()) {
    Refused(engine.error());
    return FrameVerdict{};
  }
  const bool rendered = Capture(engine, subject.Driving, picture, why);
  for (const outshine::Measure &held : engine.measures()) {
    if (held.What.rfind("draws taking", 0) == 0) {
      std::printf("PROBE %s = %.0f\n", held.What.c_str(), held.How);
    }
  }
  CHECK(rendered, "outshine rendered the subject and both readbacks landed");
  if (!rendered) {
    Refused(why);
    return FrameVerdict{};
  }

  const Mask ours =
      FromDepth(picture.Depth, picture.Linear, (int)subject.Frame.WidthPx, (int)subject.Frame.HeightPx);
  const Mask theirs = FromOracle(oracle);

  {
    double sum = 0.0; size_t many = 0;
    for (size_t at = 0; at + 3 < picture.Linear.size(); at += 4) {
      sum += picture.Linear[at]; ++many;
    }
    std::printf("LINEAR mean-red = %.9f over %zu px\n", many ? sum / (double)many : 0.0, many);
  }
  const std::vector<float> scored = WriteProducts(subject, picture, oracle, ours, theirs);

  std::vector<Metric> metrics;
  if (Reduced(subject)) {
    metrics.push_back({"oracle_samples_differing_between_seeds", (double)seedApart, 0.0, "samples",
                       Direction::AtMost});
  }

  // READ THROUGH THE DOOR, which is the point: the engine publishes what a frame cost and a client
  // reads it back. These used to be `renderer.SubjectDrawCount()` on an internal handle.
  const auto measured = [&engine](const char *named) {
    for (const outshine::Measure &held : engine.measures()) {
      if (held.What == named) { return held.How; }
    }
    return 0.0;
  };
  metrics.push_back({"subject_draws", measured("subject draws"), 0.0, "draws",
                     Direction::Reported});
  metrics.push_back({"subject_draw_calls", measured("subject draw calls"), 0.0, "calls",
                     Direction::Reported});
  metrics.push_back({"subject_surfaces", (double)subject.Surfaces.Slots.size(), 0.0, "slots",
                     Direction::Reported});

  const Metric coverageOurs{"coverage_fraction_outshine", ours.Fraction(),
                            subject.Accepted.CoverageFractionMin, "dimensionless",
                            Direction::AtLeast};
  const Metric coverageTheirs{"coverage_fraction_oracle", theirs.Fraction(),
                              subject.Accepted.CoverageFractionMin, "dimensionless",
                              Direction::AtLeast};
  metrics.push_back(coverageOurs);
  metrics.push_back(coverageTheirs);
  const bool bothPresent = coverageOurs.Held() && coverageTheirs.Held();
  if (!bothPresent) {
    Print(metrics);
    CHECK(bothPresent,
          "both renders carry a subject, so there is something to compare -- two empty masks agree "
          "perfectly and would have tested nothing");
    Refused("a side of the comparison is empty, so no agreement number is computed over it");
    return FrameVerdict{};
  }

  ScoreAlternateSpellings(subject, engine, ours, metrics);

  const Mask routedBySurface =
      ScoreSurfaceIdentity(subject, picture, oracle, ours, theirs, frame, metrics);
  const Routing routing{ours, theirs, routedBySurface};

  Transform clip;
  const bool projects = outshine::Gltf::ClipOf(subject.Eye, subject.Frame.Aspect(), clip);
  CHECK(projects, "the resolved camera yields a projection");
  if (projects) {
    const EdgeSet edges = Silhouette(subject.Model, clip, subject.Frame);
    const double tieMarginPx = TieMarginPx(ours, edges);
    metrics.push_back({"tie_margin_px", tieMarginPx, 0.0, "px", Direction::Reported});

    const WorstDisagreement worst =
        WorstDisagreementPx(routing, edges, Boundary(theirs).size(), kBoundFraction);
    metrics.push_back({"disagreement_max_px", worst.Px, subject.OracleFloorPx, "px",
                       Direction::Reported});

    metrics.push_back({"disagreement_p99_px", worst.AtFraction, subject.OracleFloorPx, "px",
                       subject.Placement == ExactnessClass::GeneralPosition ? Direction::AtMost
                                                                            : Direction::Reported,
                       Count::Picture});

    metrics.push_back({"disagreement_samples", (double)worst.Pixels, 0.0, "px",
                       Direction::Reported, Count::Picture});
    ScoreExactnessConstruction(subject, edges, tieMarginPx, metrics);
    ScoreVisibilityTerm(subject, clip, (float)measured("shadow ray near"), metrics);
  }

  ScoreStatedInvariants(subject, picture, oracle, metrics);

  const PictureDelta image = ComparePicture(scored, oracle, routing);
  CHECK(image.Comparable, "the linear tap and the coverage mask cover the oracle's frame, so every "
                          "pixel of the picture has something to be compared against");
  const Tail bound = BoundFor(subject.Path);
  ScorePictureBound(image, bound, subject.OracleSamples, metrics);

  ScoreShadingNormal(subject, picture, ours, clip, frame, metrics);

  const Mask depthOnly =
      FromDepth(picture.Depth, {}, (int)subject.Frame.WidthPx, (int)subject.Frame.HeightPx);
  ScoreVelocity(subject, picture, depthOnly, frame, metrics);

  ScoreDeterminism(subject, engine, picture, metrics);

  ScoreRadianceResidual(subject, picture, oracle, metrics);

  const Distribution boundary = BoundaryDisplacement(ours, theirs);
  metrics.push_back({"boundary_p95_px", boundary.P95, subject.Accepted.BoundaryP95MaxPx, "px",
                     Direction::Reported});
  metrics.push_back({"boundary_p50_px", boundary.P50, 0.0, "px", Direction::Reported});
  metrics.push_back({"boundary_p99_px", boundary.P99, 0.0, "px", Direction::Reported});
  metrics.push_back({"boundary_max_px", boundary.Max, 0.0, "px", Direction::Reported});

  metrics.push_back({"boundary_samples", (double)boundary.Samples, 0.0, "px", Direction::Reported});
  metrics.push_back({"iou", Iou(ours, theirs), 0.0, "dimensionless", Direction::Reported});

  metrics.push_back({"pixels_disagreeing", (double)Disagreeing(ours, theirs), 0.0, "px",
                     subject.Placement == ExactnessClass::Exact ? Direction::AtMost
                                                                : Direction::Reported,
                     Count::Picture});

  if (projects && Disagreeing(ours, theirs) > 0) {
    Note("disagreement attributed by node, both faces, overlap counted twice");
    const Attribution table =
        AttributeDisagreement(subject.Model, clip, subject.Frame, ours, theirs);
    for (const NodeDisagreement &node : table.Nodes) {
      std::printf("NOTE   node '%s' over %zu triangles: %zu px ours only, %zu px oracle only\n",
                  node.Node.c_str(), node.Triangles, node.OursOnly, node.TheirsOnly);
    }
    Note("disagreeing pixels no node's geometry projects onto", (double)table.Unattributed, "px");
    Note("triangles outside the depth range, unattributed", (double)table.Unprojectable,
         "triangles");
  }
  if (projects) {
    metrics.push_back({"triangles_outside_the_depth_range",
                       (double)TrianglesOutsideTheDepthRange(subject.Model, clip), 0.0,
                       "triangles", Direction::AtMost});
  }
  ScoreDepthProbes(subject, studio, picture.Depth, metrics);

  Note("oracle instrument floor", subject.OracleFloorPx, "px");
  const double passBound = subject.Accepted.Subject == SubjectClass::Transmissive ? 4.0 : 2.0;
  metrics.push_back(
      {"plan_passes", measured("plan passes"), passBound, "passes", Direction::AtMost});

  const Json::Ref expected = subject.Manifest.Root()["expected"]["subjectFrameFraction"];
  double declaredFraction = 0;
  const bool statesFraction = ReadDeclaredNumber(expected, "expected.subjectFrameFraction",
                                                 declaredFraction, why);
  CHECK(statesFraction, "the manifest declares the frame fraction its camera was derived for");
  if (statesFraction && projects) {
    const double fraction = ProjectedAreaPx(subject.Model, clip, subject.Frame) /
                            (subject.Frame.WidthPx * subject.Frame.HeightPx);

    metrics.push_back({"frame_fraction_error", std::fabs(fraction - declaredFraction),
                       subject.Accepted.FrameFractionTolerance, "dimensionless",
                       frame == 0 ? Direction::AtMost : Direction::Reported});
    Note("projected frame fraction", fraction, "dimensionless");
    Note("declared frame fraction", declaredFraction, "dimensionless");
  } else if (statesFraction) {
    Refused("the resolved camera yields no projection, so no frame fraction was recomputed");
  } else {
    Refused(why);
  }

  if (image.PixelsDiffering > 0) {
    const bool placed = boundary.P95 <= subject.Accepted.BoundaryP95MaxPx;
    Note(placed ? "attribution: the geometry is in the right pixels and the shading is wrong"
               : "attribution: the geometry is in the wrong pixels, so the shading is not reached");
  }

  size_t reduced = 0;
  for (Metric &metric : metrics) {
    const auto declared = subject.Reductions.find(metric.Name);
    if (declared == subject.Reductions.end() || metric.Against == Direction::Reported) { continue; }
    std::printf("REDUCED %s -- %s\n", metric.Name.c_str(), declared->second.c_str());
    metric.Against = Direction::Reported;
    ++reduced;
  }
  for (const Metric &metric : metrics) { subject.MetricsReported.insert(metric.Name); }
  Note("metrics this case declares its oracle cannot decide", (double)reduced, "reductions");

  Print(metrics);
  SayBothVerdicts(metrics, bound);
  FrameVerdict verdict{true, true, oracleDigest};
  for (const Metric &metric : metrics) {
    if (metric.Against == Direction::Reported) { continue; }
    CHECK(metric.Held(), metric.Name.c_str());
    verdict.Held = verdict.Held && metric.Held();
  }
  return verdict;
}

int ScoreRenderCase(int argc, char **argv) {
  using namespace outshine::Test;

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::printf("REFUSED SDL did not start: %s\n", SDL_GetError());
    return 1;
  }

  RunnerLog logging;
  outshine::Log::SetSink(&logging);

  Case subject;
  subject.Directory = Argument(argc, argv);
  CHECK(!subject.Directory.empty(),
        "the runner was given the case directory it is to score, which is its only argument");
  if (subject.Directory.empty()) { return Report(); }
  std::printf("CASE %s\n", subject.Directory.c_str());

  outshine::Engine engine;
  if (Prepare(subject, engine) == Prepared::No) {
    std::printf("VERDICT NOTHING-TO-COMPARE\n");
    return Report();
  }

  std::string why;
  int compared = 0;
  double furthestMovedPx = 0;
  int framesThatMoved = 0;
  std::uint64_t firstOracleDigest = 0;
  int oracleFramesThatDiffer = 0;
  int stoppedAt = -1;
  int refusedAt = -1;
  for (int frame = 0; frame < subject.Frames; ++frame) {
    if (subject.Animated()) {
      std::printf("FRAME %d of %d at %.9g s\n", frame, subject.Frames,
                  (double)frame / subject.Fps);

      subject.PreviousPositions = subject.Model.PositionsM();
      if (!PoseGeometry(subject, frame, why)) {
        CHECK(false, "the subject poses at every frame of the declared grid");
        Refused(why);
        stoppedAt = frame;
        break;
      }
      const Motion motion = ScoreMotion(subject, frame);
      if (!motion.Measurable) {
        stoppedAt = frame;
        break;
      }
      furthestMovedPx = std::fmax(furthestMovedPx, motion.MovedPx);
      subject.MovedPx = motion.MovedSincePreviousPx;
      if (motion.MovedPx > subject.OracleFloorPx) { ++framesThatMoved; }
    }
    const FrameVerdict verdict = ScoreFrame(subject, engine, frame);
    if (verdict.Compared) {
      if (frame == 0) {
        firstOracleDigest = verdict.OracleDigest;
      } else if (verdict.OracleDigest != firstOracleDigest) {
        ++oracleFramesThatDiffer;
      }
    }
    if (!verdict.Compared) {
      stoppedAt = frame;
      refusedAt = frame;
      break;
    }
    ++compared;
    if (!verdict.Held) {
      stoppedAt = frame;
      break;
    }
  }

  Note("the furthest the drawn subject moved from frame 0 over the grid", furthestMovedPx, "px");
  Note("frames whose drawn subject differs from frame 0", (double)framesThatMoved, "frames");
  Note("frames whose oracle picture differs from frame 0", (double)oracleFramesThatDiffer, "frames");
  if (subject.Animated()) {
    const int changing = framesThatMoved > 0 ? framesThatMoved : oracleFramesThatDiffer;
    std::vector<Metric> grid{{kGridChangesThePicture, (double)changing, 1.0, "frames",
                              Direction::AtLeast}};
    subject.MetricsReported.insert(kGridChangesThePicture);
    const auto declared = subject.Reductions.find(kGridChangesThePicture);
    if (declared != subject.Reductions.end()) {
      std::printf("REDUCED %s -- %s\n", kGridChangesThePicture, declared->second.c_str());
      grid[0].Against = Direction::Reported;
    }
    Print(grid);
  }

  for (const auto &declared : subject.Reductions) {
    CHECK(subject.MetricsReported.count(declared.first) == 1,
          "every declared reduction names a metric this case actually reports");
  }
  if (subject.Animated() && subject.Reductions.count(kGridChangesThePicture) == 0) {

    CHECK(furthestMovedPx > subject.OracleFloorPx || oracleFramesThatDiffer > 0,
          "the declared grid changes the picture -- the drawn subject moves, or the oracle's own "
          "frames differ -- so the sequence is not a still rendered once per frame and agreeing with "
          "the oracle by construction");
  }
  Note("frames compared", (double)compared, "frames");
  Note("frames declared", (double)subject.Frames, "frames");
  if (stoppedAt >= 0) {
    // A STOP IS NOT AGREEMENT. A frame that FAILED a metric has already said so and the sequence
    // stopping there is the design; a frame that could not be rendered or scored at all recorded
    // nothing, and this path used to return CLEAN over it -- AnimatedCube reported 37 checks and
    // zero failures while the engine refused its second pose.
    CHECK(refusedAt < 0,
          "the sequence stopped on a frame that FAILED rather than on one that could not be "
          "rendered or scored -- a refusal mid-grid is a red frame, not a shorter grid");
    std::printf("FIRST FAILING FRAME %d of %d\n", stoppedAt, subject.Frames);
    std::printf("VERDICT COMPARED\n");
    Covers("I.26.10 a render test is a directory: one runner reads the declaration, renders the "
           "subject with no world, scores it against the cached oracle by named metrics with their "
           "own thresholds and directions, and always writes the three pictures");
    return Report();
  }
  CHECK(compared == subject.Frames,
        "every frame of the declared grid was compared against the oracle at that frame");
  std::printf("VERDICT COMPARED\n");
  Covers("I.26.10 a render test is a directory: one runner reads the declaration, renders the "
         "subject with no world, scores it against the cached oracle by named metrics with their "
         "own thresholds and directions, and always writes the three pictures");
  return Report();
}

struct ConfiguredCase::Held {
  Case Subject;
  outshine::Render::SubjectScratch Scratch;
};

ConfiguredCase::ConfiguredCase() : Held_(std::make_unique<Held>()) {}
ConfiguredCase::~ConfiguredCase() = default;

bool ConfiguredCase::Read(const std::string &directory, std::string &error) {
  Held_->Subject.Directory = directory;
  if (!Held_->Subject.Directory.empty() && Held_->Subject.Directory.back() != '/') {
    Held_->Subject.Directory += '/';
  }
  if (!ReadManifest(Held_->Subject, error)) { return false; }
  return BuildSubject(Held_->Subject, error);
}

bool ConfiguredCase::Declines(void) const {
  return Held_->Subject.Criterion == CriterionKind::LimitsProbe;
}

bool ConfiguredCase::Start(outshine::Render::SceneRenderer &renderer, std::string &error,
                           const std::vector<outshine::Render::Stage> &alsoContent, int surfaceW,
                           int surfaceH) {
  outshine::Render::PlanSpec declaration;
  DeclarePlan(Held_->Subject, declaration);
  for (const outshine::Render::Stage stage : alsoContent) {
    declaration.Content.push_back(stage);
  }

  declaration.Outputs.push_back(outshine::Render::Resource::Surface);
  std::shared_ptr<const outshine::Render::Compiled> plan;
  if (![&] { auto made = outshine::Render::Compiled::Compile(declaration); if (made) { plan = *std::move(made); return true; } error = std::move(made).error(); return false; }()) { return false; }
  renderer.Init(surfaceW > 0 ? surfaceW : (int)Held_->Subject.Frame.WidthPx,
                surfaceH > 0 ? surfaceH : (int)Held_->Subject.Frame.HeightPx, plan);
  if (!renderer.DeviceUsable()) {
    error = "the device did not come up, so this case cannot be shown";
    return false;
  }
  return true;
}

bool ConfiguredCase::FrameToFill(double fill, std::string &error) {
  outshine::Camera framed;
  Viewpoint derived;
  if (!Held_->Subject.Held.frames(fill, framed) || !ViewpointOf(framed, derived)) {
    error = "the subject has no extent, so no camera can be derived from it";
    return false;
  }
  Held_->Subject.Eye = derived;
  return true;
}

bool ConfiguredCase::PoseAt(int frame, std::string &error) {
  if (Held_->Subject.Animated()) { Held_->Subject.PreviousPositions = Held_->Subject.Model.PositionsM(); }
  return PoseGeometry(Held_->Subject, frame, error);
}

bool ConfiguredCase::Draw(outshine::Render::SceneRenderer &renderer, std::string &error) {
  const outshine::Render::SubjectProxy studio = MakeStudio(Held_->Subject);
  if (!outshine::Render::Show(renderer, studio, MakeView(Held_->Subject), Held_->Scratch, error)) { return false; }
  renderer.RenderFrame();
  return true;
}

int ConfiguredCase::Frames(void) const { return Held_->Subject.Frames; }
double ConfiguredCase::Fps(void) const { return Held_->Subject.Fps; }
int ConfiguredCase::WidthPx(void) const { return (int)Held_->Subject.Frame.WidthPx; }
int ConfiguredCase::HeightPx(void) const { return (int)Held_->Subject.Frame.HeightPx; }
const std::string &ConfiguredCase::Title(void) const { return Held_->Subject.Directory; }
