/* THE RENDER RUNNER. One program over one case directory: read the glTF, resolve the camera and the
 * recipe, render, compare against the cached oracle, score. It contains no scene-specific branch at
 * all -- a case that would need one is not a render case (board:0084).
 *
 * THE GUARD IS WORTH MORE THAN THE COMPARISON. Two empty images score perfectly: IoU over two empty
 * sets is 1.0 under most formulations and boundary displacement over an empty boundary is 0, so a
 * case that renders nothing passes green having tested nothing. At two hundred directories where
 * adding one costs a file drop, that is how a suite goes hollow. Zero coverage on EITHER side is a
 * failure and not a comparison, an absent or unreadable oracle is a failure and never a skip, and
 * the trailer says COMPARED or NOTHING-TO-COMPARE so the two cannot be confused by their exit code.
 *
 * THE PICTURES ARE ALWAYS WRITTEN, pass and fail alike, because the owner opens the case directory
 * to see progress and a picture that only appears on a failure cannot show any.
 *
 * EXACTLY TWO PICTURES IN A CASE DIRECTORY AND NEVER A THIRD -- `0-reference.png`, how it should look,
 * and `1-outshine.png`, what we produce now, honest and including broken. Nothing else in the folder
 * is an image: a difference picture, a coverage mask and a second shaded frame were each proposed
 * and each refused, because every one of them was read as one of the two and the reader needed a
 * legend to tell which. THE MASK IS AN INSTRUMENT FOR ONE NARROW QUESTION AND THE PICTURE IS THE
 * PRODUCT: substituting the scored mask for the colour frame would have made every folder look
 * correct while the renderer drew no visible subject at all.
 *
 * BOTH PICTURES ARE WRITTEN HERE, FROM THE TWO BUFFERS THE SCORE IS COMPUTED ON. The reference used
 * to be Blender's own PNG beside the float dump the number came from -- two encodings of one image,
 * with nobody checking they agreed, and a second set of colour-management settings to keep honest.
 *
 * ONE ALPHA CONVENTION ON BOTH SIDES: RGBA, STRAIGHT (NON-PREMULTIPLIED), ALPHA IS COVERAGE, AND THE
 * COMPARISON READS ALL FOUR CHANNELS. Without alpha a black subject and no subject are the same
 * pixels -- MEASURED, the oracle's sphere carries 46 101 of its 46 151 covered pixels at exactly 0.0
 * RGB -- so a three-channel comparison cannot tell "we drew black" from "we drew nothing", which is
 * the empty-image hole living inside the image. At one sample per pixel under a 0.01 px box filter
 * the oracle's alpha is exactly 0 or 1, so straight and premultiplied coincide here and the choice
 * only starts to matter when a filter widens; it is stated now so that it is not chosen then. */
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "Check.h"

#include "Acceptance.h"
#include "Attribution.h"
#include "Exactness.h"
#include "Invariant.h"
#include "ManifestSchema.h"
#include "Mask.h"
#include "Metric.h"
#include "RawF32.h"
#include "PictureBound.h"
#include "Pictures.h"
#include "Radiance.h"
#include "SurfaceIdentity.h"
#include "Ties.h"

#include "Document.h"
#include "GltfStudio.h"
#include "Json.h"
#include "Log.h"
#include "Image.h"
#include "RenderPlan.h"
#include "Renderer.h"
#include "Subject.h"

using outshine::Json;
using outshine::Gltf::Document;
using outshine::Gltf::Placement;
using outshine::Gltf::Subject;
using outshine::Gltf::Transform;
using outshine::Gltf::Viewport;
using namespace outshine::Render::Parity;

