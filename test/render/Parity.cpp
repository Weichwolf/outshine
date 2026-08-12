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
 * to see progress and a picture that only appears on a failure cannot show any. */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "Check.h"

#include "Acceptance.h"
#include "Mask.h"
#include "Metric.h"
#include "OracleRaw.h"

#include "Document.h"
#include "GltfStudio.h"
#include "Json.h"
#include "Png.h"
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

[[nodiscard]] bool Spill(const std::string &path, const std::vector<uint8_t> &bytes) {
  std::FILE *file = std::fopen(path.c_str(), "wb");
  if (!file) { return false; }
  const bool whole = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
  std::fclose(file);
  return whole;
}

void Refused(const std::string &why) { std::printf("REFUSED %s\n", why.c_str()); }

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
  for (const char *product : {"oracle.raw", "0-oracle.png"}) {
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

/* A POLL, TURNED INTO AN ANSWER. The renderer's readers are polls because a browser frame thread has
 * no legal way to stand still; a runner does, so it spends frames until the transfer lands and
 * refuses rather than looping forever. */
constexpr int kSettleFrames = 4;   /* [SET] enough for the device to have a submitted frame to copy */
constexpr int kPollFrames = 240;   /* [SET] a bound, so a failed transfer ends the run instead of it */

template <typename Read>
[[nodiscard]] bool Drain(outshine::Render::Renderer &renderer, Read read) {
  for (int frame = 0; frame < kPollFrames; ++frame) {
    const outshine::Render::ReadState state = read();
    if (state == outshine::Render::ReadState::Ready) { return true; }
    if (state == outshine::Render::ReadState::Failed) { return false; }
    renderer.RenderFrame();
  }
  return false;
}

struct Picture {
  std::vector<float> Depth;
  std::vector<uint8_t> Rgba;
};

/* THE RUNNER IS A CODE CONSUMER OF THE SETUP API and not a second engine: everything it asks for is
 * `Clients::Show`, which is the same call a scenario loader that declared a glTF subject would make.
 * Nothing about the placement or the frame mapping is decided here. */
[[nodiscard]] bool Render(const Case &subject, Picture &out, std::string &error) {
  outshine::Render::Renderer renderer;
  renderer.Init((int)subject.Frame.WidthPx, (int)subject.Frame.HeightPx);
  if (!renderer.DeviceUsable()) {
    error = "no usable device: the case cannot be rendered at all";
    return false;
  }
  std::vector<float> scratch;
  if (!outshine::Clients::Show(renderer, subject.Geometry, subject.Eye, scratch, error)) {
    return false;
  }

  for (int frame = 0; frame < kSettleFrames; ++frame) { renderer.RenderFrame(); }
  if (!Drain(renderer, [&] { return renderer.ReadDepth(out.Depth); })) {
    error = "the depth readback never completed";
    return false;
  }
  if (!Drain(renderer, [&] { return renderer.ReadPixels(out.Rgba); })) {
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

/* GREEN WHERE BOTH AGREE, RED WHERE ONLY THE ORACLE HAS THE SUBJECT, BLUE WHERE ONLY WE DO. No
 * acceptance reads this file; it is a viewing aid and the numbers are decided on the floats. */
void WriteDiff(const Mask &ours, const Mask &oracle, std::vector<uint8_t> &rgba) {
  rgba.assign((size_t)ours.Width * (size_t)ours.Height * 4u, 0u);
  for (size_t pixel = 0; pixel < ours.In.size() && pixel < oracle.In.size(); ++pixel) {
    const bool inOurs = ours.In[pixel] != 0, inOracle = oracle.In[pixel] != 0;
    uint8_t *texel = &rgba[pixel * 4u];
    texel[0] = (inOracle && !inOurs) ? 255u : 0u;
    texel[1] = (inOracle && inOurs) ? 96u : 0u;
    texel[2] = (inOurs && !inOracle) ? 255u : 0u;
    texel[3] = 255u;
  }
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

  Picture picture;
  const bool rendered = Render(subject, picture, why);
  CHECK(rendered, "outshine rendered the subject and both readbacks landed");
  if (!rendered) {
    Refused(why);
    std::printf("VERDICT NOTHING-TO-COMPARE\n");
    return Report();
  }

  const Mask ours = FromDepth(picture.Depth, (int)subject.Frame.WidthPx, (int)subject.Frame.HeightPx);
  const Mask theirs = FromOracle(oracle);

  /* THE PICTURES GO DOWN BEFORE THE VERDICT, so a case that is about to fail still leaves the three
   * files a person opens to see why. */
  std::vector<uint8_t> diff;
  WriteDiff(ours, theirs, diff);
  CHECK(WritePng(subject.Directory + "1-outshine.png", picture.Rgba, ours.Width, ours.Height),
        "1-outshine.png is written into the case directory, pass or fail");
  CHECK(WritePng(subject.Directory + "2-diff.png", diff, ours.Width, ours.Height),
        "2-diff.png is written into the case directory, pass or fail");

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

  const Distribution boundary = BoundaryDisplacement(ours, theirs);
  metrics.push_back({"boundary_p95_px", boundary.P95, subject.Accepted.BoundaryP95MaxPx, "px",
                     subject.Accepted.EnforceBoundary ? Direction::AtMost : Direction::Reported});
  metrics.push_back({"boundary_p50_px", boundary.P50, 0.0, "px", Direction::Reported});
  metrics.push_back({"boundary_p99_px", boundary.P99, 0.0, "px", Direction::Reported});
  metrics.push_back({"boundary_max_px", boundary.Max, 0.0, "px", Direction::Reported});
  metrics.push_back({"iou", Iou(ours, theirs), 0.0, "dimensionless", Direction::Reported});
  metrics.push_back({"pixels_disagreeing", (double)Disagreeing(ours, theirs), 0.0, "px",
                     Direction::Reported});
  /* The oracle's own sub-pixel resolution, from the recipe that produced it: half the box filter's
   * width bounds how far a Cycles sample can sit from the pixel centre. Published beside the result
   * and never subtracted from it. */
  Note("oracle instrument floor",
       0.5 * subject.Manifest.Root()["renders"]["default"]["pixelFilter"]["widthPx"].Num(0.0), "px");

  /* THE FRAME FRACTION IS A DECLARED, RECOMPUTED, REFUSED-ON-MISMATCH PROPERTY of the case: it is
   * what the boundary bound is being applied under, so a camera that quietly frames the subject
   * smaller tightens the bound without saying so (I.26). */
  const Json::Ref expected = subject.Manifest.Root()["expected"]["subjectFrameFraction"];
  double declaredFraction = 0;
  const bool statesFraction = ReadDeclaredNumber(expected, "expected.subjectFrameFraction",
                                                 declaredFraction, why);
  CHECK(statesFraction, "the manifest declares the frame fraction its camera was derived for");
  if (statesFraction) {
    Transform clip;
    const bool projects = subject.Eye.Clip(subject.Frame.Aspect(), clip);
    CHECK(projects, "the resolved camera yields a projection");
    if (projects) {
      const double fraction = subject.Geometry.ProjectedAreaPx(clip, subject.Frame) /
                              (subject.Frame.WidthPx * subject.Frame.HeightPx);
      metrics.push_back({"frame_fraction_error", std::fabs(fraction - declaredFraction),
                         subject.Accepted.FrameFractionTolerance, "dimensionless",
                         Direction::AtMost});
      Note("projected frame fraction", fraction, "dimensionless");
      Note("declared frame fraction", declaredFraction, "dimensionless");
    }
  } else {
    Refused(why);
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
