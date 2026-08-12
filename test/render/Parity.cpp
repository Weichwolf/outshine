/* THE RENDER RUNNER. One program over one case directory: read the glTF, resolve the camera and the
 * recipe, render, compare against the cached oracle, score. It contains no scene-specific branch at
 * all -- a case that would need one is not a render case (doc/requirements.md I.26.11).
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
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "Check.h"

#include "Acceptance.h"
#include "Mask.h"
#include "Metric.h"
#include "OracleRaw.h"
#include "Ties.h"

#include "Document.h"
#include "GltfStudio.h"
#include "Json.h"
#include "Log.h"
#include "Png.h"
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
  std::string CameraSource;
  outshine::Render::SubjectSurface Surface;
};

/* BLENDER'S FACTORY WORLD, and it is a property of the ORACLE rather than of the engine, which is why
 * it stands in the test and not in `src/`. A `Background` node at colour 0.05087608844041824 linear
 * on all three channels and strength 1.0, sampled as a light (doc/requirements.md I.26.12): under a
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

[[nodiscard]] bool Spill(const std::string &path, const std::vector<uint8_t> &bytes) {
  std::FILE *file = std::fopen(path.c_str(), "wb");
  if (!file) { return false; }
  const bool whole = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
  std::fclose(file);
  return whole;
}

void Refused(const std::string &why) { std::printf("REFUSED %s\n", why.c_str()); }

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
    subject.Eye.YfovRad = declared["yfovRad"].Num(0.0);
    subject.Eye.ZNearM = declared["clipStartM"].Num(0.0);
    subject.Eye.ZFarM = declared["clipEndM"].Num(0.0);
    subject.CameraSource = "manifest";
    return subject.Eye.YfovRad > 0;
  }
  if (subject.Geometry.DeclaredPlacement(subject.File, subject.Eye)) {
    subject.CameraSource = "gltf";
    return true;
  }
  /* A CASE THAT NAMES THE FILE AS ITS CAMERA AND GETS THE FRAMING RULE INSTEAD would render a
   * perfectly good picture through a path it was written to exercise and never touch. */
  if (declared["source"].StrEquals("gltf")) {
    error = "the manifest names the glTF as the camera's source and no node of it references a "
            "camera the reader accepts";
    return false;
  }
  if (subject.Geometry.Frame(subject.Eye)) {
    subject.CameraSource = "framing-rule";
    return true;
  }
  error = "the subject has degenerate bounds, so the framing rule refuses -- a fallback camera here "
          "would manufacture exactly the empty picture the guard exists to catch";
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
  const Json::Ref root = subject.Manifest.Root();
  if (!ReadSubjectClass(root["subjectClass"].Str(""), subject.Accepted.Subject, error)) {
    return false;
  }
  subject.Accepted.BoundaryP95MaxPx = DefaultBoundaryP95Px(subject.Accepted.Subject);
  subject.Accepted.EnforceBoundary = subject.Accepted.Subject == SubjectClass::OpaqueAtLeastOnePixel;
  if (!ReadAcceptance(root["acceptance"], subject.Accepted, error)) { return false; }

  /* WHAT THE SUBJECT EMITS, taken from the case's own declaration and never from a constant here: a
   * material the manifest does not describe leaves the subject black, which is visible. */
  const Json::Ref material = root["scene"]["material"];
  for (size_t channel = 0; channel < 3; ++channel) {
    subject.Surface.AlbedoLinear[channel] = (float)material["colourLinear"][channel].Num(0.0);
  }
  const Json::Ref world = root["scene"]["world"];
  if (world["kind"].StrEquals("factory")) {
    subject.Surface.EnvironmentRadiance = (float)kFactoryWorldRadiance;
  } else if (!world["kind"].Str("").empty()) {
    error = "scene.world.kind is '" + world["kind"].Str("") +
            "', and this runner knows the radiance of Blender's factory world and of no other";
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
 * directory's ONLY tracked file is its manifest (doc/requirements.md I.26.10), so on a fresh clone
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
  if (!Present(subject.Directory + "oracle.raw")) { owed.push_back("oracle.raw"); }
  std::string missing;
  for (const std::string &name : owed) {
    if (missing.find(name) != std::string::npos) { continue; }
    if (!missing.empty()) { missing += ", "; }
    missing += name;
  }
  return missing;
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
  return ResolveCamera(subject, error);
}