namespace {

/* THE SURFACES ONE SUBJECT DRAWS WITH: one slot per material any drawn primitive names, and
 * `PartSlot` is which slot each part draws with. Two primitives of one material share a slot, which
 * is what lets the compiled draw list merge them into one call. The decoded rasters are held here
 * because the renderer copies them and the studio only points at them. */
/* THE THREE DECODED IMAGES ONE SURFACE MAY WEAR, held together because they belong to one surface:
 * three vectors indexed by slot would be three things to keep in step, and the slot is the thing. */
struct SurfaceRasters {
  outshine::Clients::Raster Colour;
  outshine::Clients::Raster Normal;
  outshine::Clients::Raster MetalRough;
  outshine::Clients::Raster Emissive;
};

struct SurfaceTable {
  std::vector<outshine::Render::SubjectMaterial> Slots;
  std::vector<int> Material;      /* the document's material index per slot, -1 where none */
  std::vector<uint32_t> PartSlot;
  std::vector<SurfaceRasters> Decoded;
};

/* WHICH OF THE FILE'S OWN CHANNELS THE DECLARED RADIANCE IS TAKEN FROM, and it is a three-valued
 * question that no boolean can carry (`Enum.2`). `Declared` is the arm where the manifest states the
 * colours itself and the file's materials are not read for appearance at all. The other two name a
 * glTF socket, and which one is a property of the ASSET: `TextureLinearInterpolationTest` states its
 * whole picture in `emissiveFactor`/`emissiveTexture` over a base colour of `[0,0,0,1]`, so a runner
 * that could only read base colour would render its two spheres black and score that. */
enum class FileColour { Declared, BaseColour, Emissive, Row };

/* WHERE THE NAMED SOCKET'S VALUE COMES FROM IN THAT FILE, and the case says which because the two
 * are different subjects. `Texture` is the arm the socket arms were built for -- the picture IS the
 * image, and a case that declared it and found no image would be scoring a flat factor while
 * claiming to score a texture. `Factor` is the arm `EmissiveStrengthTest` needs: five cubes whose
 * whole appearance is `emissiveFactor` times `KHR_materials_emissive_strength`, with no image
 * anywhere in the file. Neither is a default, so the mismatch in either direction is a refusal
 * naming the file rather than a picture nobody looks at. */
enum class FileColourCarrier { Texture, Factor };

/* WHETHER THE FILE'S OWN LIGHTS CROSS THE glTF BOUNDARY, and it is a per-case declaration because
 * the answer is not the same for every case (board:0085). For OUR OWN generated
 * fixtures the light is declared beside the asset, so that a rung measures the light we meant; for
 * a Khronos asset whose criterion is stated IN TERMS OF the light in the file -- `DirectionalLight`
 * says "the directional lightsource is defined as ..." -- re-declaring it beside the asset would
 * measure our transcription instead. `None` is the default and drops whatever the file carries. */
enum class SceneLights { None, FromFile, DeclaredSun };

/* BLENDER'S FACTORY WORLD, and it is a property of the ORACLE rather than of the engine, which is why
 * it stands in the test and not in `src/`. A `Background` node at colour 0.05087608844041824 linear
 * on all three channels and strength 1.0, sampled as a light (board:0085): under a
 * coverage recipe -- 1 spp, a box filter at 0.01 px, a Diffuse BSDF at roughness 0, zero bounces --
 * Cycles has no integration left to perform and a facet of albedo rho returns exactly rho*L.
 * At the declared albedo 0.8 that is 0.8 x 0.05087608844041824 = 0.0407008708.
 *
 * INDEPENDENT OF THE FACET'S NORMAL, and that is the environment's doing rather than an omission
 * here: a uniform environment delivers the same radiance from every direction, so the irradiance on
 * a Lambertian surface is pi*L whichever way it faces and the outgoing radiance is rho*L. THERE IS
 * NO N.L TERM TO MATCH IN THIS SCENE -- a cube's three visible faces come back at one value, and a
 * renderer that shaded N.L here would disagree with the oracle on two of them. What the oracle does
 * carry beyond rho*L is VISIBILITY: at one sample the single cosine-weighted direction either
 * escapes to the world and the pixel is rho*L, or it meets geometry and the pixel is 0. That is
 * ambient occlusion at one sample, it is a noise field rather than a value, and no rasteriser
 * reproduces it -- which is why a case whose subject occludes itself or its neighbours cannot reach
 * an exact image and says so in its own class rather than in a tolerance. */
constexpr double kFactoryWorldRadiance = 0.05087608844041824;

/* WHAT THE RUNNER READ AND NOTHING ELSE: the declaration, its resolved camera and its resolved
 * thresholds. Held as one object so the render step takes a subject rather than eleven arguments
 * (`I.23`). */
struct Case {
  std::string Directory;
  Json Manifest;
  Document File;
  Subject Geometry;
  Placement Eye;
  Viewport Frame;
  Acceptance Accepted;
  ExactnessClass Placement = ExactnessClass::GeneralPosition;
  /* THE ORACLE'S OWN SUB-PIXEL RESOLUTION, read once from the recipe that produced it: half the box
   * filter's width bounds how far a Cycles sample can sit from the pixel centre, and every near-tie
   * in this suite is judged against it. Held here rather than read at each of the four sites that
   * want it, because `Json::Ref::Num(def)` answers an absent key with the default and a filter width
   * of zero would silently turn every one of those judgements into "no tolerance at all". */
  double OracleFloorPx = 0;
  CriterionKind Criterion = CriterionKind::Numeric;
  OracleRole Oracle = OracleRole::Reference;
  std::string CameraSource;
  /* Scene-referred linear radiance per subject part, derived from the case's own material
   * declaration and from nothing else. */
  std::vector<std::array<float, 3>> Emitted;
  std::string MaterialKind;
  /* Which channel of the file's own materials the appearance comes from, or `Declared` where the
   * manifest states it. The decoded images are held in `Surfaces` because the renderer copies them
   * and the studio only points at them. */
  FileColour Colour = FileColour::Declared;
  FileColourCarrier Carrier = FileColourCarrier::Texture;
  bool MaterialFromFile() const { return Colour != FileColour::Declared; }
  /* THE ARM WHERE NO PER-PART RADIANCE IS THE ANSWER AT ALL: the surface's colour is the BRDF
   * evaluated against the light list, so the declared radiance is zero everywhere and the residual
   * against the oracle is a comparison of two shading models rather than of one number. */
  bool ShadedByLights() const { return Colour == FileColour::Row; }
  SceneLights Lights = SceneLights::None;
  /* THE DECLARED SUN, resolved once out of the manifest so that the studio builder is a copy and not
   * a second reading. Unread unless `Lights` names it, which is what `SceneLights` is for. */
  outshine::PunctualLight Sun;
  /* Whether the ORACLE of this case still has an estimator: a scene whose only sources are lights
   * with no area is sampled deterministically and owes the two-seed check; more than one such light
   * is not, because Cycles picks one per shading event. The manifest declares which. */
  bool DeltaLit = false;
  /* The environment's radiance per channel, scene-referred linear: Blender's factory world under
   * the `factory` arm, the declared colour times the declared strength under `uniform`. */
  double WorldRadiance[3] = {kFactoryWorldRadiance, kFactoryWorldRadiance, kFactoryWorldRadiance};
  /* What the ASSET says a render of itself must satisfy, empty unless the criterion is
   * `stated-invariant` -- which is the only kind that has any. */
  std::vector<Invariant> Invariants;
  SurfaceTable Surfaces;
  /* WHAT IS IN THIS CASE'S PATH THAT IS KNOWN TO DIFFER, and therefore what the picture bound's tail
   * is the sum of. Every field of it is read off the case or off the resolved surfaces; none of it
   * is a threshold a manifest can set (board:0089). */
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

/* WHICH CASES OWE THE TWO-SEED CHECK: the ones whose oracle has no estimator left. A surface that
 * emits its declared colour gathers nothing, whatever the colour is keyed on. A scene lit only by
 * punctual lights over a world of strength zero is the same claim from the other side -- a light
 * with no radius is sampled in one deterministic direction -- so two seeds must agree bit for bit
 * there too, and any difference names the source that still has an area. */
bool Reduced(const Case &subject) {
  return subject.MaterialKind == "emission" || subject.MaterialKind == "emission-per-material" ||
         subject.DeltaLit;
}

/* Which glTF socket a manifest's `scene.material.source` names, or `Declared` for the arm where the
 * manifest states the colours itself. WHICH (source, kind) PAIRS EXIST is the schema's and the
 * refusal below is about this runner's arms, not about the manifest's legality -- a spelling the
 * schema declares and this reader has no arm for is a hole here, and a spelling it does not declare
 * never reaches this line. */
[[nodiscard]] bool ReadFileColour(const Json::Ref &declared, FileColour &out, std::string &error) {
  if (declared.StrEquals("gltf-base-colour")) {
    out = FileColour::BaseColour;
    return true;
  }
  if (declared.StrEquals("gltf-emissive")) {
    out = FileColour::Emissive;
    return true;
  }
  if (declared.StrEquals("gltf")) {
    out = FileColour::Row;
    return true;
  }
  if (declared.StrEquals("manifest") || !declared.Valid()) {
    out = FileColour::Declared;
    return true;
  }
  error = "scene.material.source is '" + declared.Str("") +
          "', and this runner has no arm for it";
  return false;
}

[[nodiscard]] bool ReadFileColourCarrier(const Json::Ref &declared, FileColourCarrier &out,
                                         std::string &error) {
  if (declared.StrEquals("texture")) {
    out = FileColourCarrier::Texture;
    return true;
  }
  if (declared.StrEquals("factor")) {
    out = FileColourCarrier::Factor;
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
  /* `sun` IS A DECLARED DELTA LIGHT AND IT REACHES BOTH SIDES. Blender takes the same four numbers
   * and builds a SUN whose angular diameter the case declares; the studio builds a directional
   * light out of the direction and the irradiance times the colour. THE ANGLE HAS NO ENGINE SIDE
   * and that is the whole reason a case may only declare zero: a sun with an angular diameter has an
   * area, which puts an integral back in the oracle and a soft terminator in a picture the engine
   * draws hard. `point` is still the arm this runner has no path to. */
  if (declared.StrEquals("sun")) {
    out = SceneLights::DeclaredSun;
    return true;
  }
  error = "scene.light.kind is '" + declared.Str("") +
          "', and this runner builds the file's own lights ('gltf'), a declared 'sun', or none -- "
          "a 'point' declared beside the asset reaches the oracle and has no path into the studio";
  return false;
}

/* THE DECLARED SUN AS THE ENGINE'S OWN LIGHT. Blender's Sun Strength is an irradiance in W/m^2 on a
 * surface facing the beam and `KHR_lights_punctual`'s directional `intensity` is an illuminance in
 * lux on the same surface, so the two are the same NUMBER in the same PLACE and the RAW arm of the
 * importer is what says so -- there is no conversion here to get wrong.
 *
 * AN ANGULAR DIAMETER ABOVE ZERO IS REFUSED AND NOT ROUNDED AWAY. A sun with an angle is an area
 * source: the oracle gets an estimator back and a picture gets a soft terminator that a punctual
 * light cannot draw. The refusal names the number so that a case cannot acquire one silently. */
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

/* THE RENDERER'S OWN LINES, ON THE RUNNER'S STDOUT. The library emits nothing without an injected
 * sink, and a test whose subject IS the renderer was reading a device that could not speak: a
 * validation error, a lost device and a failed buffer map were all the same silence. */
class RunnerLog : public outshine::LogSink {
public:
  void Write(double, outshine::LogLevel level, const char *tag, const char *event,
             const std::vector<outshine::LogField> &fields) override {
    if (level < outshine::LogLevel::Info) { return; }
    std::printf("LOG %s %s", tag, event);
    for (const outshine::LogField &field : fields) {
      std::printf(" %s=%s", field.Key, field.Value.c_str());
    }
    std::printf("\n");
  }
};

/* THE CAMERA COMES FROM THE MANIFEST WHERE IT DECLARES ONE, then from the glTF, then from the
 * framing rule -- and never from anywhere else, so no viewpoint can be tuned into a pass. */
[[nodiscard]] bool ResolveCamera(Case &subject, std::string &error) {
  const Json::Ref declared = subject.Manifest.Root()["scene"]["camera"];
  if (declared["source"].StrEquals("manifest")) {
    double eye[3] = {0, 0, 0}, aim[3] = {0, 0, 0};
    for (size_t axis = 0; axis < 3; ++axis) {
      eye[axis] = declared["positionM"][axis].Num(0.0);
      aim[axis] = declared["lookAtM"][axis].Num(0.0);
    }
    if (!Placement::LookAt(eye, aim, declared["rollRad"].Num(0.0), subject.Eye)) {
      error = "the manifest's camera aims at its own eye or straight up";
      return false;
    }
    subject.Eye.ZNearM = declared["clipStartM"].Num(0.0);
    subject.Eye.ZFarM = declared["clipEndM"].Num(0.0);
    subject.CameraSource = "manifest";
    /* THE PROJECTION IS DECLARED AND NOT INFERRED FROM WHICH FIELD IS PRESENT. A parallel
     * projection is a different matrix, not a very long focal length, and a case that needs one
     * needs it for a reason it can state: `PointLightIntensityTest` compares two panels 2.25 m
     * apart in the same picture, and only a parallel projection makes them congruent in pixels, so
     * "identical" is decidable there instead of approximate. */
    if (declared["projection"].StrEquals("orthographic")) {
      subject.Eye.Kind = outshine::Gltf::CameraKind::Orthographic;
      subject.Eye.YMagM = declared["yMagM"].Num(0.0);
      /* The engine's parallel projection carries the VERTICAL extent and derives the horizontal
       * from the frame's aspect, so the horizontal magnification is not a second declaration. */
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
  /* A CASE THAT NAMES THE FILE AS ITS CAMERA AND GETS THE FRAMING RULE INSTEAD would render a
   * perfectly good picture through a path it was written to exercise and never touch, so the
   * refusal is the reader's own sentence and there is no arm past it. THE INDEX IS THE MANIFEST'S
   * AND HAS NO DEFAULT: `Cameras` carries a perspective and an orthographic camera at one point,
   * and "the first one" would render one and report the other's criterion. */
  if (declared["source"].StrEquals("gltf")) {
    if (declared["index"].GetKind() != Json::Kind::Number) {
      error = "the manifest names the glTF as the camera's source and declares no `index` into its "
              "`cameras`, and a file may carry more than one";
      return false;
    }
    if (!outshine::Gltf::DeclaredPlacement(subject.File, (int)declared["index"].Num(-1),
                                           subject.Eye, error)) {
      return false;
    }
    subject.CameraSource = "gltf";
    return true;
  }
  if (subject.Geometry.Frame(subject.Eye)) {
    subject.CameraSource = "framing-rule";
    return true;
  }
  error = "the subject has degenerate bounds, so the framing rule refuses -- a fallback camera here "
          "would manufacture exactly the empty picture the guard exists to catch";
  return false;
}

/* WHAT THE ORACLE STILL CARRIES THAT OUR PICTURE CANNOT BE HELD TO, read off the case's own
 * description of its reference and never off a tolerance (board:0089).
 *
 * `not-bit-reproducible` OWES ITS MEASUREMENT AND IS REFUSED WITHOUT ONE. It is not an estimator --
 * there is nothing random left in the mathematics -- so a bound is derivable; but it is not zero
 * either, and what makes it a number is the residue between two renders of the same scene at the
 * same seed on this host. A case that claimed the word without the number would be claiming room it
 * had not measured. */
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

/* THE DISPLAY TRANSFER BOTH SIDES ARE READ THROUGH, and the runner refuses any spelling but the one
 * it implements rather than quietly scoring on a curve nobody applied. `Standard` over an `sRGB`
 * device is the sRGB OETF and nothing else, which is exactly what our plan's `Transfer::Linear` into
 * an sRGB-encoding attachment produces -- so the picture bound's `T` is the same function on both
 * sides. Every exposure and gamma in the chain must be the identity, because a case that scaled one
 * of them would be compared on an axis this runner does not know it is on. */
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
  /* THE DECLARATION IS READ BEFORE THE DOCUMENT IS. The schema is the preparer's too, so a manifest
   * this runner accepts is one the preparer can prepare -- which is the property the two closed sets
   * did not have. */
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
  const Json::Ref filterWidth = root["renders"]["default"]["pixelFilter"]["widthPx"];
  if (filterWidth.GetKind() != Json::Kind::Number || filterWidth.Num() <= 0.0) {
    error = "renders.default.pixelFilter.widthPx is absent or not positive, and half of it is the "
            "oracle's own sub-pixel resolution -- the floor every near-tie here is judged against";
    return false;
  }
  subject.OracleFloorPx = 0.5 * filterWidth.Num();
  /* THE ASSET SAYS WHAT CORRECT IS AND THE KIND OF ANSWER DECIDES THE INSTRUMENT (I.26.12). The
   * quotation and the file it came from are required beside it, so the kind cannot move without a
   * quotation moving with it. */
  if (!ReadCriterionKind(root["criterion"]["kind"].Str(""), subject.Criterion, error)) {
    return false;
  }
  if (root["criterion"]["says"].Str("").empty() ||
      root["criterion"]["statedAt"].Str("").empty()) {
    error = "criterion states no `says` or no `statedAt`, so the acceptance is ours and not the "
            "asset's";
    return false;
  }
  /* WHICH KINDS OF CRITERION MAY CARRY STATED INVARIANTS, and it is not one kind but two. A
   * `stated-invariant` case MUST declare them, because they are its whole acceptance. A
   * `self-describing` case MAY, because reclassifying the PICTURE releases nothing that is
   * computable from our own render alone -- `DirectionalLight` keeps its hue check when its
   * reference stops deciding (board:0085). A `numeric` or `limits-probe` case may
   * not: the first is scored on the image and the second has no pass at all, so an invariant there
   * would be an acceptance its criterion does not claim. */
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

  /* `gltf-base-colour` IS THE ARM WHERE THE FILE OWNS THE SURFACE, so nothing is read here and the
   * factor and the image both come out of the document once the subject has been built. Declaring
   * either of them beside a Khronos asset would be measuring our re-declaration and not the asset. */
  const Json::Ref material = root["scene"]["material"];
  if (!ReadFileColour(material["source"], subject.Colour, error)) { return false; }
  subject.MaterialKind = material["kind"].Str("");
  if ((subject.Colour == FileColour::BaseColour || subject.Colour == FileColour::Emissive) &&
      !ReadFileColourCarrier(material["carriedBy"], subject.Carrier, error)) {
    return false;
  }
  const Json::Ref light = root["scene"]["light"];
  if (!ReadSceneLights(light["kind"], subject.Lights, error)) { return false; }
  if (subject.Lights == SceneLights::DeclaredSun && !ReadDeclaredSun(light, subject.Sun, error)) {
    return false;
  }
  /* WHETHER THE TWO SEEDS MUST AGREE BIT FOR BIT, declared in one spelling on both arms. A DECLARED
   * SUN CAN NO LONGER ANSWER `selected`: the preparer takes the subject's own emission out of
   * Cycles' light tree and out of every gathering ray, so that arm's enumeration has lost the word.
   * The file's own lights are a different question -- their number is upstream's -- so the `gltf`
   * arm keeps it. The measurements are in the schema's note beside the field. */
  subject.DeltaLit = light["estimator"].StrEquals("delta");
  if (!ReadOraclePath(light, subject.Path, error)) { return false; }
  if (!ReadDisplayTransfer(root["renders"]["default"], error)) { return false; }
  /* THE ENVIRONMENT AS A RADIANCE, per channel, and the two arms are the two ways a case can state
   * one. `factory` is Blender's own and its number is not the manifest's to restate; `uniform` is
   * the arm a case takes to REMOVE the environment, and the only value in the tree today is zero --
   * which is the reduction a lit case needs, because an environment is an area source and a delta
   * light is not. */
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

/* WHAT THE PREPARER OWES THIS CASE, taken from the case's own declaration rather than from a list
 * here: every file a subject names as `as`, and the oracle products of the default recipe. A case
 * directory's ONLY tracked file is its manifest (board:0083), so on a fresh clone
 * all of these are absent and the answer is "not prepared" -- which is red, and is neither a pass
 * nor a skip, because a tier that skipped when its inputs were missing could not be told from a tier
 * that passed having compared nothing. */
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
    const std::string name = recipes.Key(which);
    const std::string product =
        name == "default" ? std::string("oracle.exr") : "oracle." + name + ".exr";
    if (!Present(subject.Directory + product)) { owed.push_back(product); }
  }
  std::string missing;
  for (const std::string &name : owed) {
    if (missing.find(name) != std::string::npos) { continue; }
    if (!missing.empty()) { missing += ", "; }
    missing += name;
  }
  return missing;
}

outshine::Render::SubjectWrap WrapOf(outshine::Gltf::Wrap wrap) {
  switch (wrap) {
    case outshine::Gltf::Wrap::ClampToEdge: return outshine::Render::SubjectWrap::ClampToEdge;
    case outshine::Gltf::Wrap::MirroredRepeat: return outshine::Render::SubjectWrap::MirroredRepeat;
    case outshine::Gltf::Wrap::Repeat: return outshine::Render::SubjectWrap::Repeat;
  }
  return outshine::Render::SubjectWrap::Repeat;
}

/* THE SURFACE TABLE THE SUBJECT DRAWS WITH: one slot per material any drawn primitive names, in the
 * order the parts first name them. Two primitives of one material get one slot, which is what lets
 * the compiled draw list merge them into one call, and a primitive that names no material gets a
 * slot of the engine's declared default -- which is a surface, not an absence. */
void ResolveSurfaceTable(const Document &file, const Subject &geometry, SurfaceTable &out) {
  out.Slots.clear();
  out.Material.clear();
  out.Decoded.clear();
  out.PartSlot.assign(geometry.Parts().size(), 0);
  for (size_t part = 0; part < geometry.Parts().size(); ++part) {
    const int material = geometry.Parts()[part].Material;
    size_t slot = out.Material.size();
    for (size_t at = 0; at < out.Material.size(); ++at) {
      if (out.Material[at] == material) {
        slot = at;
        break;
      }
    }
    if (slot == out.Material.size()) {
      outshine::Render::SubjectMaterial surface;
      if (material >= 0 && (size_t)material < file.Materials().size()) {
        /* THE WHOLE ROW AND NOT A CHANNEL OF IT. The coverage factor is then `baseColorFactor.a` by
         * construction whatever the colour channel is, which is glTF's own rule: alpha comes from
         * the base colour and from nowhere else, even where the picture is stated in emissive. */
        surface.Row = file.Materials()[(size_t)material].Surface;
      }
      out.Material.push_back(material);
      out.Slots.push_back(surface);
    }
    out.PartSlot[part] = (uint32_t)slot;
  }
}

/* THE SURFACES THE FILE OWNS: each slot's colour image and the sampler that addresses it. Every
 * refusal here names what the asset declared and what was missing, because a texture that quietly
 * failed to load draws the factor alone -- a flat colour that looks exactly like a material somebody
 * authored that way.
 *
 * ON THE SOCKET ARMS THE COLOUR IMAGE AND THE COVERAGE IMAGE ARE ONE BINDING, AND WHERE THAT IS A
 * LIE THE CASE IS REFUSED. Those arms REPLACE the closure with a single emitter or a Diffuse BSDF
 * whose whole appearance is one image, and the subject draw's `Colour` slot is that image: its rgb
 * is the colour and its alpha is the coverage. glTF, though, takes coverage always from
 * `baseColorTexture` whichever socket the picture comes from. Under the base-colour arm the two are
 * the same image by construction; under the emissive arm they are the same image only where the
 * asset happens to name one texture for both -- `TextureLinearInterpolationTest`'s label plate does
 * -- so anything else stops here by name. THIS IS A PROPERTY OF THE LOWERED ARMS AND NOT OF THE
 * SURFACE: the `gltf` arm below binds four of the file's images at once.
 */
/* ONE OF THE FILE'S OWN MAPS INTO ONE SURFACE SLOT, WHICHEVER SOCKET IT SITS IN. It is a different
 * function from the colour one because it is a different question: there is no alpha and no coverage
 * to decide, and a socket that declares nothing is an ordinary material rather than a refusal --
 * glTF's defaults are the factors, and white is what the shader multiplies by when no image is bound.
 *
 * THE sRGB TRANSFER IS NOT DECIDED HERE AND CANNOT BE. What crosses is the file's RGBA8 texels; which
 * of them carry the transfer is a property of the socket, and `SubjectDraw::Upload` is where the
 * socket is named. A `Linear` in this function's name was true of the two maps it had and would be a
 * lie about the third (`NL.1`). */
[[nodiscard]] bool ReadSocketImage(const Document &file, const outshine::Gltf::MaterialRef &material,
                                   const outshine::Gltf::TextureRef &declared, const char *socket,
                                   outshine::Clients::Raster &raster,
                                   outshine::Render::SubjectTexture &bound, std::string &error) {
  if (!declared.Declared()) { return true; }
  if (declared.TexCoord != 0) {
    error = std::string("material '") + material.Name + "' reads its " + socket + " from TEXCOORD_" +
            std::to_string(declared.TexCoord) + ", and this subject carries the first uv set only";
    return false;
  }
  const outshine::Gltf::Texture &texture = file.Textures()[(size_t)declared.Texture];
  std::vector<uint8_t> encoded;
  if (!file.ImageBytes(texture.Source, encoded)) {
    error = std::string("material '") + material.Name + "' names " + socket + " image " +
            std::to_string(texture.Source) + ", whose bytes could not be read";
    return false;
  }
  if (!outshine::Clients::DecodeImage(encoded.data(), encoded.size(), raster) || !raster.Holds()) {
    error = std::string("the ") + socket + " image of material '" + material.Name + "' is " +
            std::to_string(encoded.size()) + " bytes that this decoder does not read";
    return false;
  }
  bound.Rgba = raster.Rgba.data();
  bound.Width = (uint32_t)raster.Width;
  bound.Height = (uint32_t)raster.Height;
  if (texture.Sampler >= 0) {
    const outshine::Gltf::Sampler &sampler = file.Samplers()[(size_t)texture.Sampler];
    bound.WrapU = WrapOf(sampler.WrapS);
    bound.WrapV = WrapOf(sampler.WrapT);
    bound.Magnify = sampler.Mag == outshine::Gltf::Filter::Nearest
        ? outshine::Render::SubjectFilter::Nearest
        : outshine::Render::SubjectFilter::Linear;
    bound.Minify = sampler.Min == outshine::Gltf::Filter::Nearest
        ? outshine::Render::SubjectFilter::Nearest
        : outshine::Render::SubjectFilter::Linear;
    bound.Mip = sampler.Mip == outshine::Gltf::MipFilter::None
        ? outshine::Render::SubjectMip::None
        : (sampler.Mip == outshine::Gltf::MipFilter::Nearest
               ? outshine::Render::SubjectMip::Nearest
               : outshine::Render::SubjectMip::Linear);
  }
  return true;
}

[[nodiscard]] bool ResolveFileSurface(const Document &file, const Subject &geometry,
                                      FileColour channel, FileColourCarrier carrier,
                                      SurfaceTable &table, std::string &error) {
  table.Decoded.assign(table.Slots.size(), SurfaceRasters{});
  const char *socket = channel == FileColour::Emissive ? "emissiveTexture" : "baseColorTexture";
  size_t textured = 0;
  for (size_t slot = 0; slot < table.Slots.size(); ++slot) {
    const int index = table.Material[slot];
    if (index < 0 || (size_t)index >= file.Materials().size()) { continue; }
    const outshine::Gltf::MaterialRef &material = file.Materials()[(size_t)index];
    const outshine::Gltf::TextureRef &declared =
        channel == FileColour::Emissive ? material.Emissive : material.BaseColour;
    if (table.Slots[slot].State().Kind() != outshine::SurfaceKind::Opaque &&
        material.BaseColour.Texture != declared.Texture) {
      error = std::string("material '") + material.Name + "' is not OPAQUE, takes its colour from " +
              socket + " " + std::to_string(declared.Texture) + " and its coverage from " +
              "baseColorTexture " + std::to_string(material.BaseColour.Texture) +
              ", and this subject binds one image per surface -- the second binding is the missing "
              "capability, not a texture to substitute";
      return false;
    }
    if (!declared.Declared()) { continue; }
    if (declared.TexCoord != 0) {
      error = std::string("material '") + material.Name + "' reads its " + socket + " from TEXCOORD_" +
              std::to_string(declared.TexCoord) + ", and this subject carries the first uv set only";
      return false;
    }
    const outshine::Gltf::Texture &texture = file.Textures()[(size_t)declared.Texture];
    std::vector<uint8_t> encoded;
    if (!file.ImageBytes(texture.Source, encoded)) {
      error = "material '" + material.Name + "' names image " + std::to_string(texture.Source) +
              ", whose bytes could not be read";
      return false;
    }
    if (!outshine::Clients::DecodeImage(encoded.data(), encoded.size(), table.Decoded[slot].Colour) ||
        !table.Decoded[slot].Colour.Holds()) {
      error = std::string("the ") + socket + " image of material '" + material.Name + "' is " +
              std::to_string(encoded.size()) + " bytes that this decoder does not read";
      return false;
    }
    outshine::Render::SubjectTexture &base = table.Slots[slot].Colour;
    base.Rgba = table.Decoded[slot].Colour.Rgba.data();
    base.Width = (uint32_t)table.Decoded[slot].Colour.Width;
    base.Height = (uint32_t)table.Decoded[slot].Colour.Height;
    if (texture.Sampler >= 0) {
      const outshine::Gltf::Sampler &sampler = file.Samplers()[(size_t)texture.Sampler];
      base.WrapU = WrapOf(sampler.WrapS);
      base.WrapV = WrapOf(sampler.WrapT);
      base.Magnify = sampler.Mag == outshine::Gltf::Filter::Nearest
          ? outshine::Render::SubjectFilter::Nearest
          : outshine::Render::SubjectFilter::Linear;
      base.Minify = sampler.Min == outshine::Gltf::Filter::Nearest
          ? outshine::Render::SubjectFilter::Nearest
          : outshine::Render::SubjectFilter::Linear;
      base.Mip = sampler.Mip == outshine::Gltf::MipFilter::None
          ? outshine::Render::SubjectMip::None
          : (sampler.Mip == outshine::Gltf::MipFilter::Nearest
                 ? outshine::Render::SubjectMip::Nearest
                 : outshine::Render::SubjectMip::Linear);
    }
    ++textured;
  }

  /* THE OTHER THREE MAPS, AND ONLY UNDER THE ARM THAT SHADES WITH THE FILE'S OWN ROW. The other two
   * arms REPLACE the closure -- a diffuse or an emissive one -- so a normal map they decoded would
   * be an image nothing reads, and `SciFiHelmet` says so in its own manifest rather than binding
   * one silently.
   *
   * THE EMISSIVE IMAGE IS HERE AND NOT WITH THE COLOUR, because under THIS arm it is not the colour:
   * `emissiveFactor * emissiveTexture` is a radiance added to what the BRDF returns, and the socket
   * arms above take the emissive INSTEAD of the closure. `BoomBox`, `Lantern` and `WaterBottle` all
   * state `emissiveFactor` as `[1, 1, 1]` and put the whole picture of the glow in the image, so a
   * row read without it emits white over the entire body. */
  if (channel == FileColour::Row) {
    for (size_t slot = 0; slot < table.Slots.size(); ++slot) {
      const int index = table.Material[slot];
      if (index < 0 || (size_t)index >= file.Materials().size()) { continue; }
      const outshine::Gltf::MaterialRef &material = file.Materials()[(size_t)index];
      table.Slots[slot].NormalScale = (float)material.NormalScale;
      const struct {
        const outshine::Gltf::TextureRef &Declared;
        const char *Socket;
        outshine::Clients::Raster &Into;
        outshine::Render::SubjectTexture &Bound;
      } maps[] = {
          {material.Normal, "normalTexture", table.Decoded[slot].Normal, table.Slots[slot].Normal},
          {material.MetallicRoughness, "metallicRoughnessTexture", table.Decoded[slot].MetalRough,
           table.Slots[slot].MetalRough},
          {material.Emissive, "emissiveTexture", table.Decoded[slot].Emissive,
           table.Slots[slot].Emissive},
      };
      for (const auto &map : maps) {
        if (!ReadSocketImage(file, material, map.Declared, map.Socket, map.Into, map.Bound, error)) {
          return false;
        }
      }
    }
  }

  /* A TEXTURE IS OWED WHERE THE CASE SAYS THE PICTURE IS ONE, in both directions. Under `gltf` the
   * row is the appearance and a material with no image is an ordinary material, so nothing is owed;
   * under the two socket arms the case has declared which of the two it reads, and a file that
   * disagrees with its own case's declaration is what stops here. */
  if (channel != FileColour::Row && carrier == FileColourCarrier::Texture && textured == 0) {
    error = std::string("the manifest hands the surface to the file's ") + socket +
            " and no material of it declares one";
    return false;
  }
  if (carrier == FileColourCarrier::Factor && textured > 0) {
    error = std::string("the manifest says the appearance is the ") + socket +
            " FACTOR and " + std::to_string(textured) + " material(s) of the file declare an image "
            "on that socket, which this case would then be sampling instead";
    return false;
  }
  if (textured > 0 && !geometry.HasUv()) {
    error = "the file's materials are the surface and the subject carries no TEXCOORD_0 to sample "
            "them with";
    return false;
  }
  return true;
}

/* WHAT EACH PART OF THE SUBJECT EMITS, derived from the case's own material declaration and from
 * nothing else. Three arms, and the split between them is board:0087's:
 *
 * `diffuse` IS THE CLOSED FORM AND IT IS ONLY AVAILABLE TO A SINGLE UNOCCLUDED FACET. Under a
 * uniform environment a Lambertian facet returns `rho*L` whichever way it faces, with no integral
 * left -- but only while nothing in the scene can be seen from anything else. Where it can, Cycles
 * at one sample takes ONE cosine-weighted direction per pixel and either escapes to the world or
 * meets geometry, so the pixel is a Bernoulli draw whose mean is the visible sky fraction, and the
 * case has stopped measuring a placement. A subject of more than one mesh-bearing node is refused
 * this arm outright, because there the surfaces certainly do see one another.
 *
 * `emission` REMOVES ALL FOUR INTEGRALS AT ONCE -- the world as a light, the sun's disk, the point
 * light's radius and visibility -- because a surface that emits its declared colour gathers nothing.
 * IT DECLARES ONE COLOUR PER NODE AND HAS NO SHORTER SPELLING: a single flat colour over three
 * touching cubes fuses them into one silhouette, which hides a misplaced node inside the union and
 * is a WORSE instrument than the noise it replaces. The boundary between two declared colours is
 * exact; a boundary in binary ambient-occlusion noise never was. */
[[nodiscard]] bool ResolveEmission(const Case &subject, const Document &file,
                                   const Subject &geometry,
                                   std::vector<std::array<float, 3>> &out, std::string &error) {
  const Json::Ref material = subject.Manifest.Root()["scene"]["material"];
  const size_t parts = geometry.Parts().size();
  out.assign(parts, {0.0f, 0.0f, 0.0f});

  /* THE LIT ARM DECLARES NO RADIANCE. Every part emits nothing and the picture is what the light
   * list and the surface's own row make of it, which is the whole point of the arm -- EXCEPT where
   * the file's own material says the surface is not lit. `KHR_materials_unlit` states the whole
   * appearance of such a surface as its base colour, so that colour IS its declared radiance, and a
   * lit scene carrying one caption plate has a part whose picture no light decides. */
  if (subject.ShadedByLights()) {
    for (size_t part = 0; part < parts; ++part) {
      const int index = geometry.Parts()[part].Material;
      if (index < 0 || (size_t)index >= file.Materials().size()) { continue; }
      const outshine::Material &surface = file.Materials()[(size_t)index].Surface;
      if (!surface.Unlit) { continue; }
      for (size_t channel = 0; channel < 3; ++channel) {
        out[part][channel] = surface.BaseColour[channel];
      }
    }
    return true;
  }

  if (subject.MaterialFromFile()) {
    /* THE FILE OWNS THE COLOUR AND THE CASE OWNS THE CLOSURE, and the closure is one factor.
     * `diffuse`: the metal-rough model at metalness 0 under a uniform environment reduces to
     * `baseColour(u,v) * factor * L`. `emission`: the surface's radiance IS the declared colour, so
     * the environment leaves the arithmetic entirely -- which is what a subject whose surfaces see
     * one another has to declare, because there Cycles at one sample is measuring visibility
     * (board:0087). The texel is the shader's either way; this is the factor. */
    const bool emits = subject.MaterialKind == "emission";
    for (size_t part = 0; part < parts; ++part) {
      const int index = geometry.Parts()[part].Material;
      const outshine::Material surface = index >= 0 && (size_t)index < file.Materials().size()
                                             ? file.Materials()[(size_t)index].Surface
                                             : outshine::Material{};
      for (size_t channel = 0; channel < 3; ++channel) {
        const double factor = subject.Colour == FileColour::Emissive
                                  ? (double)surface.Emission[channel]
                                  : (double)surface.BaseColour[channel];
        out[part][channel] =
            (float)(factor * (emits ? 1.0 : subject.WorldRadiance[channel]));
      }
    }
    return true;
  }

  /* ONE COLOUR PER MATERIAL, WHICH IS THE KEY A MULTI-MATERIAL ASSET HAS. A per-NODE colour cannot
   * reach two primitives of one node that name different materials -- `SciFiHelmet` and
   * `AlphaBlendModeTest` are exactly that -- and the importer carries the material's own name
   * across, so the two sides key on the same string. */
  if (subject.MaterialKind == "emission-per-material") {
    const Json::Ref declared = material["colourLinearPerMaterial"];
    std::vector<std::string> matched;
    for (size_t part = 0; part < parts; ++part) {
      const int index = geometry.Parts()[part].Material;
      if (index < 0 || (size_t)index >= file.Materials().size()) {
        error = "part " + std::to_string(part) +
                " names no material, so a per-material colour has nothing to key on";
        return false;
      }
      const std::string &name = file.Materials()[(size_t)index].Name;
      const Json::Ref colour = declared[name.c_str()];
      if (colour.Size() != 3) {
        error = "scene.material.colourLinearPerMaterial declares no colour for material '" + name +
                "'";
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
            "', and this runner knows 'diffuse', 'emission' and 'emission-per-material'";
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

/* WHETHER A WEIGHT IS INTERPOLATED ANYWHERE IN THIS CASE'S PATH, asked of the surfaces that were
 * actually bound rather than of anything a manifest says. An image of one texel has no span to
 * interpolate across and a nearest-filtered one carries no weight, so neither puts the sampler term
 * into the bound -- and a case cannot acquire the term by declaring anything (I.26.15). */
[[nodiscard]] bool AnyLinearFilteredImage(const SurfaceTable &surfaces) {
  const auto interpolates = [](const outshine::Render::SubjectTexture &image) {
    return image.Rgba != nullptr && (image.Width > 1u || image.Height > 1u) &&
           image.Magnify == outshine::Render::SubjectFilter::Linear;
  };
  for (const outshine::Render::SubjectMaterial &slot : surfaces.Slots) {
    if (interpolates(slot.Colour) || interpolates(slot.Normal) || interpolates(slot.MetalRough) ||
        interpolates(slot.Emissive)) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool BuildSubject(Case &subject, std::string &error) {
  const std::string entry =
      subject.Manifest.Root()["subjects"][size_t{0}]["entry"].Str("scene.gltf");
  if (!subject.File.ReadFile(subject.Directory + entry)) {
    error = subject.File.Error();
    return false;
  }
  if (!subject.Geometry.Build(subject.File)) {
    error = subject.Geometry.Error();
    return false;
  }
  ResolveSurfaceTable(subject.File, subject.Geometry, subject.Surfaces);
  if (subject.MaterialFromFile() &&
      !ResolveFileSurface(subject.File, subject.Geometry, subject.Colour, subject.Carrier,
                          subject.Surfaces,
                          error)) {
    return false;
  }
  if (!ResolveEmission(subject, subject.File, subject.Geometry, subject.Emitted, error)) {
    return false;
  }
  subject.Path.LinearFilteredSampler = AnyLinearFilteredImage(subject.Surfaces);
  return ResolveCamera(subject, error);
}

struct Picture {
  std::vector<float> Depth;
  std::vector<uint8_t> Rgba;
  /* THE SCENE-REFERRED LINEAR TAP, RGBA f32: what the plan's `sceneLinear` holds, before any display
   * transfer. Determinism is judged on this and never on the PNG, whose 8-bit quantisation would
   * hide a difference the float buffer carries. */
  std::vector<float> Linear;
  /* THE NORMAL THE BRDF RECEIVED, xyzw per pixel (board:1122). WHETHER A LOBE WAS SHADED AT ALL IS
   * CARRIED BY THE VECTOR AND NOT BY w: the emissive arms write a zero VECTOR, and a zero-length
   * vector is not a direction, so the three-way excludes those pixels by that predicate and never by
   * an angular threshold -- an angle against a zero vector is meaningless and would read as a clean
   * 90 degrees on every one of them.
   *
   * `w` CARRIES THE FACING, +1 front and -1 back (board:1126). It was documented as marking the
   * shaded-ness above and nothing ever read it, which is the same shape as a metric whose name
   * misstates its instrument. It exists because both tangent assets declare `doubleSided`, so the
   * shader's back-face branch is reachable and the question of how many DISPUTED pixels are shaded
   * back-facing decides whether that branch's defect is the one under investigation. */
  std::vector<float> ShadingNormal;
  /* WHICH SURFACE SLOT THE FRAGMENT WORE, one value per pixel in `x`, one higher than the slot
   * (board:1138). It is the coverage predicate's missing half: `Depth` says a pixel is covered and
   * this says by WHAT, so a surface swap has a spelling that is not a number of codes. */
  std::vector<float> SurfaceIdentity;
};

/* THE RUNNER IS A CODE CONSUMER OF THE SETUP API and not a second engine: everything it asks for is
 * `Clients::Show`, which is the same call a scenario loader that declared a glTF subject would make.
 * Nothing about the placement or the frame mapping is decided here. */
[[nodiscard]] bool Capture(outshine::Render::Renderer &renderer,
                           const outshine::Clients::Studio &studio, Picture &out,
                           std::string &error) {
  outshine::Clients::StudioScratch scratch;
  if (!outshine::Clients::Show(renderer, studio, scratch, error)) { return false; }

  for (int frame = 0; frame < renderer.SettleFrames(); ++frame) { renderer.RenderFrame(); }
  /* NO FRAME IS DRAWN BETWEEN THE THREE READS, so no pace reaches the picture: each copies a target
   * that already exists and waits for that copy alone. How many frames a picture needs before it is
   * the picture is the PLAN's statement, above. */
  if (renderer.ReadDepth(out.Depth) != outshine::Render::ReadState::Ready) {
    error = "the depth readback did not complete";
    return false;
  }
  if (renderer.ReadSceneLinear(out.Linear) != outshine::Render::ReadState::Ready) {
    error = "the scene-referred linear readback did not complete";
    return false;
  }
  if (renderer.ReadPixels(out.Rgba) != outshine::Render::ReadState::Ready) {
    error = "the colour readback did not complete";
    return false;
  }
  if (renderer.ReadShadingNormal(out.ShadingNormal) != outshine::Render::ReadState::Ready) {
    error = "the shading-normal readback did not complete";
    return false;
  }
  if (renderer.ReadSurfaceIdentity(out.SurfaceIdentity) != outshine::Render::ReadState::Ready) {
    error = "the surface-identity readback did not complete";
    return false;
  }
  return true;
}

/* REVERSED-Z: the depth attachment is cleared to 0 at the far plane, so anything that wrote depth is
 * strictly greater. That is the whole of our coverage predicate and it reads no colour at all. */
Mask FromDepth(const std::vector<float> &depth, int width, int height) {
  Mask mask;
  mask.Width = width;
  mask.Height = height;
  mask.In.resize(depth.size());
  for (size_t pixel = 0; pixel < depth.size(); ++pixel) { mask.In[pixel] = depth[pixel] > 0.0f; }
  return mask;
}

/* THE ORACLE'S SIDE IS ITS ALPHA, which Cycles writes only for camera rays carrying the transparent
 * background flag -- an exact coverage channel that never touched the lighting. */
Mask FromOracle(const RawF32 &oracle) {
  Mask mask;
  mask.Width = oracle.Width();
  mask.Height = oracle.Height();
  mask.In.resize((size_t)mask.Width * (size_t)mask.Height);
  for (int y = 0; y < mask.Height; ++y) {
    for (int x = 0; x < mask.Width; ++x) {
      mask.In[(size_t)y * (size_t)mask.Width + (size_t)x] =
          oracle.At(x, y, oracle.Channels() - 1) > 0.0f;
    }
  }
  return mask;
}

/* THE ORACLE'S PIXELS IN OUR OUTPUT'S CURRENCY, and the transform is the standard one rather than a
 * house curve: `renders.default.colourManagement.viewTransform` is `Standard`, which is the sRGB
 * OETF over the scene-referred linear values and nothing else. The float dump is what is read, never
 * the oracle's own PNG, because the float is the reference and the PNG is for looking. */
uint8_t Srgb8(double linear) {
  const double clamped = linear < 0.0 ? 0.0 : (linear > 1.0 ? 1.0 : linear);
  const double encoded = clamped <= 0.0031308 ? 12.92 * clamped
                                              : 1.055 * std::pow(clamped, 1.0 / 2.4) - 0.055;
  const double level = encoded * 255.0 + 0.5;
  return (uint8_t)(level > 255.0 ? 255.0 : level);
}

/* THE ORACLE'S FRAME IN OUR OUTPUT'S FORM: RGB through the sRGB transfer, alpha carried straight
 * across as the coverage it already is. The three colour channels get a curve and the fourth does
 * not, because alpha is not a colour and encoding it would bend a coverage into a display code. */
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

  /* THE SAME DECLARATION TWICE IN ONE PROCESS. What this isolates is frame-to-frame state: a second
   * process would also change the allocator, the device and the shader cache, and a difference there
   * would not say which. It is judged on the linear tap and never on the PNG, because an 8-bit
   * quantisation hides a difference the float buffer carries. `CLAUDE.md`: the mathematics is
   * deterministic, and if pace decides the result the coupling is a bug. */
void ScoreDeterminism(const Case &subject, const outshine::Clients::Studio &studio,
                      outshine::Render::Renderer &renderer, const Picture &picture,
                      std::vector<Metric> &metrics) {
  using namespace outshine::Test;
  Picture again;
  std::string trouble;
  const bool twice = Capture(renderer, studio, again, trouble);
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
    if (Capture(renderer, studio, third, trouble) && third.Linear.size() == picture.Linear.size()) {
      size_t stable = 0;
      for (size_t at = 0; at < third.Linear.size(); ++at) {
        if (third.Linear[at] != picture.Linear[at]) { ++stable; }
      }
      Note("a third render differs from the first in", (double)stable, "halves");
    }
  }
}

  /* THE RADIANCE RESIDUAL, IN THE TAP'S OWN ALPHABET (board:0087). Zero is the bar
   * and there is nothing in it to nudge: the tap is f32 on both sides, so our float either is the
   * oracle's float or it is not.
   *
   * MEASURED ON EVERY CASE AND ENFORCED ON THE ONES THAT ARE ABOUT IT, and the split is the case's
   * own declaration rather than a choice here. A coverage case declares a flat colour in its
   * manifest and says in the same breath that nothing in the comparison reads it -- it is there so
   * the picture a person opens is not black -- so making that colour's last bit a verdict would be
   * enforcing something the case states it is not for. A case whose surface is the FILE's is about
   * the value, and there the number is the verdict. Both print it, so the residual is visible in
   * every log whether or not it decides that log's colour. */
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

/* EVERYTHING A CASE MUST PASS BEFORE ANYTHING IS RENDERED: the manifest, its inputs, its subject and
 * camera, the cached oracle and its shape, the oracle's own residual, the plan and the device. Each
 * of them ends the run the same way, so the sentence that ends it stands ONCE at the caller instead
 * of nine times here (`ES.3`) -- and each phase keeps its own `CHECK` at its own line, because the
 * line number is what sends a reader to the phase that stopped. */
enum class Prepared { Yes, No };

Prepared Prepare(Case &subject, RawF32 &oracle, size_t &seedApart,
                 outshine::Render::Renderer &renderer) {
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
    outshine::Test::Unprepared((subject.Directory + " is missing " + owed).c_str());
    return Prepared::No;
  }

  const bool loaded = BuildSubject(subject, why);
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
  /* The reference is still rendered and still lands in the directory; this is the line that says it
   * decides nothing here, so the disagreement stays visible until Blender gains what it lacks
   * (board:0085, condition three). */
  if (subject.Oracle == OracleRole::CannotExpressTheCriterion) {
    std::printf("ORACLE NOT-DECIDING -- %s\n",
                subject.Manifest.Root()["criterion"]["oracleLimitation"].Str("").c_str());
  }

  /* THE ORACLE IS READ BEFORE ANYTHING IS RENDERED. An absent reference is a property of the case,
   * and finding it out after a device bring-up would report a rendering failure for a missing file. */
  /* THE EXR IS THE ORACLE AND THE FLAT DUMP IS ITS CACHE (board:1119). */
  const bool haveOracle = oracle.ReadExrFile(subject.Directory + "oracle.exr");
  CHECK(haveOracle, "the cached oracle is present and decodes as the float image of this frame");
  if (!haveOracle) {
    Refused(oracle.Error());
    return Prepared::No;
  }
  const bool sameFrame = oracle.Width() == (int)subject.Frame.WidthPx &&
                         oracle.Height() == (int)subject.Frame.HeightPx;
  CHECK(sameFrame, "the oracle was rendered at the resolution the manifest's recipe declares");
  if (!sameFrame) {
    Refused("oracle.exr is " + std::to_string(oracle.Width()) + "x" +
            std::to_string(oracle.Height()) + " and the recipe declares " +
            std::to_string((int)subject.Frame.WidthPx) + "x" +
            std::to_string((int)subject.Frame.HeightPx));
    return Prepared::No;
  }

  /* THE ORACLE STATES ITS OWN RESIDUAL BEFORE IT JUDGES OURS, and for an emission case that residual
   * must be nothing at all: two seeds, the same bits, or the case fails on the ORACLE and not on us.
   * Why an emitter owes exactly this, and why the second recipe may differ in the seed alone, is
   * declared where the manifest is read (test/corpus/prep/manifest.py) and derived in
   * `board/` */
  if (Reduced(subject)) {
    RawF32 shifted;
    const bool haveShift = shifted.ReadExrFile(subject.Directory + "oracle.seed-shift.exr");
    CHECK(haveShift, "the emission case carries a second oracle rendered at another seed");
    if (!haveShift) {
      Refused(shifted.Error());
      return Prepared::No;
    }
    const bool sameShape = shifted.Width() == oracle.Width() &&
                           shifted.Height() == oracle.Height() &&
                           shifted.Channels() == oracle.Channels();
    CHECK(sameShape, "the two seeds were rendered into the same frame");
    if (!sameShape) {
      Refused("oracle.seed-shift.exr is not the shape oracle.exr is");
      return Prepared::No;
    }
    for (int y = 0; y < oracle.Height(); ++y) {
      for (int x = 0; x < oracle.Width(); ++x) {
        for (int channel = 0; channel < oracle.Channels(); ++channel) {
          if (oracle.At(x, y, channel) != shifted.At(x, y, channel)) { ++seedApart; }
        }
      }
    }
  }

  /* THE CASE'S OWN DECLARATION, and it is the whole of what will be created and encoded. One content
   * stage and two requested outputs: the depth the coverage predicate reads, and the picture a person
   * opens. No light model, no atmosphere chain, no shadow, no occlusion, no temporal resolve, no
   * present -- none of them is switched off here, none of them is in the plan at all.
   *
   * `Transfer::Linear` because the oracle's own view transform is `Standard`, which is the sRGB
   * transfer function over scene-referred linear values and nothing else, and the frame target is
   * sRGB-encoding: a curve here would be measuring the curve (board:0087). */
  outshine::Render::PlanSpec declaration;
  /* THE SHADING NORMAL IS REQUESTED, WHICH IS WHAT ATTACHES IT (board:1121, board:1122). The plan
   * prunes a target nothing reads, so asking for it here is the whole of why it exists in this
   * plan and in no other. */
  /* THE SURFACE IDENTITY IS REQUESTED FOR THE SAME REASON AND ON THE SAME TERMS (board:1138): the
   * plan prunes a target nothing reads, so no plan outside this runner pays for it. */
  declaration.Outputs = {outshine::Render::Resource::SceneDepth,
                         outshine::Render::Resource::SceneShadingNormal,
                         outshine::Render::Resource::SceneSurfaceIdentity,
                         outshine::Render::Resource::FrameTex};
  declaration.Content = {outshine::Render::Stage::Subjects};
  declaration.Display =
      outshine::Render::Declared<outshine::Render::Transfer>(outshine::Render::Transfer::Linear);
  declaration.Exposure = outshine::Render::Declared<float>(1.0f);
  /* THE TAP IS f32 BECAUSE THE VALUE IS THE VERDICT (board:0087). At rgba16float the
   * store's own rounding was 63x the arithmetic term and every channel of the flat cases sat exactly
   * one binary16 step low -- the format speaking, not the engine. The rule this obeys is that a rung
   * needing tighter than the storage floor changes the storage and never the threshold. */
  declaration.Precision = outshine::Render::Declared<outshine::Render::ScenePrecision>(
      outshine::Render::ScenePrecision::Float);
  std::shared_ptr<const outshine::Render::RenderPlan> plan;
  const bool compiled = outshine::Render::RenderPlan::Compile(declaration, &plan, why);
  CHECK(compiled, "the case's render declaration compiles");
  if (!compiled) {
    Refused(why);
    return Prepared::No;
  }
  std::printf("PLAN %s %d passes, %d stages\n", plan->Digest().c_str(), plan->PassCount(),
              (int)plan->Order().size());

  renderer.Init((int)subject.Frame.WidthPx, (int)subject.Frame.HeightPx, plan);
  const bool usable = renderer.DeviceUsable();
  CHECK(usable, "the device came up, so the case can be rendered at all");
  if (!usable) {
    Refused("no usable device");
    return Prepared::No;
  }

  return Prepared::Yes;
}

/* THE TWO CONDITIONS OF THE EXACTNESS CONSTRUCTION, RECOMPUTED FROM THE PROJECTED GEOMETRY AND HELD
 * WHERE THE CASE CLAIMS THEM (`board/`). Published on every case and enforced on
 * an `exact` one: the counts and residuals say, for a case that cannot claim it, exactly what stands
 * in the way -- how many distinct silhouette lines its freedoms would have to satisfy, and whether
 * any of them is straight at a rational slope at all. */
void ScoreExactnessConstruction(const Case &subject, const EdgeSet &silhouette, double tieMarginPx,
                                std::vector<Metric> &metrics) {
  const Exactness measured = Measure(silhouette);
  const bool claimed = subject.Placement == ExactnessClass::Exact;
  metrics.push_back({"silhouette_lines", (double)measured.LineCount(), 0.0, "lines",
                     Direction::Reported});
  metrics.push_back({"silhouette_edges", (double)measured.SilhouetteEdges, 0.0, "edges",
                     Direction::Reported});
  /* CONDITION (A), IN PIXELS AND NOT IN RADIANS: how far the far endpoint of the worst silhouette
   * edge sits off the rational line fitted to it. An angular residual says nothing until it is
   * multiplied by the edge's own length, and it is the length that makes an irrational slope
   * unrecoverable. The threshold is the oracle's own filter half-width -- a deviation under it is
   * below the reference's resolution and nothing about it is decidable. */
  metrics.push_back({"exactness_slope_residual_px", measured.SlopeResidualPx, subject.OracleFloorPx,
                     "px", claimed ? Direction::AtMost : Direction::Reported});
  /* CONDITION (B): the distance from every pixel centre in the plane to the nearest silhouette line,
   * which is a constant of the line and not a draw. Against the ruled floor, ten times the oracle's
   * jitter. */
  metrics.push_back({"exactness_margin_px", measured.MarginPx, kMarginFloorPx, "px",
                     claimed ? Direction::AtLeast : Direction::Reported});
  /* What the same slopes would deliver if every constant sat at half a lattice step -- the ceiling
   * the offset condition is measured against, so a margin that is short says whether the slope or
   * the placement is what fell short. */
  metrics.push_back({"exactness_margin_ceiling_px", measured.CeilingPx, 0.0, "px",
                     Direction::Reported});
  /* ONE QUANTITY REACHED TWO WAYS, and on a constructed case they must be the same number. The
   * margin above is PREDICTED from the line constants over the whole integer lattice; `Ties.h`
   * MEASURES it over the boundary pixels this render actually produced. On a subject whose slope is
   * not rational the prediction means nothing and they may differ by anything, which is why the
   * claim is made only where the case claims the construction. The tolerance is nine digits below a
   * pixel: both sides are the same projected doubles put through different arithmetic on quantities
   * of order 1000 px, so their disagreement floor is around 1e-10 px. */
  constexpr double kOneQuantityPx = 1e-9; /* [SET] raster pixels */
  metrics.push_back({"exactness_margin_agreement_px",
                     std::fabs(tieMarginPx - measured.MarginPx), kOneQuantityPx, "px",
                     claimed ? Direction::AtMost : Direction::Reported});
  /* THE LINES THEMSELVES, WHICH IS WHAT A CONSTRUCTION IS WRITTEN FROM: the slope to roll to and the
   * constant to place. Capped, because a subject with no lattice structure has as many lines as it
   * has silhouette edges and printing 1.5 million of them says nothing the count above did not. */
  constexpr size_t kLinesPrinted = 16; /* [SET] */
  for (size_t line = 0; line < measured.Lines.size() && line < kLinesPrinted; ++line) {
    const LatticeLine &fit = measured.Lines[line];
    std::printf("NOTE   silhouette line %ld x - %ld y = %.9f over %zu edges: margin %.9g px of "
                "%.9g px, slope residual %.3g px\n",
                fit.P, fit.Q, fit.C, fit.Edges, fit.MarginPx, fit.CeilingPx, fit.SlopeResidualPx);
  }
}

/* THE VISIBILITY ESTIMATOR'S OWN DISPLACEMENT, IN SCREEN PIXELS (board:0089). A
 * shadow boundary is a coverage boundary of the LIGHT's visibility, so a disagreement about it is
 * bounded in the predicate's own geometry and never in codes. Our estimator is an exact ray and the
 * only thing that displaces it is where the ray starts: `ShadowRay.h`'s self-intersection bias, a
 * fixed fraction of the subject's own diagonal, which the device half reads and this reads back.
 *
 * PROJECTED THROUGH THE CASE'S OWN CAMERA AND NOT THROUGH AN ASSUMED ONE: the raster displacement of
 * a world displacement of that magnitude, taken as the operator norm of the finite-difference
 * Jacobian at the subject's centre, so a perspective case and a parallel case are answered by one
 * expression. THE NUMBER IS DERIVED AND NOT FITTED -- it shrinks when the bias does, and no case can
 * widen it, which is the property that separates it from a tolerance. */
void ScoreVisibilityTerm(const Case &subject, const Transform &clip, double biasM,
                         std::vector<Metric> &metrics) {
  double centre[3];
  subject.Geometry.CentreM(centre);
  double at[2];
  double ndc[3];
  clip.Point(centre, ndc);
  subject.Frame.Raster(ndc, at);
  /* The 2x3 Jacobian by one-sided differences over the bias itself, so the step IS the displacement
   * being bounded and no second length enters. */
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
  /* The operator 2-norm of a 2x3 matrix is the square root of the largest eigenvalue of J*J^T, and
   * for the 2x2 symmetric J*J^T that is closed form -- so the worst direction is answered rather
   * than the three axes sampled, which would understate a diagonal one. */
  double gram[3] = {0, 0, 0}; /* xx, xy, yy */
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

/* TWO OF OUR OWN RENDERS, AND NO ORACLE IN IT AT ALL. A second spelling of the same surface -- the
 * same placement through a `matrix`, the same triangles under another index width, the same quad as
 * a strip or a fan -- must land in the same pixels, and that claim is DECIDABLE: it is exact, not
 * within a tolerance, so its threshold is zero disagreeing pixels and it needs no reference. */
void ScoreAlternateSpellings(const Case &subject, const outshine::Clients::Studio &studio,
                             outshine::Render::Renderer &renderer, const Mask &ours,
                             std::vector<Metric> &metrics) {
  const Json::Ref identical = subject.Manifest.Root()["identicalCoverage"];
  for (size_t which = 0; which < identical.Size(); ++which) {
    const std::string name = identical[which].Str("");
    Document alternate;
    Subject spelling;
    Picture again;
    std::string trouble;
    bool built = alternate.ReadFile(subject.Directory + name);
    if (!built) {
      trouble = alternate.Error();
    } else if (!(built = spelling.Build(alternate))) {
      trouble = spelling.Error();
    } else {
      outshine::Clients::Studio other = studio;
      other.Geometry = &spelling;
      /* RESOLVED AGAINST THE ALTERNATE'S OWN NODES AND ITS OWN MATERIALS, not copied from the
       * entry's. The two spell one surface, so their names agree -- and if they ever did not,
       * copying by position would colour the wrong body while the count still matched. */
      SurfaceTable surfaces;
      ResolveSurfaceTable(alternate, spelling, surfaces);
      built = (!subject.MaterialFromFile() ||
               ResolveFileSurface(alternate, spelling, subject.Colour, subject.Carrier, surfaces,
                                  trouble)) &&
              ResolveEmission(subject, alternate, spelling, other.EmittedRadiance, trouble);
      other.PartSurface = surfaces.PartSlot;
      other.Surfaces = surfaces.Slots;
      built = built && Capture(renderer, other, again, trouble);
    }
    CHECK(built, ("the alternate spelling " + name + " reads, builds and renders").c_str());
    if (!built) {
      Refused(trouble);
      continue;
    }
    const Mask other = FromDepth(again.Depth, ours.Width, ours.Height);
    metrics.push_back({"differs_from_" + name, (double)Disagreeing(ours, other), 0.0, "px",
                       Direction::AtMost});
  }
}

/* WHAT THE ASSET ITSELF SAYS MUST HOLD, on the linear tap, before anything is compared with the
 * oracle. Its caller places it ahead of the image comparison because for a `stated-invariant` case
 * this IS the verdict and the comparison below it is the diagnostic beside it -- the reverse of
 * every other kind, and the reason the two are never both enforced on one case. */
/* THE ORACLE'S PIXELS IN THE SHAPE THE INVARIANTS ARE COMPUTED ON. `RawF32` interleaves by its own
 * channel count, which is 3 or 4 depending on what Cycles wrote, and `LinearFrame` reads a stride of
 * four -- so this repacks rather than casting, and a subject with no alpha reads as covered
 * everywhere, which is what an invariant over a declared rectangle wants. */
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

  /* THE SAME QUESTION PUT TO THE ORACLE, AND IT IS NOT A CONVENIENCE (board:1131). A `region-compare`
   * puts two rectangles of ONE render beside each other, so its bound is a property of the subject AND
   * of the engine both populations were measured on. That is the instrument-domain failure in its
   * population-too-small face: with one engine the number cannot separate *we broke it* from *this is
   * what the technique does*, and it decided a shipping question on exactly that ambiguity -- filtering
   * a normal map flattens it, and the geometry the flattened quad imitates does not flatten.
   *
   * REPORTED AND NEVER ENFORCED. The oracle is what our pixels are judged against; it is not a second
   * subject with its own thresholds, and giving it one would be two verdicts over one comparison. */
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

/* THE STUDIO THE CASE DECLARES. The file's lights cross only where the case says so: every other
 * case is lit by nothing and draws the radiance it declared, which is what keeps a rung measuring
 * the light we meant. */
outshine::Clients::Studio MakeStudio(const Case &subject) {
  outshine::Clients::Studio studio;
  studio.Geometry = &subject.Geometry;
  studio.Eye = subject.Eye;
  studio.EmittedRadiance = subject.Emitted;
  studio.PartSurface = subject.Surfaces.PartSlot;
  studio.Surfaces = subject.Surfaces.Slots;
  if (subject.Lights == SceneLights::FromFile) {
    for (const outshine::Gltf::PlacedLight &placed : subject.Geometry.Lights()) {
      studio.Lights.push_back(placed.Light);
    }
  }
  if (subject.Lights == SceneLights::DeclaredSun) { studio.Lights.push_back(subject.Sun); }
  return studio;
}

/* WHICH SURFACE THE PICTURE IS OF, and it is the criterion silhouette, coverage and hue cannot be.
 * Swapping the near shell of a closed body for its far one with the shading normals reversed leaves
 * the silhouette, the coverage and the hue all unchanged -- three criteria simultaneously blind,
 * each individually correct -- and `DirectionalLight` shipped in exactly that state without any of
 * them noticing (board:0073). Range is not invariant under it.
 *
 * THE RANGE IS ALONG THE VIEW RAY AND NOT ALONG THE BORESIGHT, which is what the cosine is: the
 * depth attachment carries `kNearM / rangeAlongBoresight`, and a probe away from the frame centre
 * that skipped the correction would report a distance the geometry does not have. */
[[nodiscard]] bool RangeAt(const outshine::Gltf::Placement &eye, const outshine::Gltf::Viewport &frame,
                           const std::vector<float> &depth, int column, int row, double &out,
                           std::string &error) {
  if (eye.Kind != outshine::Gltf::CameraKind::Perspective) {
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
  out = (double)outshine::Render::Renderer::kNearM / (double)depth[at] * secant;
  return true;
}

void ScoreDepthProbes(const Case &subject, const outshine::Clients::Studio &studio,
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
    if (!RangeAt(studio.Eye, subject.Frame, depth, probe["atPx"][(size_t)0].Int(-1),
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

/* WHAT THE RUN WAS GIVEN, printed before it is rendered, so a picture that comes out wrong can be
 * attributed to the declaration rather than to the renderer without a second run. */
void NoteWhatTheStudioCarries(const Case &subject, const outshine::Clients::Studio &studio) {
  if (subject.Lights == SceneLights::FromFile) {
    for (const outshine::Gltf::PlacedLight &placed : subject.Geometry.Lights()) {
      outshine::Test::Note(
          ("light '" + placed.LightName + "' on node '" + placed.NodeName + "', intensity").c_str(),
          (double)placed.Light.Intensity,
          placed.Light.Kind == outshine::LightKind::Directional ? "lux" : "candela");
    }
  }
  outshine::Test::Note("punctual lights the studio declares", (double)studio.Lights.size(),
                       "lights");
  for (size_t part = 0; part < subject.Geometry.Parts().size(); ++part) {
    outshine::Test::Note(
        ("declared radiance of node '" + subject.Geometry.Parts()[part].NodeName + "', red").c_str(),
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
  }
}

/* THE FILE'S OWN DECLARED `NORMAL`, RASTERISED (board:1122). The third leg, and the only one that
 * ADJUDICATES: ours and Cycles are the same quantity computed twice, so where they differ neither is
 * evidence about the other. The accessor is authored outside this tree.
 *
 * INTERPOLATED THE WAY A RASTERISER INTERPOLATES IT -- barycentric over the projected triangle, in
 * the glTF frame the accessor is already in -- and depth-ordered by the convention THIS projection
 * actually uses: `gltf/Camera.h` puts NDC z in [-1, +1] with -1 AT THE NEAR PLANE, which is
 * explicitly not the engine's reversed-Z. Nearer is LESSER here, and keeping the greater one kept
 * the FAR surface: on a closed body that is the back, whose normal points away, and it reported p50
 * around 90-115 degrees on `water-bottle`, `boom-box`, `corset` and `lantern` while the open grid of
 * `normal-tangent-mirror` was unaffected because it has no back to pick. It is
 * the geometric normal the normal map perturbs and not the perturbed one, which is exactly what
 * makes it the adjudicator: a disagreement between the other two is a disagreement about the
 * PERTURBATION, and this is the frame the perturbation is relative to. */
struct DeclaredNormals {
  std::vector<float> Xyz; /* 3 per pixel, glTF frame; zero where no triangle covers the pixel */
  std::vector<float> Depth;
};

DeclaredNormals RasteriseDeclaredNormals(const Subject &geometry, const Transform &clip,
                                         const Viewport &viewport, int width, int height) {
  DeclaredNormals out;
  out.Xyz.assign((size_t)width * (size_t)height * 3u, 0.0f);
  out.Depth.assign((size_t)width * (size_t)height, 2.0f);
  if (geometry.Normals().size() < geometry.PositionsM().size()) { return out; }
  const std::vector<uint32_t> &indices = geometry.Indices();
  /* PER PART, AND THE INDEX RUN IS THE PART'S OWN. Walking `Indices()` as one flat list is right for
   * a subject of one primitive and wrong for every other: a part's triangles start at
   * `part.FirstIndex`, so a flat walk reads one part's indices as another's and interpolates
   * normals across a seam that does not exist. It reported p50 103 degrees on `water-bottle` and
   * `boom-box`, and BOTH legs returned the same wrong number, which is what said the fault was
   * here rather than in either of them. */
  for (const outshine::Gltf::Part &part : geometry.Parts()) {
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
        /* NEARER IS LESSER in this projection (`gltf/Camera.h`), so the nearest surface is the
         * smallest z and the buffer starts beyond the far plane. */
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

/* THE FILE'S MATERIAL NAMES, in the file's own order, which is the currency the two sides state a
 * surface identity in (board:1138). An unnamed material yields an empty string and therefore matches
 * nothing: the correspondence is by name, and a file that named none of its materials has no
 * correspondence to derive rather than a set of empty matches. */
[[nodiscard]] std::vector<std::string> FileMaterialNames(const Document &file) {
  std::vector<std::string> names;
  names.reserve(file.Materials().size());
  for (const outshine::Gltf::MaterialRef &material : file.Materials()) {
    names.push_back(material.Name);
  }
  return names;
}

/* WHAT EACH SIDE PUTS AT ONE PIXEL, PRINTED (board:1138). The pixels are the ones the two sides
 * NAME DIFFERENTLY, so the population is derived from the reading and a case whose sides agree
 * prints nothing at all -- a coordinate written down here would go stale at the first reframing and
 * then read as a finding.
 *
 * THE ORACLE'S OWN SPLIT IS PRINTED UNDER ITS OWN WORD, because it is a different fact: there the
 * oracle's index pass and the oracle's picture name different materials, and neither line is about
 * us. Two facts under one prefix is how the second one gets read as the first. */
void NoteDisagreements(const outshine::Render::Parity::IdentityReading &reading) {
  for (const outshine::Render::Parity::Disagreement &where : reading.Disagreements) {
    std::printf("SURFACE-AT %d,%d oracle=%s ours=%s\n", where.X, where.Y,
                where.Oracle.Name.c_str(), where.Ours.Name.c_str());
  }
  for (const outshine::Render::Parity::Disagreement &where : reading.Splits) {
    std::printf("SURFACE-ORACLE-SPLIT %d,%d its index says %s and its picture does not\n", where.X,
                where.Y, where.Oracle.Name.c_str());
  }
}

/* THE COLOUR THE ORACLE'S PICTURE MUST CARRY WHERE ITS INDEX PASS NAMES A MATERIAL (board:1138),
 * derived from what the runner already resolved and from nothing read a second time: the radiance
 * each part was declared to emit, gathered onto the file material that part wears.
 *
 * IT IS COMPUTABLE ONLY UNDER THE DECLARED ARMS. Where the appearance is the file's own image times
 * a factor, or a BRDF against a light list, the picture at a pixel is not a constant per material
 * and there is no closed form to hold it against -- so this refuses by name instead of comparing
 * against the factor alone, which would call every textured pixel a split.
 *
 * TWO PARTS OF ONE MATERIAL DECLARING DIFFERENT RADIANCE IS A REFUSAL AND NOT A LAST WRITE. The
 * per-material arm keys on the material, so a disagreement there means the resolution above did
 * something this derivation cannot express, and taking either value would hide it. */
[[nodiscard]] outshine::Render::Parity::DeclaredColours ColoursPerFileMaterial(const Case &subject) {
  outshine::Render::Parity::DeclaredColours out;
  if (subject.MaterialFromFile() || subject.ShadedByLights()) {
    out.Why = "the case takes its appearance from the file's own materials, so the oracle's picture "
              "is not one colour per material and its index pass cannot be held against it";
    return out;
  }
  const size_t materials = subject.File.Materials().size();
  out.ByFileMaterial.assign(materials, {0.0f, 0.0f, 0.0f});
  out.Known.assign(materials, 0u);
  for (size_t part = 0; part < subject.Geometry.Parts().size() && part < subject.Emitted.size();
       ++part) {
    const int material = subject.Geometry.Parts()[part].Material;
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

/* WHICH SURFACE EACH SIDE PUTS AT EACH PIXEL, AND WHERE THEY DISAGREE (board:1138).
 *
 * IT IS REPORTED AND IT IS NOT A BOUND. What this count feeds is a routing decision inside the
 * picture bound, and that is its own work item; a threshold here would be a number nobody derived.
 *
 * THE OBJECT PASS IS READ FOR ITS DISCRIMINATION AND NOT FOR A VERDICT, because our side carries no
 * node identity to hold it against: the compiled draw list merges the primitives of several nodes
 * into one call whenever they share a material, so the finest identity a fragment can carry through
 * the per-slot uniform is the SURFACE. Publishing how many distinct objects the pass separates says
 * what a node-level comparison would have to work with, and claims nothing our side can answer. */
void ScoreSurfaceIdentity(const Case &subject, const Picture &picture, const RawF32 &oraclePicture,
                          const Mask &ours, const Mask &theirs, std::vector<Metric> &metrics) {
  using namespace outshine::Test;
  using namespace outshine::Render::Parity;

  const std::vector<std::string> names = FileMaterialNames(subject.File);
  OracleSurfaces oracle;
  if (!oracle.Read(subject.Directory, IndexPass::Material, names)) {
    Refused(oracle.Error());
    return;
  }
  if (oracle.Width() != theirs.Width || oracle.Height() != theirs.Height) {
    Refused("the oracle's material-index pass is not the shape its picture is");
    return;
  }
  if (picture.SurfaceIdentity.size() < (size_t)ours.Width * (size_t)ours.Height * 4u) {
    Refused("the surface-identity attachment does not cover the frame we rendered");
    return;
  }

  const OurSurfaces mine(picture.SurfaceIdentity, ours.Width, subject.Surfaces.Material, names);
  const DeclaredColours declared = ColoursPerFileMaterial(subject);
  const IdentityQuestion asked{oracle, mine, theirs, ours, oraclePicture, declared};
  const IdentityReading reading = ReadSurfaceIdentity(asked);

  /* THE ONE CLAIM THIS READER ENFORCES, and it is about our own plumbing rather than about the
   * oracle (board:1138): every pixel our depth says we drew carries a surface slot our own table
   * holds. It is the property that makes every count below mean anything -- an attachment the
   * encoder never wrote would read as slot -1 everywhere and the comparison would be a comparison
   * with the target's clear. */
  CHECK(reading.OursNamingNoSlot == 0,
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
  /* ABSENT AND NOT ZERO WHERE THE ORACLE'S TWO PRODUCTS CANNOT BE HELD AGAINST EACH OTHER: a zero
   * there would say "the oracle never contradicts itself on this case", which is a claim this
   * instrument has no way to make. */
  metrics.push_back({"surface_oracle_index_unlike_its_own_picture",
                     declared.Computable ? (double)reading.OracleSplit : std::nan(""), 0.0, "px",
                     Direction::Reported});
  metrics.push_back({"surface_identity_disagreeing_attributable",
                     reading.AttributionKnown ? (double)reading.Attributable : std::nan(""), 0.0,
                     "px", Direction::Reported});
  /* THE DOMAIN IS REPORTED WHERE IT BITES AND NOWHERE ELSE. A case with nothing to attribute is not
   * short of an instrument, so saying so on every such case would put a refusal beside a clean
   * reading and teach a reader to skip both. */
  if (!reading.AttributionKnown) { Refused("surface identity: " + declared.Why); }
  if (!reading.Adjudicated) { Refused("surface identity: " + reading.Refusal); }
  NoteDisagreements(reading);

  /* THE OBJECT PASS'S DISCRIMINATION, on its own, because it is a different partition of the same
   * frame and a count of one there means a node-level question cannot be asked of this case either. */
  OracleSurfaces objects;
  if (!objects.Read(subject.Directory, IndexPass::Object, names)) {
    Refused(objects.Error());
    return;
  }
  metrics.push_back({"surface_oracle_distinct_objects",
                     (double)DistinctOracleIndices(objects, theirs), 0.0, "indices",
                     Direction::Reported});
}

/* THE THREE LEGS OF THE NORMAL COMPARISON, PUBLISHED RATHER THAN DIFFERENCED (board:1122).
 *
 * WHY THREE. Ours against Cycles says the two differ and NOTHING about which is wrong. The file's own
 * `NORMAL` accessor is the third party, authored outside this tree, and it is what separates an
 * engine fix from `reduce the oracle`: if Cycles matches the file and we do not, the defect is ours;
 * if we match and Cycles does not, Cycles is shading normals the glTF never declared.
 *
 * THE FRAME MAP IS BLENDER-WORLD TO glTF AND IT IS VALIDATED, not assumed: `(x, y, z) -> (x, z, -y)`,
 * unit length preserved to one f32 ulp, residual tilt 0.3174 degrees DERIVED from the normal
 * texture's own 8-bit quantisation -- `128/255*2-1 = 0.00392` per axis over two axes. So a
 * disagreement above ~0.4 degrees is real and the effect under investigation is 4.2 to 10.3 degrees.
 *
 * PIXELS WITH NO SHADING NORMAL ARE EXCLUDED BY PREDICATE AND NEVER BY THRESHOLD. The emissive arms
 * shade no lobe and write a declared zero VECTOR; an angle against a zero vector is meaningless and
 * would come back as a clean 90 degrees on every one of them, which reads as a finding. The count of
 * excluded pixels is published beside the result, because an exclusion nobody counts is a mask. */
void ScoreShadingNormal(const Case &subject, const Picture &picture, const Mask &ours,
                        const Transform &clip, std::vector<Metric> &metrics) {
  using namespace outshine::Test;
  RawF32 cycles;
  const std::string path = subject.Directory + "oracle.normal.raw";
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

  const DeclaredNormals declared = RasteriseDeclaredNormals(subject.Geometry, clip, subject.Frame,
                                                            ours.Width, ours.Height);
  /* THE FILE'S LEG BESIDE THE OTHER TWO (board:1126). Three quantities compared inside one process
   * and none of them openable is an investigation that has to be re-run to be questioned. */
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
  /* [DERIVED] THE SIGNAL THRESHOLD, AND IT IS DELIBERATELY NOT THE FLOOR. The two legs' median
   * disagreement is [MEASURED] 0.00099 degrees on the tangent assets and 0.0129 on `water-bottle` --
   * so a floor-selected population admitted 147 669 pixels at a median margin of 0.0025 degrees, and
   * gave a 2:1 count over a population that mostly agrees, quoted about a 9.48 degree tail it does not
   * contain. A pixel carries an opinion about which leg is right only once the two differ by more than
   * the term BOTH were validated against: the normal texture's own 8-bit quantisation, 128/255 per axis
   * over two axes = 0.3174 degrees. Rounded up to 0.4, so every admitted pixel disagrees by more than
   * the asset is able to express, and the floor -- three hundred times smaller -- is what makes that
   * rounding safe rather than arbitrary. */
  constexpr double kNormalSignalDeg = 0.4;
  size_t shaded = 0, noLobe = 0, uncovered = 0, adjudicated = 0;
  size_t disputed = 0, oursNearer = 0, cyclesNearer = 0;
  std::vector<double> disputedMargin;
  /* WHERE ACROSS THE FRAME THE DISPUTE SITS, in four equal vertical bands. `normal-tangent` and its
   * mirror are regular grids whose columns differ in ONE declared thing -- real geometry, a normal map,
   * a V-mirrored tangent, a U-mirrored tangent -- so a dispute concentrated in one band names which
   * declared thing it is about, and a dispute spread evenly says the grid is not the axis at all. The
   * band is a position in the image and NOT a claim that band N is column N: that mapping is by layout
   * and is not read from the file. */
  size_t bandDisputed[4] = {0, 0, 0, 0};
  size_t shadedBack = 0, disputedBack = 0;
  double worstDeg = 0, sumDeg = 0;
  std::vector<double> degrees, oursVsFile, cyclesVsFile;
  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      if (!ours.At((int)x, (int)y)) { ++uncovered; continue; }
      const size_t at = (y * width + x) * 4u;
      /* OUR LEG IS IN THE ENGINE'S FRAME AND THE COMPARISON IS IN glTF's, so it is mapped back by
       * the inverse of the permutation `GltfStudio::EcefFromGltf` applied at upload --
       * `ecef = (gltf.y, gltf.x, -gltf.z)`, so `gltf = (ecef.y, ecef.x, -ecef.z)`. It is a signed
       * permutation of determinant +1, its own inverse transpose, so a normal stays a normal and
       * stays unit under it. THIS WAS THE DEFECT THE FIRST READING FOUND: the oracle's leg had been
       * validated and ours never had, and a frame error and a shading-normal defect look identical
       * -- 179 degrees on one case and 107 on another, which no sign error can produce. */
      const double ex = picture.ShadingNormal[at], ey = picture.ShadingNormal[at + 1],
                   ez = picture.ShadingNormal[at + 2];
      const double ox = ey, oy = ex, oz = -ez;
      const double oursLength = std::sqrt(ox * ox + oy * oy + oz * oz);
      /* THE PREDICATE, and it is length rather than a small angle: a zero vector is not a direction
       * and no tolerance can make it one. */
      if (oursLength <= 0.0) { ++noLobe; continue; }
      const bool backFacing = picture.ShadingNormal[at + 3] < 0.0f;
      if (backFacing) { ++shadedBack; }
      /* Blender world to glTF: the importer maps glTF +Z to Blender -Y. */
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

      /* THE ADJUDICATOR. Where no triangle of the file covers this pixel there is nothing to
       * adjudicate WITH, so it is skipped by the same kind of predicate the no-lobe pixels are --
       * a zero-length declared normal is not a direction either. */
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

      /* WHICH LEG THE DECLARATION AGREES WITH, OVER THE PIXELS THAT DISAGREE AT ALL. The two legs
       * agree to a floor of 0.001 degrees, but SELECTING AT THE FLOOR WAS THE WRONG POPULATION: it
       * admits pixels that disagree by barely more than the instrument's own limit, and they have no
       * opinion about which leg is right. The question is the 9.48 degree tail, so the selection is
       * the SIGNAL threshold -- above the 0.3174 degree term both legs were validated against, which
       * is the point past which a disagreement cannot be the texture's quantisation. THE POPULATION
       * SIZE IS PUBLISHED FIRST: a verdict over two hundred pixels and one over two hundred thousand
       * are different claims, and the first one is not a verdict. */
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
  /* THE THREE LEGS, PUBLISHED SIDE BY SIDE. `ours vs cycles` says they differ; the two against the
   * FILE say which of them the declaration agrees with, and that is the whole of the branch. */
  std::sort(oursVsFile.begin(), oursVsFile.end());
  std::sort(cyclesVsFile.begin(), cyclesVsFile.end());
  Note("shading normal, pixels the file adjudicates", (double)adjudicated, "px");
  /* THE POPULATION BEFORE THE VERDICT. */
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
    /* `cyclesVsFile - oursVsFile` at the median: POSITIVE means the declaration sits nearer ours. */
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

/* THE PICTURE BOUND, SCORED AND PUBLISHED (board:0089). The bound is the sum of the
 * terms this case's own path puts in it, the metric is the tail, and the histogram beside it is
 * unbounded by design -- 519 pixels at one code and 5 190 pixels at one code are equally acceptable,
 * and one pixel past the tail is red.
 *
 * IT REPLACED THE PNG COMPARISON RATHER THAN JOINING IT. `image_pixels_differing` asked the same
 * question -- whole image, RGBA, ours against the oracle's -- through two 8-bit encodings, so its
 * own rounding was inside the answer and its unit could not be smaller than a code. Two instruments
 * for one claim is how the two come to disagree.
 *
 * THE BUCKET COUNTS ARE PRINTED SPARSELY. A case at zero prints one line saying so, and a case with
 * a tail prints the buckets that hold something -- 256 lines of zeros would bury the tail that
 * decides it. */
void SayWhereItWas(const char *kind, const Excursion &worst) {
  using namespace outshine::Test;
  if (worst.Code <= 0.0) { return; }
  std::printf("NOTE   worst %s disagreement: %.9g codes at (%zu, %zu) channel %zu, ours %.9g "
              "against %.9g, over %zu px\n",
              kind, worst.Code, worst.X, worst.Y, worst.Channel, worst.Ours, worst.Theirs,
              worst.Pixels);
}

void ScorePictureBound(const PictureDelta &picture, const Tail &bound,
                       std::vector<Metric> &metrics) {
  for (const BoundTerm &term : bound.Terms) {
    std::printf("BOUND  %-56s %14.9g codes\n", term.Mechanism.c_str(), term.Codes);
  }
  if (!bound.Enforced) {
    std::printf("BOUND  %-56s %14s\n",
                "the oracle still estimates, so no tail bound may be enforced", "--");
  }
  metrics.push_back({"picture_max_delta_code", picture.Appearance.Code, bound.Codes, "codes",
                     bound.Enforced ? Direction::AtMost : Direction::Reported, Count::Picture});
  /* A PREDICATE HAS ONE VALUE WHERE THE TWO SIDES AGREE IT APPLIES, so zero here is a statement and
   * not a tolerance: an oracle alpha between 0 and 1 over our `covered(sceneDepth)` is the blended
   * -surface defect, and this is the gate that reaches it. */
  metrics.push_back({"picture_max_delta_code_alpha", picture.Predicate.Code, 0.0, "codes",
                     Direction::AtMost, Count::Picture});
  metrics.push_back({"picture_max_delta_code_routed", picture.Routed.Code, 0.0, "codes",
                     Direction::Reported, Count::Picture});
  /* WHERE THE ORACLE SAYS NO LIGHT REACHED THIS PIXEL AND WE DISAGREE. Reported, because it is a
   * DIAGNOSTIC over a population and the verdict is the tail above; it exists because eight of the
   * thirteen cases outside the bound have a worst pixel of exactly this shape and a worst pixel is an
   * anecdote until it is counted. */
  metrics.push_back({"picture_oracle_black_channels", (double)picture.OracleBlackChannels, 0.0,
                     "channels", Direction::Reported, Count::Picture});
  metrics.push_back({"picture_oracle_black_we_lit", (double)picture.OracleBlackWeLit, 0.0, "channels",
                     Direction::Reported, Count::Picture});
  metrics.push_back({"picture_oracle_black_worst_code", picture.OracleBlackWorstCode, 0.0, "codes",
                     Direction::Reported, Count::Picture});

  /* THE CHANNELS THAT DECIDED IT, in order, with where they are. The tail is a max, so on a picture
   * that is exact almost everywhere the verdict belongs to a handful -- and a handful is where a cause
   * can still be read off the coordinates. */
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

/* THE TWO COUNTS THE SUITE IS QUOTED BY, PRINTED ONCE PER CASE SO THAT NEITHER CAN BE QUOTED FOR THE
 * OTHER (board:0089). Khronos's criterion is a statement about a FEATURE and it
 * does not stop being met because our picture is not the reference's picture; the picture bound is
 * about the PICTURE and it is the owner's standard. THE CASE IS RED IF EITHER IS RED, and the two
 * lines are what make "the suite is green" unsayable without saying which of them it is about. */
void SayBothVerdicts(const std::vector<Metric> &metrics, const Tail &bound) {
  bool criterionMet = true;
  bool withinPicture = true;
  for (const Metric &metric : metrics) {
    if (metric.Against == Direction::Reported || metric.Held()) { continue; }
    (metric.Counts == Count::Picture ? withinPicture : criterionMet) = false;
  }
  std::printf("KHRONOS-CRITERION %s\n", criterionMet ? "met" : "red");
  /* A CASE WHOSE ORACLE STILL ESTIMATES IS NOT WITHIN THE BOUND, IT IS UNBOUNDED. Printing `within`
   * there would be the escape hatch I.26.15 refuses, one word further along: a case nobody can count
   * either way would be counted as a pass. */
  std::printf("PICTURE-BOUND %s\n",
              !bound.Enforced ? "not-enforced" : (withinPicture ? "within" : "outside"));
}

std::string Argument(int argc, char **argv) {
  if (argc < 2 || argv[1][0] == '\0') { return std::string(); }
  std::string directory = argv[1];
  if (directory.back() != '/') { directory += '/'; }
  return directory;
}

} // namespace

int main(int argc, char **argv) {
  using namespace outshine::Test;

  RunnerLog logging;
  outshine::Log::SetSink(&logging);

  Case subject;
  subject.Directory = Argument(argc, argv);
  CHECK(!subject.Directory.empty(),
        "the runner was given the case directory it is to score, which is its only argument");
  if (subject.Directory.empty()) { return Report(); }
  std::printf("CASE %s\n", subject.Directory.c_str());

  RawF32 oracle;
  size_t seedApart = 0;
  outshine::Render::Renderer renderer;
  if (Prepare(subject, oracle, seedApart, renderer) == Prepared::No) {
    std::printf("VERDICT NOTHING-TO-COMPARE\n");
    return Report();
  }

  const outshine::Clients::Studio studio = MakeStudio(subject);
  NoteWhatTheStudioCarries(subject, studio);

  std::string why;
  Picture picture;
  const bool rendered = Capture(renderer, studio, picture, why);
  CHECK(rendered, "outshine rendered the subject and both readbacks landed");
  if (!rendered) {
    Refused(why);
    std::printf("VERDICT NOTHING-TO-COMPARE\n");
    return Report();
  }

  const Mask ours = FromDepth(picture.Depth, (int)subject.Frame.WidthPx, (int)subject.Frame.HeightPx);
  const Mask theirs = FromOracle(oracle);

  /* THE PICTURES GO DOWN BEFORE THE VERDICT, so a case that is about to fail still leaves the two
   * frames a person opens to see why -- especially then. */
  const std::vector<uint8_t> reference = Encoded(oracle);
  const Pictures products(subject.Directory);
  std::string unwritten;
  CHECK(products.Png("0-reference.png", reference, theirs.Width, theirs.Height, unwritten),
        "0-reference.png is written from the same floats the score is computed on");
  CHECK(products.Png("1-outshine.png", picture.Rgba, ours.Width, ours.Height, unwritten),
        "1-outshine.png is written beside the reference, pass or fail");
  if (!unwritten.empty()) { Refused(unwritten); }

  /* OUR FLOAT FRAME BESIDE THE ORACLE'S, IN THE ORACLE'S OWN LAYOUT. Until it existed the compared
   * values were openable on one side and not on the other, so a case failing on floats -- which is
   * the whole of the picture bound -- left nothing to look at, which is the position the owner was
   * in when the mask was green and the picture was wrong. `.raw` is DATA and not a picture, so the
   * two-picture rule above is untouched: what may not be in this folder is a third IMAGE.
   *
   * IT IS THE BUFFER THE BOUND IS COMPUTED ON, and the read-back below is what makes that a checked
   * property rather than a sentence: the file is opened again through the same reader the oracle is
   * read through and held against the samples that were scored. A writer that put down anything
   * else would rebuild exactly the split -- the picture showing one thing, the score measuring
   * another -- that this whole round is about. */
  /* THE SHADING NORMAL BESIDE THE FRAME (board:1126). It is a diagnostic product like the two
   * pictures: the attachment is already read back for the three-way, and a quantity that only
   * exists inside one process cannot be taken apart by anything that did not render it. */
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

  std::vector<Metric> metrics;
  if (Reduced(subject)) {
    metrics.push_back({"oracle_samples_differing_between_seeds", (double)seedApart, 0.0, "samples",
                       Direction::AtMost});
  }
  /* THE DRAW LIST, COUNTED. A batching claim nobody counts is a claim, and the two numbers together
   * say what the key bought on this subject: how many primitives were drawn and how many
   * `DrawIndexed` calls they cost after the compiled list merged what shared a pipeline and a
   * surface slot. */
  metrics.push_back({"subject_draws", (double)renderer.SubjectDrawCount(), 0.0, "draws",
                     Direction::Reported});
  metrics.push_back({"subject_draw_calls", (double)renderer.SubjectBatchCount(), 0.0, "calls",
                     Direction::Reported});
  metrics.push_back({"subject_surfaces", (double)subject.Surfaces.Slots.size(), 0.0, "slots",
                     Direction::Reported});
  /* NEITHER SIDE MAY BE DEGENERATE AND NO SCORE IS COMPUTED UNTIL BOTH ARE NOT. This is the guard,
   * and it is placed here rather than reported afterwards precisely so that the agreement numbers
   * over two empty masks are never printed at all.
   *
   * THE GUARD READS THE TWO METRICS BY NAME AND NOT BY POSITION. It used to ask `metrics[0]` and
   * `metrics[1]`, which were the two coverage fractions when it was written; four metrics have since
   * been inserted ahead of them, so it was reading the seed check and the draw count, and a case
   * whose oracle merely had an estimator left was refused as "a side of the comparison is empty"
   * with 44 950 covered pixels on one side and 44 998 on the other. Naming the two objects is what
   * makes the drift unspellable rather than caught. */
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
    std::printf("VERDICT NOTHING-TO-COMPARE\n");
    return Report();
  }

  ScoreAlternateSpellings(subject, studio, renderer, ours, metrics);

  /* WHETHER "EVERY PIXEL MUST AGREE" IS A FAIR DEMAND ON THIS SUBJECT, and it is a property of the
   * subject rather than of either renderer (Ties.h). Reported beside the disagreement so a red is
   * attributable without a second run. */
  Transform clip;
  const bool projects = subject.Eye.Clip(subject.Frame.Aspect(), clip);
  CHECK(projects, "the resolved camera yields a projection");
  if (projects) {
    const EdgeSet edges = Silhouette(subject.Geometry, clip, subject.Frame);
    const double tieMarginPx = TieMarginPx(ours, edges);
    metrics.push_back({"tie_margin_px", tieMarginPx, 0.0, "px", Direction::Reported});
    /* THE `general-position` ARM'S WHOLE ACCEPTANCE (`board/`), and it introduces
     * no constant of its own: the oracle's filter half-width is already declared in the recipe, and
     * a disagreement further from the silhouette than the oracle can resolve is not a tie however
     * few pixels of it there are. Under `exact` the pixel count is the acceptance instead and this
     * is reported beside it, because a bound that admits a disagreement is not what that arm says.
     *
     * IT COUNTS TOWARD THE PICTURE AND NOT THE FEATURE, and that follows from the routing rather
     * than from taste: the picture bound sends every pixel the two sides disagree about covering to
     * THIS metric, so a routed pixel that failed here while the picture read `within` would be a
     * picture defect reported as a feature defect -- the misquote I.26.15 exists to make
     * unspellable. */
    metrics.push_back({"worst_disagreement_px", WorstDisagreementPx(ours, theirs, edges),
                       subject.OracleFloorPx, "px",
                       subject.Placement == ExactnessClass::GeneralPosition ? Direction::AtMost
                                                                            : Direction::Reported,
                       Count::Picture});
    ScoreExactnessConstruction(subject, edges, tieMarginPx, metrics);
    ScoreVisibilityTerm(subject, clip, renderer.ShadowRayNearM(), metrics);
  }

  /* THE VERDICT IS THE RENDERED IMAGE AGAINST THE ORACLE'S IMAGE, because that is what a render case
   * is for. Everything below it -- coverage, the boundary distribution, IoU -- is printed as a
   * DIAGNOSTIC and none of it is sufficient for a pass: a case scoring the coverage mask exactly
   * while drawing no visible subject was green here for a whole round, which is the vacuous-gate
   * failure in its purest form. */
  /* WHAT THE ASSET ITSELF SAYS MUST HOLD, on the linear tap, before anything is compared with the
   * oracle. It is placed ahead of the image comparison because for a `stated-invariant` case this
   * IS the verdict and the comparison below is the diagnostic beside it -- the reverse of every
   * other kind, and the reason the two are never both enforced on one case. */
  ScoreStatedInvariants(subject, picture, oracle, metrics);

  /* THE PICTURE ITSELF, WHOLE, ON THE LINEAR TAP AND ON THE DECLARED TRANSFER (I.26.15). Our alpha
   * is `covered(sceneDepth)`, so it comes from the depth mask and not from the colour attachment --
   * the same expression the display shader evaluates, over the same input. */
  const PictureDelta image = ComparePicture(scored, oracle, ours, theirs);
  CHECK(image.Comparable, "the linear tap and the coverage mask cover the oracle's frame, so every "
                          "pixel of the picture has something to be compared against");
  const Tail bound = BoundFor(subject.Path);
  ScorePictureBound(image, bound, metrics);

  ScoreShadingNormal(subject, picture, ours, clip, metrics);

  ScoreSurfaceIdentity(subject, picture, oracle, ours, theirs, metrics);

  ScoreDeterminism(subject, studio, renderer, picture, metrics);

  ScoreRadianceResidual(subject, picture, oracle, metrics);

  const Distribution boundary = BoundaryDisplacement(ours, theirs);
  metrics.push_back({"boundary_p95_px", boundary.P95, subject.Accepted.BoundaryP95MaxPx, "px",
                     Direction::Reported});
  metrics.push_back({"boundary_p50_px", boundary.P50, 0.0, "px", Direction::Reported});
  metrics.push_back({"boundary_p99_px", boundary.P99, 0.0, "px", Direction::Reported});
  metrics.push_back({"boundary_max_px", boundary.Max, 0.0, "px", Direction::Reported});
  /* How many distances the percentiles above were taken over, which is both the instrument's sample
   * size and the whole of its cost: the nearest-neighbour search is exhaustive, so a case pays the
   * square of this number and nothing else about the subject. */
  metrics.push_back({"boundary_samples", (double)boundary.Samples, 0.0, "px", Direction::Reported});
  metrics.push_back({"iou", Iou(ours, theirs), 0.0, "dimensionless", Direction::Reported});
  /* THE PLACEMENT ACCEPTANCE, AND WHICH ARM IT IS COMES FROM THE CASE'S OWN DECLARED CLASS AND FROM
   * NOTHING INFERRED (`board/`). It used to be `self-describing AND opaque`, and
   * that pair is not the question: it made an exactness demand of every self-describing case whose
   * subject happened to be opaque, whether or not the subject could meet it, and made none of a
   * `numeric` case that could. `exact` demands every pixel and declares no tolerance; the other arm
   * is bounded above by `worst_disagreement_px` instead, so a placement is never unbounded. */
  metrics.push_back({"pixels_disagreeing", (double)Disagreeing(ours, theirs), 0.0, "px",
                     subject.Placement == ExactnessClass::Exact ? Direction::AtMost
                                                                : Direction::Reported,
                     Count::Picture});
  /* AND WHERE THEY ARE, BY THE FILE'S OWN NODE NAMES. A count says the two renderers differ; the
   * table says whether the difference belongs to one class of node or straddles every one of them,
   * which is the discriminator between a transform question and a raster question. */
  if (projects && Disagreeing(ours, theirs) > 0) {
    Note("disagreement attributed by node, both faces, overlap counted twice");
    const Attribution table =
        AttributeDisagreement(subject.Geometry, clip, subject.Frame, ours, theirs);
    for (const NodeDisagreement &node : table.Nodes) {
      std::printf("NOTE   node '%s' over %zu triangles: %zu px ours only, %zu px oracle only\n",
                  node.Node.c_str(), node.Triangles, node.OursOnly, node.TheirsOnly);
    }
    Note("disagreeing pixels no node's geometry projects onto", (double)table.Unattributed, "px");
    Note("triangles outside the depth range, unattributed", (double)table.Unprojectable,
         "triangles");
  }
  ScoreDepthProbes(subject, studio, picture.Depth, metrics);

  /* Published beside the result and never subtracted from it. */
  Note("oracle instrument floor", subject.OracleFloorPx, "px");
  metrics.push_back(
      {"plan_passes", (double)renderer.Plan().PassCount(), 2.0, "passes", Direction::AtMost});

  /* THE FRAME FRACTION IS A DECLARED, RECOMPUTED, REFUSED-ON-MISMATCH PROPERTY of the case: it is
   * what the boundary bound is being applied under, so a camera that quietly frames the subject
   * smaller tightens the bound without saying so (I.26). */
  const Json::Ref expected = subject.Manifest.Root()["expected"]["subjectFrameFraction"];
  double declaredFraction = 0;
  const bool statesFraction = ReadDeclaredNumber(expected, "expected.subjectFrameFraction",
                                                 declaredFraction, why);
  CHECK(statesFraction, "the manifest declares the frame fraction its camera was derived for");
  if (statesFraction && projects) {
    const double fraction = subject.Geometry.ProjectedAreaPx(clip, subject.Frame) /
                            (subject.Frame.WidthPx * subject.Frame.HeightPx);
    metrics.push_back({"frame_fraction_error", std::fabs(fraction - declaredFraction),
                       subject.Accepted.FrameFractionTolerance, "dimensionless",
                       Direction::AtMost});
    Note("projected frame fraction", fraction, "dimensionless");
    Note("declared frame fraction", declaredFraction, "dimensionless");
  } else if (statesFraction) {
    Refused("the resolved camera yields no projection, so no frame fraction was recomputed");
  } else {
    Refused(why);
  }

  /* TEN REDS MUST BE TEN PIECES OF INFORMATION, NOT ONE. The coverage mask separates the two
   * failures a render case can have, and they lead to different work: geometry in the wrong pixels
   * is the reader, the camera or the raster convention, and it stops the shading question being
   * asked at all; geometry in the right pixels with a different image is the shading. */
  if (image.PixelsDiffering > 0) {
    const bool placed = boundary.P95 <= subject.Accepted.BoundaryP95MaxPx;
    Note(placed ? "attribution: the geometry is in the right pixels and the shading is wrong"
               : "attribution: the geometry is in the wrong pixels, so the shading is not reached");
  }

  Print(metrics);
  SayBothVerdicts(metrics, bound);
  for (const Metric &metric : metrics) {
    if (metric.Against == Direction::Reported) { continue; }
    CHECK(metric.Held(), metric.Name.c_str());
  }
  std::printf("VERDICT COMPARED\n");
  Covers("I.26.10 a render test is a directory: one runner reads the declaration, renders the "
         "subject with no world, scores it against the cached oracle by named metrics with their "
         "own thresholds and directions, and always writes the three pictures");
  return Report();
}