/* A POLL, TURNED INTO AN ANSWER, AND IT RENDERS NOTHING. The renderer's readers are polls because a
 * browser frame thread has no legal way to stand still; a runner does, so it spends turns until the
 * transfer lands and refuses rather than looping forever.
 *
 * IT USED TO RENDER A FRAME PER TURN, and that is how pace got into the picture: the number of turns
 * a transfer takes is the host's business, so the number of frames the accumulator had seen when the
 * colour was copied was too. The transfer is submitted by the first poll and needs no further frame.
 * How many frames a picture needs before it is the picture is the PLAN's statement, below. */
constexpr int kPollTurns = 4000;      /* [SET] a bound, so a failed transfer ends the run instead of it */
constexpr int kPollWaitMs = 1;        /* [SET] the bound is therefore four seconds of wall clock */

template <typename Read>
[[nodiscard]] bool Drain(Read read) {
  for (int turn = 0; turn < kPollTurns; ++turn) {
    const outshine::Render::ReadState state = read();
    if (state == outshine::Render::ReadState::Ready) { return true; }
    if (state == outshine::Render::ReadState::Failed) { return false; }
    /* A RUNNER MAY STAND STILL, which is the whole difference from the frame thread the poll shape
     * exists for. The wait is what makes the bound a wall clock instead of a spin that gives up
     * before the copy has retired -- and no frame is drawn while it waits, so no pace reaches the
     * picture. */
    std::this_thread::sleep_for(std::chrono::milliseconds(kPollWaitMs));
  }
  return false;
}

struct Picture {
  std::vector<float> Depth;
  std::vector<uint8_t> Rgba;
  /* THE SCENE-REFERRED LINEAR TAP, RGBA binary16: what the plan's `sceneLinear` holds, before any
   * display transfer. Determinism is judged on this and never on the PNG, whose 8-bit quantisation
   * would hide a difference the float buffer carries. */
  std::vector<uint16_t> Linear;
};

/* THE RUNNER IS A CODE CONSUMER OF THE SETUP API and not a second engine: everything it asks for is
 * `Clients::Show`, which is the same call a scenario loader that declared a glTF subject would make.
 * Nothing about the placement or the frame mapping is decided here. */
[[nodiscard]] bool Capture(outshine::Render::Renderer &renderer,
                           const outshine::Clients::Studio &studio, Picture &out,
                           std::string &error) {
  std::vector<float> scratch;
  if (!outshine::Clients::Show(renderer, studio, scratch, error)) { return false; }

  for (int frame = 0; frame < renderer.SettleFrames(); ++frame) { renderer.RenderFrame(); }
  if (!Drain([&] { return renderer.ReadDepth(out.Depth); })) {
    error = "the depth readback never completed";
    return false;
  }
  if (!Drain([&] { return renderer.ReadSceneLinear(out.Linear); })) {
    error = "the scene-referred linear readback never completed";
    return false;
  }
  if (!Drain([&] { return renderer.ReadPixels(out.Rgba); })) {
    error = "the colour readback never completed";
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
Mask FromOracle(const OracleRaw &oracle) {
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
std::vector<uint8_t> Encoded(const OracleRaw &oracle) {
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

/* WHAT A RENDER CASE IS FOR: the rendered image against the oracle's, channel by channel, ALPHA
 * INCLUDED. Both sides are RGBA with straight alpha and alpha is coverage, so a pixel where one side
 * drew a black subject and the other drew nothing differs here -- which under a three-channel
 * comparison it did not. */
struct ImageDelta {
  size_t Differing = 0;
  int MaxChannel = 0;
  double MeanAbs = 0;
};

ImageDelta CompareImages(const std::vector<uint8_t> &ours, const std::vector<uint8_t> &theirs) {
  ImageDelta delta;
  double total = 0;
  const size_t pixels = theirs.size() / 4u;
  for (size_t pixel = 0; pixel < pixels; ++pixel) {
    bool differs = false;
    for (size_t channel = 0; channel < 4; ++channel) {
      const int want = theirs[pixel * 4u + channel];
      const int got = ours[pixel * 4u + channel];
      const int apart = got > want ? got - want : want - got;
      if (apart > delta.MaxChannel) { delta.MaxChannel = apart; }
      total += apart;
      differs = differs || apart != 0;
    }
    delta.Differing += differs ? 1u : 0u;
  }
  delta.MeanAbs = pixels > 0 ? total / (4.0 * (double)pixels) : 0.0;
  return delta;
}

[[nodiscard]] bool WritePng(const std::string &path, const std::vector<uint8_t> &rgba, int width,
                            int height) {
  std::vector<uint8_t> encoded;
  if (!outshine::Clients::EncodePng(rgba.data(), width, height, encoded)) { return false; }
  return Spill(path, encoded);
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

  std::string why;
  const bool declared = ReadManifest(subject, why);
  CHECK(declared, "the case's manifest parses and its acceptance block resolves");
  if (!declared) {
    Refused(why);
    std::printf("VERDICT NOTHING-TO-COMPARE\n");
    return Report();
  }

  const std::string owed = MissingInputs(subject);
  if (!owed.empty()) {
    outshine::Test::Unprepared((subject.Directory + " is missing " + owed).c_str());
    std::printf("VERDICT NOTHING-TO-COMPARE\n");
    return Report();
  }

  const bool loaded = BuildSubject(subject, why);
  CHECK(loaded, "the case's subject and its camera both resolve");
  if (!loaded) {
    Refused(why);
    std::printf("VERDICT NOTHING-TO-COMPARE\n");
    return Report();
  }
  std::printf("CAMERA %s\n", subject.CameraSource.c_str());

  /* THE ORACLE IS READ BEFORE ANYTHING IS RENDERED. An absent reference is a property of the case,
   * and finding it out after a device bring-up would report a rendering failure for a missing file. */
  OracleRaw oracle;
  const bool haveOracle = oracle.ReadFile(subject.Directory + "oracle.raw");
  CHECK(haveOracle, "the cached oracle is present and reads as a float32 dump of this frame");
  if (!haveOracle) {
    Refused(oracle.Error());
    std::printf("VERDICT NOTHING-TO-COMPARE\n");
    return Report();
  }
  const bool sameFrame = oracle.Width() == (int)subject.Frame.WidthPx &&
                         oracle.Height() == (int)subject.Frame.HeightPx;
  CHECK(sameFrame, "the oracle was rendered at the resolution the manifest's recipe declares");
  if (!sameFrame) {
    Refused("oracle.raw is " + std::to_string(oracle.Width()) + "x" +
            std::to_string(oracle.Height()) + " and the recipe declares " +
            std::to_string((int)subject.Frame.WidthPx) + "x" +
            std::to_string((int)subject.Frame.HeightPx));
    std::printf("VERDICT NOTHING-TO-COMPARE\n");
    return Report();
  }

  /* THE CASE'S OWN DECLARATION, and it is the whole of what will be created and encoded. One content
   * stage and two requested outputs: the depth the coverage predicate reads, and the picture a person
   * opens. No light model, no atmosphere chain, no shadow, no occlusion, no temporal resolve, no
   * present -- none of them is switched off here, none of them is in the plan at all.
   *
   * `Transfer::Linear` because the oracle's own view transform is `Standard`, which is the sRGB
   * transfer function over scene-referred linear values and nothing else, and the frame target is
   * sRGB-encoding: a curve here would be measuring the curve (doc/requirements.md I.26.13). */
  outshine::Render::PlanSpec declaration;
  declaration.Outputs = {outshine::Render::Resource::SceneDepth,
                         outshine::Render::Resource::FrameTex};
  declaration.Content = {outshine::Render::Stage::Subjects};
  declaration.Display =
      outshine::Render::Declared<outshine::Render::Transfer>(outshine::Render::Transfer::Linear);
  declaration.Exposure = outshine::Render::Declared<float>(1.0f);
  std::shared_ptr<const outshine::Render::RenderPlan> plan;
  const bool compiled = outshine::Render::RenderPlan::Compile(declaration, &plan, why);
  CHECK(compiled, "the case's render declaration compiles");
  if (!compiled) {
    Refused(why);
    std::printf("VERDICT NOTHING-TO-COMPARE\n");
    return Report();
  }
  std::printf("PLAN %s %d passes, %d stages\n", plan->Digest().c_str(), plan->PassCount(),
              (int)plan->Order().size());

  outshine::Render::Renderer renderer;
  renderer.Init((int)subject.Frame.WidthPx, (int)subject.Frame.HeightPx, plan);
  const bool usable = renderer.DeviceUsable();
  CHECK(usable, "the device came up, so the case can be rendered at all");
  if (!usable) {
    Refused("no usable device");
    std::printf("VERDICT NOTHING-TO-COMPARE\n");
    return Report();
  }

  outshine::Clients::Studio studio;
  studio.Geometry = &subject.Geometry;
  studio.Eye = subject.Eye;
  studio.Surface = subject.Surface;

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
  CHECK(WritePng(subject.Directory + "0-reference.png", reference, theirs.Width, theirs.Height),
        "0-reference.png is written from the same floats the score is computed on");
  CHECK(WritePng(subject.Directory + "1-outshine.png", picture.Rgba, ours.Width, ours.Height),
        "1-outshine.png is written beside the reference, pass or fail");

  std::vector<Metric> metrics;
  metrics.push_back({"coverage_fraction_outshine", ours.Fraction(),
                     subject.Accepted.CoverageFractionMin, "dimensionless", Direction::AtLeast});
  metrics.push_back({"coverage_fraction_oracle", theirs.Fraction(),
                     subject.Accepted.CoverageFractionMin, "dimensionless", Direction::AtLeast});

  /* NEITHER SIDE MAY BE DEGENERATE AND NO SCORE IS COMPUTED UNTIL BOTH ARE NOT. This is the guard,
   * and it is placed here rather than reported afterwards precisely so that the agreement numbers
   * over two empty masks are never printed at all. */
  const bool bothPresent = metrics[0].Held() && metrics[1].Held();
  if (!bothPresent) {
    Print(metrics);
    CHECK(bothPresent,
          "both renders carry a subject, so there is something to compare -- two empty masks agree "
          "perfectly and would have tested nothing");
    Refused("a side of the comparison is empty, so no agreement number is computed over it");
    std::printf("VERDICT NOTHING-TO-COMPARE\n");
    return Report();
  }

  /* TWO OF OUR OWN RENDERS, AND NO ORACLE IN IT AT ALL. A second spelling of the same surface -- the
   * same placement through a `matrix`, the same triangles under another index width, the same quad
   * as a strip or a fan -- must land in the same pixels, and that claim is DECIDABLE: it is exact,
   * not within a tolerance, so its threshold is zero disagreeing pixels and it needs no reference. */
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
      built = Capture(renderer, other, again, trouble);
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

  /* WHETHER "EVERY PIXEL MUST AGREE" IS A FAIR DEMAND ON THIS SUBJECT, and it is a property of the
   * subject rather than of either renderer (Ties.h). Reported beside the disagreement so a red is
   * attributable without a second run. */
  Transform clip;
  const bool projects = subject.Eye.Clip(subject.Frame.Aspect(), clip);
  CHECK(projects, "the resolved camera yields a projection");
  if (projects) {
    const EdgeSet edges = ProjectEdges(subject.Geometry, clip, subject.Frame);
    metrics.push_back({"tie_margin_px", TieMarginPx(ours, edges), 0.0, "px", Direction::Reported});
    metrics.push_back({"worst_disagreement_px", WorstDisagreementPx(ours, theirs, edges), 0.0, "px",
                       Direction::Reported});
  }

  /* THE VERDICT IS THE RENDERED IMAGE AGAINST THE ORACLE'S IMAGE, because that is what a render case
   * is for. Everything below it -- coverage, the boundary distribution, IoU -- is printed as a
   * DIAGNOSTIC and none of it is sufficient for a pass: a case scoring the coverage mask exactly
   * while drawing no visible subject was green here for a whole round, which is the vacuous-gate
   * failure in its purest form. */
  const ImageDelta image = CompareImages(picture.Rgba, reference);
  metrics.push_back({"image_pixels_differing", (double)image.Differing, 0.0, "px",
                     Direction::AtMost});
  metrics.push_back({"image_max_channel_delta", (double)image.MaxChannel, 0.0, "sRGB8",
                     Direction::Reported});
  metrics.push_back({"image_mean_abs_delta", image.MeanAbs, 0.0, "sRGB8", Direction::Reported});

  /* THE SAME DECLARATION TWICE IN ONE PROCESS. What this isolates is frame-to-frame state: a second
   * process would also change the allocator, the device and the shader cache, and a difference there
   * would not say which. It is judged on the linear tap and never on the PNG, because an 8-bit
   * quantisation hides a difference the float buffer carries. `CLAUDE.md`: the mathematics is
   * deterministic, and if pace decides the result the coupling is a bug. */
  {
    Picture again;
    std::string trouble;
    const bool twice = Capture(renderer, studio, again, trouble);
    CHECK(twice, "the same declaration renders a second time in the same process");
    size_t apart = 0, worst = 0;
    size_t firstAt = again.Linear.size();
    if (twice && again.Linear.size() == picture.Linear.size()) {
      for (size_t at = 0; at < again.Linear.size(); ++at) {
        if (again.Linear[at] == picture.Linear[at]) { continue; }
        const size_t off = again.Linear[at] > picture.Linear[at]
                               ? (size_t)(again.Linear[at] - picture.Linear[at])
                               : (size_t)(picture.Linear[at] - again.Linear[at]);
        if (off > worst) { worst = off; }
        if (apart == 0) { firstAt = at; }
        ++apart;
      }
    }
    metrics.push_back({"linear_halves_differing_between_renders", (double)apart, 0.0, "halves",
                       Direction::AtMost});
    if (apart > 0) {
      Note("first differing half-float, at channel index", (double)firstAt, "index");
      Note("widest disagreement between two renders", (double)worst, "half-float codes");
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
  metrics.push_back({"pixels_disagreeing", (double)Disagreeing(ours, theirs), 0.0, "px",
                     Direction::Reported});
  /* The oracle's own sub-pixel resolution, from the recipe that produced it: half the box filter's
   * width bounds how far a Cycles sample can sit from the pixel centre. Published beside the result
   * and never subtracted from it. */
  Note("oracle instrument floor",
       0.5 * subject.Manifest.Root()["renders"]["default"]["pixelFilter"]["widthPx"].Num(0.0), "px");
  metrics.push_back({"plan_passes", (double)plan->PassCount(), 2.0, "passes", Direction::AtMost});
  Note("declared subject radiance",
       (double)subject.Surface.AlbedoLinear[0] * (double)subject.Surface.EnvironmentRadiance,
       "linear, scene-referred");

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
  if (image.Differing > 0) {
    const bool placed = boundary.P95 <= subject.Accepted.BoundaryP95MaxPx;
    Note(placed ? "attribution: the geometry is in the right pixels and the shading is wrong"
               : "attribution: the geometry is in the wrong pixels, so the shading is not reached");
  }

  Print(metrics);
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
