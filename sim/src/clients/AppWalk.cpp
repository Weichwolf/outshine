/* THE PEDESTRIAN'S FRAME, native. One declared scene (mods/demo/scene.json), one PNG — no mission, no
 * units, no sensors, no HUD. It links render/ + world/ + the handful of core/ value types those two
 * need, which is precisely the part of the tree that survives the airframe cut. */
#include "Renderer.h"
#include "WindField.h"
#include "World.h"
#include "Camera.h"
#include "CivilTime.h"
#include "CloudDensity.h"
#include "ConstantWindWeather.h"
#include "ElevationProvider.h"
#include "Ephemeris.h"
#include "Geodesy.h"
#include "Scene.h"
#include "Snapshot.h"
#include "SceneWeather.h"
#include "SubjectBench.h"
#include "TerrainLoader.h"
#include "TreeFoliage.h"
#include "TreeGrower.h"
#include "TreeField.h"
#include "stages/TreeStage.h"
#include "TreeLeaf.h"
#include "TreeMesh.h"
#include "TreeSpecies.h"
#include "Units.h"
#include "GroundMaterials.h"
#include "Json.h"
#include "VegetationTemplates.h"
#include "Log.h"
#include "LogSinks.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <sys/stat.h>
#include <vector>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace outshine;

namespace {

/* HARD-WIRED, because this is a bench and not a game: there is exactly one scene and choosing it is
 * not a feature. Relative to sim/, where the make targets run. */
const char *kScenePath = "../mods/demo/scene.json";   /* --scene overrides it, bench only */
const char *kVegetationPath = "assets/world/vegetation.json";
const char *kGroundMaterialPath = "assets/world/ground-materials.json";

/* THE BENCH'S CARD IS A DATUM, not a DEM sample: the subject bench has to run with no network at all,
 * and a plant's verdict may not depend on a tile server answering. 100.6 m is the reference scene's
 * own measured ground (`/elev?lat=52.10602&lon=9.43453&block=1`), so the air column over the bench is
 * the air column over the scene. */
const double kRigGroundAslM = 100.6;
/* [SET] the least clearance at which an eye is outside the ground. Two metres is a standing person's
 * eye and it is under the DEM's own sample error, so it corrects the lens as little as it can. */
const double kMinStandClearM = 2.0;

/* ONE SPECIES = ONE FILE, and the bench names it. */
const char *kSpeciesDir = "assets/world/species";

/* THE LAMINA'S BASE REFLECTANCE, and it is not a new number: it is the vegetation table's own
 * `laubmischwald` fresh blade, sRGB (74, 92, 46) linearised — the green the ground shader already
 * draws a broadleaf stand in. A species' declared `leaf_r/g/b` multiplies it, which is what makes
 * that triple a TINT (spruce 0.40/0.74/0.55 is darker and bluer than beech, not a colour of its own). */
const float kLeafBaseLinear[3] = {0.0684f, 0.1072f, 0.0273f};

/* [SET] The fan a shoot's leaves stand in about their stalk, full angle. A card is one shoot tip, and
 * a rosette of four laminae over less than a right angle stacks into a line from every direction but
 * one. */
constexpr float kCardFanDeg = 110.0f;

/* A DECLARED COLOUR IS sRGB; a reflectance is linear. `kLeafBaseLinear` above is the same conversion
 * spelled out by hand, and `bark_r/g/b` never got it: 0.53 read as a reflectance is a 50 % grey, and
 * a beech drawn in it is a birch. Linearised, the sixteen declarations land where bark is measured —
 * beech 0.246, oak 0.076, spruce 0.060, birch 0.677 against published visible-band reflectances of
 * roughly 0.05...0.25 for temperate bark and 0.45...0.60 for birch. */
float SrgbToLinear(float v) {
  return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

Render::TreeLook TreeLookOf(const World::TreeSpecies &sp) {
  Render::TreeLook look;
  const World::TreeSpecies::Shading &sh = sp.ShadingParams();
  const World::TreeSpecies::Leaf &lf = sp.LeafParams();
  for (int c = 0; c < 3; c++) {
    look.BarkRgb[c] = SrgbToLinear(sh.BarkColor[c]);
    look.LeafRgb[c] = kLeafBaseLinear[c] * sh.LeafTint[c];
  }
  look.BarkDark = sh.BarkDark;
  look.BarkFreq = sh.BarkFreq;
  look.BarkRidge = sh.BarkRidge;
  look.LeafWidth = lf.Width;
  look.LeafWidest = lf.Widest;
  look.LeafTip = lf.Tip;
  look.LeafBaseFill = lf.BaseFill;
  look.LeafLobes = (float)lf.Lobes;
  look.LeafLobeDepth = lf.LobeDepth;
  look.LeafSerration = lf.Serration;
  look.LeafFold = lf.Fold;
  look.NeedleWidth = lf.Kind == World::TreeSpecies::LeafKind::Needle ? lf.NeedleWidth : 0.0f;
  return look;
}

/* THE BENCH'S FLOOR IS THE TEMPLATE'S DECLARED SUBSTRATE, and it has to be read from the class table
 * rather than from the resolved vegetation row: `swardClosure` overwrites a row's ground reflectance
 * with the stand's own aggregate colour — for `wiese` completely. On the bench that would be the
 * subject's colour standing in for the ground it is judged against. */
bool RigSubstrate(const char *vegPath, const World::GroundMaterials &mats, const std::string &tpl,
                  std::string *name, float rgb[3]) {
  FILE *f = fopen(vegPath, "rb");
  if (!f) return false;
  std::string text;
  char buf[8192];
  for (size_t n; (n = fread(buf, 1, sizeof buf, f)) > 0;) text.append(buf, n);
  fclose(f);

  Render::Json doc;
  if (!doc.Parse(text.c_str(), text.size())) return false;
  const Render::Json::Ref ts = doc.Root()["templates"];
  for (size_t i = 0; i < ts.Size(); i++) {
    if (ts[i]["name"].Str() != tpl) continue;
    const std::string cls = ts[i]["ground"]["class"].Str();
    const int mi = mats.Find(cls);
    if (mi < 0) return false;
    *name = cls;
    for (int c = 0; c < 3; c++) rgb[c] = mats.At((size_t)mi).Albedo[c];
    return true;
  }
  return false;
}

bool ReadWholeFile(const char *path, std::string *out) {
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  char buf[8192];
  out->clear();
  for (size_t n; (n = fread(buf, 1, sizeof buf, f)) > 0;) out->append(buf, n);
  fclose(f);
  return !out->empty();
}

void Usage(const char *argv0) {
  fprintf(stderr,
          "usage: %s [--size WxH] [--warm N] [--view KM] [--base URL] [--out PATH]\n"
          "       [--ev STOPS] [--bench N] [--spin N] [--eye M] [--eye-asl M] [--pitch DEG]\n"
          "       [--yaw DEG]\n"
          "       [--snapshot PATH]\n"
          "       [--rig TEMPLATE] [--rig-species NAME] [--rig-height M] [--rig-out DIR]\n"
          "       [--rig-turn N] [--rig-no-leaves]\n"
          "       [--wind-t S] [--wind-deg D] [--wind-ms X] [--seq N] [--seq-dt S] [--seq-out DIR]\n"
          "  --spin N    a full 360 deg turntable in N frames, each one WAITED FOR: p50/p95/p99\n"
          "              of the frame time. A still frame prices one azimuth; a forest is not\n"
          "              isotropic, so a scattered field has no price without this\n"
          "  --snapshot  a standpoint another client wrote (clients/Snapshot.h): lat/lon/yaw/pitch\n"
          "              out of one line of fb-sim's shots.jsonl. It REPLACES --stepE/--stepN/\n"
          "              --yaw/--pitch and is refused if the scene it names is not this one\n"
          "  --rig       the SUBJECT BENCH (doc/goal.md §3): one plant alone on its own declared\n"
          "              substrate beside an 18 %% card, in declared light, no world and no network.\n"
          "              TEMPLATE names a vegetation row; 7 views x 3 lights, every cell filled\n"
          "  --rig-species  the subject is a TREE grown from assets/world/species/NAME.json; the\n"
          "              framings become the tree's (portrait/a/b/closeup_hd/oblique/crown/eye) and\n"
          "              the height is the species' own declared one\n"
          "  --rig-no-leaves  grows and draws the bark net alone: the A/B that prices the leaf net\n"
          "  --rig-leaf-mult N  brackets the declared leaf_cards, the way --ev brackets the declared\n"
          "              exposure. 1 is what the species declares and is the only number a scene sees\n"
          "  --rig-height  overrides the template's own plant height; a 25 m tree needs no other flag\n"
          "  --walkE/N   metres per streaming pass — the camera MOVES while the tiles arrive, which is\n"
          "              the only condition under which a class can be caught renaming its ground\n"
          "  --wind-t    the WIND clock in seconds. It is not the sky clock: the sun stands where the\n"
          "              scene declared it while the flow runs, which is what a wave measurement needs\n"
          "  --wind-deg/--wind-ms  override the scene's declared met wind. A BENCH parameter, like\n"
          "              --eye/--yaw: the scene keeps one declaration and the bench brackets it\n"
          "  --eye-asl   the LENS ALTITUDE above sea level, which is what a camera operator publishes.\n"
          "              The height above ground follows from this DEM, and a lens the DEM buries is\n"
          "              lifted to the minimum clearance and the lift is reported. A standpoint is\n"
          "              not a standpoint if the eye is inside the ground\n"
          "  --seq N     after the warm-up, write N frames %%04d.png into --seq-out, advancing the WIND\n"
          "              clock by --seq-dt between them and nothing else. One streaming state, one sun\n"
          "  --seq-yaw D  degrees of yaw PER SEQUENCE FRAME. A temporal filter can only be judged on a\n"
          "              moving camera: a still one hides both ghosting and a wrong motion vector\n"
          "  --seq-stepE/N M  metres per sequence frame, east/north. Same argument, translation\n"
          "  --seq-prof PATH  the MOVING MEASUREMENT: one CSV row per sequence frame with the frame\n"
          "              time and every residency counter. Writes no PNG — a stutter is the tail of a\n"
          "              series, and stbi's compression time is not part of a frame\n"
          "  --wind-probe PATH  the STATE channel: the declared WindField, sampled on a\n"
          "              world line along the wind over --seq x --seq-dt, as CSV. No picture involved\n"
          "  --depth     W*H raw f32 reversed-Z depth beside the PNG; range = 0.05/d/cos(off-axis)\n"
          "  the SCENE is %s and is not selectable; everything below is bench plumbing\n"
          "  --warm N    the CEILING on streaming passes, not a count: the run warms until the world\n"
          "              reports full residency and fails loudly if it does not reach it in N\n"
          "  --walk N    passes the --walkE/N step is applied over; warming continues standing still\n"
          "  --settle N  frames rendered after the accumulator is emptied, before the readback\n",
          argv0, kScenePath);
}

}  // namespace

int main(int argc, char **argv) {
  /* `warm` is a CEILING. Streaming is asynchronous since the tile pool, so a pass count buys no
   * residency at all; 4000 passes is ~20 s of standing still, far past the ~600 the reference scene
   * needs off a warm tile server (measured 2026-08-07). */
  int width = 1280, height = 720, warm = 4000, walkPasses = 240, settle = -1;
  double viewKm = 60.0;
  std::string base = "http://localhost:8081", outPath = "walk.png", depthPath;
  int bench = 0, spin = 0;
  bool manualEv = false;
  double evStops = 0.0;
  /* The STANDPOINT, not the scene: a LOD ladder has to be judged from more than one distance
   * (doc/goal.md §5 wants a curve, not a point), an exposure anchor from more than one pointing, and
   * world-fixed placement only from more than one position. Everything else the scene still declares. */
  double eyeOverrideM = -1.0, eyeAslM = -1.0e9, pitchOverrideDeg = 1.0e9, yawOverrideDeg = 1.0e9;
  double stepEastM = 0.0, stepNorthM = 0.0, walkEastM = 0.0, walkNorthM = 0.0;
  std::string classDump;
  double classSpan = 400.0, classStep = 0.05;
  bool classCmp = false;
  /* The wind clock and the wind itself are BENCH parameters for the same reason the standpoint is:
   * the scene declares one condition and a measurement needs a bracket around it. */
  double windT = 0.0, windDegOverride = 1.0e9, windMsOverride = -1.0;
  int seqFrames = 0;
  double seqDt = 1.0 / 30.0;
  /* THE CAMERA'S OWN MOTION over the sequence. Zero is the wind-only sequence the previous round
   * used; anything else is what makes ghosting and reprojection measurable at all. */
  double seqYawDeg = 0.0, seqStepE = 0.0, seqStepN = 0.0;
  std::string seqOut = "seq", windProbe, seqProf;
  std::string rigTemplate, rigSpecies, rigOut = "bench";
  /* A BENCH parameter like --eye: the engine still has ONE declared scene, and a comparison over
   * several places needs to point at several declarations of it. */
  std::string scenePath = kScenePath;
  double orthoM = 0.0;
  bool rigLeaves = true;
  int rigLeafMult = 1;
  std::string snapshotPath;
  double rigHeightM = 0.0;
  int rigTurn = 8;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--warm" && i + 1 < argc) warm = atoi(argv[++i]);
    else if (a == "--walk" && i + 1 < argc) walkPasses = atoi(argv[++i]);
    else if (a == "--settle" && i + 1 < argc) settle = atoi(argv[++i]);
    else if (a == "--view" && i + 1 < argc) viewKm = atof(argv[++i]);
    else if (a == "--base" && i + 1 < argc) base = argv[++i];
    else if (a == "--out" && i + 1 < argc) outPath = argv[++i];
    else if (a == "--depth" && i + 1 < argc) depthPath = argv[++i];
    else if (a == "--bench" && i + 1 < argc) bench = atoi(argv[++i]);
    else if (a == "--spin" && i + 1 < argc) spin = atoi(argv[++i]);
    else if (a == "--ev" && i + 1 < argc) { manualEv = true; evStops = atof(argv[++i]); }
    else if (a == "--scene" && i + 1 < argc) scenePath = argv[++i];
    else if (a == "--ortho" && i + 1 < argc) orthoM = atof(argv[++i]);
    else if (a == "--eye" && i + 1 < argc) eyeOverrideM = atof(argv[++i]);
    else if (a == "--eye-asl" && i + 1 < argc) eyeAslM = atof(argv[++i]);
    else if (a == "--pitch" && i + 1 < argc) pitchOverrideDeg = atof(argv[++i]);
    else if (a == "--yaw" && i + 1 < argc) yawOverrideDeg = atof(argv[++i]);
    else if (a == "--wind-t" && i + 1 < argc) windT = atof(argv[++i]);
    else if (a == "--wind-deg" && i + 1 < argc) windDegOverride = atof(argv[++i]);
    else if (a == "--wind-ms" && i + 1 < argc) windMsOverride = atof(argv[++i]);
    else if (a == "--seq" && i + 1 < argc) seqFrames = atoi(argv[++i]);
    else if (a == "--seq-dt" && i + 1 < argc) seqDt = atof(argv[++i]);
    else if (a == "--seq-out" && i + 1 < argc) seqOut = argv[++i];
    else if (a == "--seq-yaw" && i + 1 < argc) seqYawDeg = atof(argv[++i]);
    else if (a == "--seq-stepE" && i + 1 < argc) seqStepE = atof(argv[++i]);
    else if (a == "--seq-stepN" && i + 1 < argc) seqStepN = atof(argv[++i]);
    else if (a == "--seq-prof" && i + 1 < argc) seqProf = argv[++i];
    else if (a == "--wind-probe" && i + 1 < argc) windProbe = argv[++i];
    else if (a == "--rig" && i + 1 < argc) rigTemplate = argv[++i];
    else if (a == "--rig-species" && i + 1 < argc) rigSpecies = argv[++i];
    else if (a == "--rig-no-leaves") rigLeaves = false;
    else if (a == "--rig-leaf-mult" && i + 1 < argc) rigLeafMult = atoi(argv[++i]);
    else if (a == "--rig-height" && i + 1 < argc) rigHeightM = atof(argv[++i]);
    else if (a == "--rig-out" && i + 1 < argc) rigOut = argv[++i];
    else if (a == "--rig-turn" && i + 1 < argc) rigTurn = atoi(argv[++i]);
    else if (a == "--snapshot" && i + 1 < argc) snapshotPath = argv[++i];
    else if (a == "--stepE" && i + 1 < argc) stepEastM = atof(argv[++i]);
    else if (a == "--stepN" && i + 1 < argc) stepNorthM = atof(argv[++i]);
    else if (a == "--walkE" && i + 1 < argc) walkEastM = atof(argv[++i]);
    else if (a == "--walkN" && i + 1 < argc) walkNorthM = atof(argv[++i]);
    else if (a == "--class-dump" && i + 1 < argc) classDump = argv[++i];
    else if (a == "--class-span" && i + 1 < argc) classSpan = atof(argv[++i]);
    else if (a == "--class-step" && i + 1 < argc) classStep = atof(argv[++i]);
    else if (a == "--class-cmp") classCmp = true;
    else if (a == "--size" && i + 1 < argc) {
      std::string s = argv[++i];
      size_t x = s.find('x');
      if (x == std::string::npos) { Usage(argv[0]); return 1; }
      width = atoi(s.substr(0, x).c_str());
      height = atoi(s.substr(x + 1).c_str());
    } else { Usage(argv[0]); return 1; }
  }

  static Clients::StdoutLogSink gStdoutSink;
  Log::SetSink(&gStdoutSink);
  Log::SetLevel(LogLevel::Debug);

  Clients::Scene scene;
  if (!scene.Load(scenePath.c_str())) {
    Log::Error("walk", "scene_load_failed", {{"path", scenePath}, {"why", scene.Error()}});
    return 1;
  }
  /* A STANDPOINT SOMEONE ELSE STOOD AT (clients/Snapshot.h). It replaces the standpoint flags rather
   * than combining with them: two ways of saying where the eye is, applied at once, is a picture
   * neither of them describes. */
  Clients::Snapshot snap;
  if (!snapshotPath.empty()) {
    if (stepEastM != 0.0 || stepNorthM != 0.0 || yawOverrideDeg < 1.0e8 || pitchOverrideDeg < 1.0e8) {
      Log::Error("walk", "snapshot_conflicts_with_standpoint_flags", {{"path", snapshotPath}});
      return 1;
    }
    if (!snap.Load(snapshotPath.c_str())) {
      Log::Error("walk", "snapshot_load_failed", {{"path", snapshotPath}, {"why", snap.Error()}});
      return 1;
    }
    if (!snap.Matches(scene)) {
      Log::Error("walk", "snapshot_scene_mismatch", {{"path", snapshotPath}, {"why", snap.Error()}});
      return 1;
    }
  }
  const bool haveSnapshot = !snapshotPath.empty();
  const double lat = haveSnapshot ? snap.Lat() : scene.Lat() + stepNorthM / kMPerDeg;
  const double lon = haveSnapshot ? snap.Lon()
                                  : scene.Lon() + stepEastM / (kMPerDeg * std::cos(scene.Lat() * kDeg2Rad));
  const double yawDeg = haveSnapshot ? snap.YawDeg()
                                     : (yawOverrideDeg < 1.0e8 ? yawOverrideDeg : scene.YawDeg());
  const double clk = (double)scene.UtcS();
  float sunEl = 0.0f, sunAz = 0.0f;
  SunPos(lat, lon, clk, &sunEl, &sunAz);
  Log::Info("walk", "scene", {{"path", scene.Path()}, {"lat", lat}, {"lon", lon},
      {"eyeM", scene.EyeM()}, {"yawDeg", yawDeg}, {"pitchDeg", scene.PitchDeg()},
      {"fovDeg", scene.FovDeg()}, {"utc", scene.Utc()}, {"utcS", (double)scene.UtcS()},
      {"windDeg", scene.WindDeg()}, {"windMs", scene.WindMs()},
      {"cloudCover", scene.CloudCover()}, {"sunElDeg", (double)sunEl}, {"sunAzDeg", (double)sunAz}});

  World::GroundMaterials mats;
  if (!mats.Load(kGroundMaterialPath)) {
    Log::Error("walk", "ground_materials_failed", {{"path", kGroundMaterialPath}, {"why", mats.Error()}});
    return 1;
  }
  World::VegetationTemplates veg;
  if (!veg.Load(kVegetationPath, mats)) {
    Log::Error("walk", "vegetation_table_failed", {{"path", kVegetationPath}, {"why", veg.Error()}});
    return 1;
  }

  Render::Renderer R;
  R.SetVegetationTable(veg.Rows(), veg.RowBytes(), veg.BareRockTemplate(),
                        veg.Limit().SlopeBandDeg());
  R.SetSkyClock(clk);
  R.SetFovDeg(scene.FovDeg());
  R.SetOrthoM(orthoM);

  World::TreeMesh worldTree;
  World::TreeField treeField;
  World::TreeField::Crown treeCrown;
  std::vector<float> treeStands, treeDist;
  float worldTreeSigma = 0.0f;
  const double windDeg = windDegOverride < 1.0e8 ? windDegOverride : scene.WindDeg();
  const double windMs = windMsOverride >= 0.0 ? windMsOverride : scene.WindMs();
  R.SetWind(windDeg, windMs);
  R.SetWindClock(windT);
  {
    Render::ExposureParams ep = scene.Exposure();
    if (manualEv) ep.CompEv = (float)evStops;   /* the bench's own bracket, over what the scene declared */
    R.SetExposure(ep);
  }
  R.InitOffscreen(width, height);
  if (!R.Ready()) { Log::Error("walk", "device_init_failed"); return 1; }

  /* THE SUBJECT BENCH takes the binary over completely: no World, no tile stream, no scene light.
   * doc/goal.md §3 — a plant is judged alone before it is allowed into the picture. */
  if (!rigTemplate.empty() || !rigSpecies.empty()) {
    Clients::SubjectBench bench(R, veg);
    /* Held for the whole bench run: the stage borrows nothing, but the arrays are uploaded once and
     * the numbers below are logged off the same objects the picture was drawn from. */
    World::TreeMesh mesh;
    World::TreeFoliage foliage;
    if (!rigSpecies.empty()) {
      const std::string path = std::string(kSpeciesDir) + "/" + rigSpecies + ".json";
      std::string text;
      if (!ReadWholeFile(path.c_str(), &text)) {
        Log::Error("walk", "rig_species_unreadable", {{"path", path}});
        return 1;
      }
      World::TreeSpecies sp;
      if (!sp.Parse(text.c_str(), text.size())) {
        Log::Error("walk", "rig_species_parse_failed", {{"path", path}, {"why", sp.Error()}});
        return 1;
      }
      World::TreeGrower grower;
      const auto t0 = std::chrono::steady_clock::now();
      /* THE SAME MESH THE NEAREST FIELD RANK GETS. Asking for pixel 0 asked for a tube on every
       * 3 mm twig — a mesh nobody draws and, before the budget was solved rather than cut, one that
       * truncated the crown. */
      grower.Grow(sp, mesh, Render::TreeStage::RankPixel(0));
      const auto t1 = std::chrono::steady_clock::now();
      World::TreeLeaf::Build(sp.LeafParams(), mesh);
      const auto t2 = std::chrono::steady_clock::now();
      foliage.Build(mesh, sp, rigLeafMult);
      const auto t3 = std::chrono::steady_clock::now();

      const double h = rigHeightM > 0.0 ? rigHeightM : (double)sp.HeightM();
      const double crownX = (double)(mesh.BoxMax.X - mesh.BoxMin.X) * h;
      const double crownZ = (double)(mesh.BoxMax.Z - mesh.BoxMin.Z) * h;
      const double proj = 0.25 * 3.14159265358979 * crownX * crownZ;

      R.SetTreeLook(TreeLookOf(sp));
      R.SetTreeBark(0, mesh.BarkVerts.data(), (uint32_t)mesh.BarkVertexCount(), mesh.BarkIdx.data(),
                    (uint32_t)mesh.BarkIdx.size());
      R.SetTreeLeaf(mesh.LeafVerts.data(), (uint32_t)mesh.LeafVertexCount(), mesh.LeafIdx.data(),
                    (uint32_t)mesh.LeafIdx.size(), foliage.Instances().data(),
                    (uint32_t)foliage.Count(), foliage.ScaleM());
      R.SetTreeLeavesVisible(rigLeaves);
      const auto ms = [](std::chrono::steady_clock::time_point a,
                         std::chrono::steady_clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
      };
      Log::Info("walk", "rig_tree", {{"species", rigSpecies}, {"heightM", h},
          {"declaredSpreadM", (double)sp.SpreadM()}, {"grownCrownXM", crownX}, {"grownCrownZM", crownZ},
          {"spreadRatio", sp.SpreadM() > 0.0f ? 0.5 * (crownX + crownZ) / (double)sp.SpreadM() : -1.0},
          {"barkTris", (double)(mesh.BarkIdx.size() / 3)},
          {"leafTrisEach", (double)(mesh.LeafIdx.size() / 3)},
          {"leafPoints", (double)mesh.LeafPoints.size()},
          {"leafInstances", (double)foliage.Count()},
          {"leafTrisTotal", (double)(mesh.LeafIdx.size() / 3 * foliage.Count())},
          {"leafLenM", (double)foliage.ScaleM() * (double)sp.LeafParams().Length},
          {"oneLeafAreaM2", foliage.OneLeafAreaM2()},
          {"leafAreaM2", foliage.LeafAreaM2()}, {"crownProjM2", proj},
          {"lai", proj > 0.0 ? foliage.LeafAreaM2() / proj : -1.0},
          {"laiDeclared", (double)sp.Lai()}, {"laminaePerPoint", foliage.PerPoint()},
          {"meshKb", (double)mesh.Bytes() / 1024.0},
          {"instKb", (double)(foliage.Instances().size() * sizeof(float)) / 1024.0},
          {"growMs", ms(t0, t1)}, {"leafMs", ms(t1, t2)}, {"foliageMs", ms(t2, t3)},
          {"leafMult", (double)rigLeafMult}, {"leavesDrawn", rigLeaves ? 1.0 : 0.0}});
      if (!bench.SelectTree(rigSpecies.c_str(), h)) {
        Log::Error("walk", "rig_species_height_invalid", {{"name", rigSpecies}});
        return 1;
      }
    } else if (!bench.Select(rigTemplate.c_str(), rigHeightM)) {
      Log::Error("walk", "rig_unknown_subject", {{"name", rigTemplate}});
      return 1;
    }
    /* A TREE DECLARES NO SUBSTRATE. Without a template naming a ground class the floor stays at the
     * bench's 18 % neutral, which is what a subject without a declared substrate deserves. */
    if (!rigTemplate.empty()) {
      std::string subName;
      float subRgb[3] = {0, 0, 0};
      if (!RigSubstrate(kVegetationPath, mats, rigTemplate, &subName, subRgb)) {
        Log::Error("walk", "rig_substrate_unresolved", {{"template", rigTemplate}});
        return 1;
      }
      bench.SetSubstrate(subName, subRgb);
    }
    mkdir(rigOut.c_str(), 0755);
    bench.Stand(scene.Lat(), scene.Lon(), kRigGroundAslM);
    bench.SetOutDir(rigOut);
    bench.SetTurntableSteps(rigTurn);
    return bench.Run() ? 0 : 1;
  }

  World::World W;
  if (!W.Open(&R, base.c_str(), lat, lon, 32, viewKm * 1000.0, 512)) {
    Log::Error("walk", "world_open_failed", {{"base", base}});
    return 1;
  }

  /* WHERE THE FEET ARE. The scene declares a height above ground, not an altitude, so the ground is
   * the DEM's answer and nothing else — the same DEM the mesh is built from. */
  const double ground = fb_stream_ground(lat, lon);
  if (!ElevationResolved(ground)) {
    Log::Error("walk", "ground_unresolved", {{"lat", lat}, {"lon", lon}, {"base", base}});
    return 1;
  }
  /* AN EYE INSIDE THE GROUND IS NOT A STANDPOINT. The DEM samples 12.9 m at z13, so on a valley
   * floor or a sharp ridge it misses the lens by metres in either direction — measured, mayrhofen:
   * the lens stands at 633 m and this DEM answers 635.6 m. The operator's altitude is the datum and
   * the DEM is the approximation, so the eye is lifted to the minimum clearance and the LIFT is
   * reported: a camera whose lift is not zero stands on a DEM that misses its point, and the page
   * says so instead of showing a grey box. */
  double standLiftM = 0.0;
  double eyeM = eyeOverrideM >= 0.0 ? eyeOverrideM : scene.EyeM();
  if (eyeAslM > -1.0e8) {
    const double want = eyeAslM - ground;
    eyeM = want < kMinStandClearM ? kMinStandClearM : want;
    standLiftM = eyeM - want;
    Log::Info("walk", "standpoint", {{"lensAslM", eyeAslM}, {"groundM", ground},
        {"eyeM", eyeM}, {"liftM", standLiftM}, {"minClearM", kMinStandClearM}});
  }
  const double pitchDeg = haveSnapshot ? snap.PitchDeg()
                                       : (pitchOverrideDeg < 1.0e8 ? pitchOverrideDeg : scene.PitchDeg());
  double altAsl = ground + eyeM;
  /* WHAT THE OTHER CLIENT ANSWERED AT THE SAME PLACE, subtracted here and nowhere else: a DEM or an
   * ephemeris that disagreed moves the whole picture, and no pixel comparison would name the cause. */
  if (haveSnapshot)
    Log::Info("walk", "snapshot", {{"name", snap.Name()}, {"client", snap.Client()},
        {"lat", lat}, {"lon", lon}, {"yawDeg", yawDeg}, {"pitchDeg", pitchDeg},
        {"groundM", ground}, {"dGroundM", ground - snap.GroundM()},
        {"dAltAslM", altAsl - snap.AltAslM()},
        {"dSunElDeg", (double)sunEl - snap.SunElDeg()},
        {"dSunAzDeg", (double)sunAz - snap.SunAzDeg()}});

  bool standChecked = false;
  double eye[3], fwd[3], right[3], up[3];
  GeoToEcef(lat, lon, altAsl, eye);
  CameraBasisEcef(yawDeg, pitchDeg, 0.0, lat, lon, fwd, right, up);

  State hs{};
  hs.Platform.AltM = (float)altAsl;
  hs.Platform.YawDeg = (float)yawDeg;
  hs.Platform.PitchDeg = (float)pitchDeg;
  hs.Platform.Mode = Mode::Manual;
  hs.Env.SunElDeg = sunEl;
  hs.Env.SunAzDeg = sunAz;
  MoonPos(lat, lon, clk, &hs.Env.MoonElDeg, &hs.Env.MoonAzDeg, &hs.Env.MoonPhase);
  hs.Env.CloudCover = (float)scene.CloudCover();
  R.SetSceneState(hs);
  R.SetCameraBasis(eye, fwd, right, up);
  /* NACH SetCameraBasis, weil TreeStage::Upload ohne Geraet nichts zurueckgibt: der Stamm wurde
   * vorher still verworfen und die Stage zeichnete jahrelang nichts.
   * EIN Baum waechst, und er steht tausendfach. Die Art ist die haeufigste des deklarierten
   * Laubmischwaldes; Artenmischung je Stand ist die naechste Runde. */
        {
    std::string txt;
    if (FILE *f = fopen("assets/world/species/buche.json", "rb")) {
      char buf[8192]; size_t k;
      while ((k = fread(buf, 1, sizeof buf, f)) > 0) txt.append(buf, k);
      fclose(f);
    }
    World::TreeSpecies sp;
    if (!txt.empty() && sp.Parse(txt.c_str(), txt.size())) {
      const double h = (double)sp.HeightM();
      World::TreeGrower g;
      World::TreeFoliage foliage;
      std::vector<float> rankCards;
      double crownProjM2 = 0.0;
      /* ONE MESH PER RANK, and the ladder is the stage's own: rank k may be one pixel coarse at its
       * nearest stand. THE NEAREST RANK PAYS THE INDEX IN COUNT — one card per kLeavesPerCard grown
       * laminae, each drawing the species' own leaf. Only the ranks above it thin, by four per step
       * so a card keeps its size on screen, and `CardLeafM` regrows the leaf they draw so the
       * declared index survives the thinning. */
      for (int rank = 0; rank < Render::TreeStage::kRanks; ++rank) {
        g.Grow(sp, worldTree, Render::TreeStage::RankPixel(rank));
        World::TreeLeaf::Build(sp.LeafParams(), worldTree);
        foliage.Build(worldTree, sp, 1);
        const uint32_t stride =
            (uint32_t)Render::TreeStage::kLeavesPerCard << (2u * (unsigned)rank);
        rankCards.clear();
        for (size_t i = 0; i < foliage.Count(); i += stride) {
          const float *c = &foliage.Instances()[i * World::TreeFoliage::kFloats];
          rankCards.insert(rankCards.end(), c, c + World::TreeFoliage::kFloats);
        }
        const uint32_t nCards = (uint32_t)(rankCards.size() / World::TreeFoliage::kFloats);
        /* THE CROWN'S PROJECTION IS THE SPECIES', not the rank's. A coarse rank drops the thin shoots
         * that reach furthest out, so its own bark box is smaller — sizing the leaf by that box would
         * shrink the canopy exactly where it is already thinnest. */
        if (rank == 0) { crownProjM2 = foliage.CrownProjM2(); }
        const float cardLeafM = foliage.CardLeafM(Render::TreeStage::kLeavesPerCard, nCards,
                                                  (double)sp.Lai(), crownProjM2);
        R.SetTreeBark(rank, worldTree.BarkVerts.data(), (uint32_t)worldTree.BarkVertexCount(),
                      worldTree.BarkIdx.data(), (uint32_t)worldTree.BarkIdx.size());
        R.SetTreeCards(rank, rankCards.data(), nCards, cardLeafM, kCardFanDeg);
        Log::Info("walk", "trees_grown", {{"rank", (double)rank},
            {"pixelHeightFrac", (double)Render::TreeStage::RankPixel(rank)},
            {"barkVerts", (int)worldTree.BarkVertexCount()},
            {"barkTris", (double)(worldTree.BarkIdx.size() / 3)},
            {"heightM", h}, {"bhdCm", 200.0 * (double)worldTree.BhdRadius * h},
            {"cards", (double)nCards},
            {"leavesPerCard", (double)Render::TreeStage::kLeavesPerCard},
            {"cardLeafM", (double)cardLeafM}, {"declaredLeafM", (double)foliage.ScaleM()},
            {"crownProjM2", crownProjM2}, {"lai", (double)sp.Lai()},
            {"leafPoints", (double)worldTree.LeafPoints.size()},
            {"laminae", (double)foliage.Count()}, {"perPoint", foliage.PerPoint()},
            {"growHeight", (double)g.GrowHeight()}});
        if (rank == 0) {
          R.SetTreeLook(TreeLookOf(sp));
          /* bpar.z ist die Baumhoehe und kommt aus SetTreeStand; ohne sie skaliert jede Instanz auf
           * null. */
          R.SetTreeStand(0.0, 0.0, 0.0, h);
          treeCrown.HalfWidth = std::fmax(std::fmax(-worldTree.BoxMin.X, worldTree.BoxMax.X),
                                          std::fmax(-worldTree.BoxMin.Z, worldTree.BoxMax.Z));
          treeCrown.Bottom = worldTree.BoxMin.Y;
          treeCrown.Top = worldTree.BoxMax.Y;
          treeCrown.HeightM = (float)h;
          R.SetTreeCrown(treeCrown.HalfWidth, treeCrown.Top, treeCrown.Bottom);
        }
      }
      R.BakeTreeImpostor();
      worldTreeSigma = sp.HeightSigma();
    }
  }


  Clients::SceneWeather wind(scene);
  const WindNed w = wind.WindNedMs(lat, lon, altAsl);
  /* The field's horizontal origin is the SCENE's own point, never the standpoint: --stepE/--stepN
   * move the camera under one sky instead of carrying the sky with them. */
  W.SetVegetation(&veg);
  W.SetSunElevationDeg(sunEl);
  W.SetWeather(&wind);
  Log::Info("walk", "stand", {{"groundM", ground}, {"eyeM", eyeM}, {"pitchDeg", pitchDeg}, {"aslM", altAsl},
      {"sunElDeg", (double)sunEl}, {"sunAzDeg", (double)sunAz},
      {"moonElDeg", (double)hs.Env.MoonElDeg}, {"cloudCover", (double)hs.Env.CloudCover},
      {"windN", w.N}, {"windE", w.E}, {"windD", w.D}});

  const double lonPerM = 1.0 / (kMPerDeg * std::cos(lat * kDeg2Rad));
  /* WARM UNTIL THE WORLD IS THERE, not for a number of passes. The walk, if one was asked for, is a
   * declared length so its end standpoint stays a property of the flags and not of the network. */
  const bool walking = walkEastM != 0.0 || walkNorthM != 0.0;
  double clat = lat, clon = lon;
  int warmed = 0;
  while (warmed < warm && !W.Resident()) {
    if (walking && warmed < walkPasses) {
      clat = lat + walkNorthM * (double)warmed / kMPerDeg;
      clon = lon + walkEastM * (double)warmed * lonPerM;
      const double g = fb_stream_ground(clat, clon);
      GeoToEcef(clat, clon, (ElevationResolved(g) ? g : ground) + eyeM, eye);
      CameraBasisEcef(yawDeg, pitchDeg, 0.0, clat, clon, fwd, right, up);
      R.SetCameraBasis(eye, fwd, right, up);
    }
    W.Update(clat, clon, eye, fwd, (double)warmed * 1000.0 / 60.0);
    /* Die Streuung ist eine Funktion des Ortes, also wird sie neu ausgewertet und nicht fortgeschrieben.
     * 900 m: darueber ist ein 25-m-Baum bei 720p unter zwei Pixel breit und gehoert in die Ferne, die
     * das Material traegt. */
    /* EINMAL, wenn die Kacheln stehen. Die Streuung fragt je Stand die Gelaendehoehe, und 166 823
     * Abfragen alle 32 Paesse sind hundert Millionen ueber einen Vorlauf — gemessen als Haenger. */
    /* THE STANDPOINT IS CHECKED AGAINST WHAT IS DATA AND WHAT IS DRAWN. Terrain and buildings come
     * from DEM and OSM, so the eye is LIFTED above them and the lift is reported; a tree is a draw
     * from a landcover density, so a tree whose crown holds the eye is REFUSED (TreeField::Crown).
     * Buildings only exist once the vector tiles have landed, which is why this waits for residency
     * and re-seats the camera. */
    if (!standChecked && eyeAslM > -1.0e8 && W.Resident()) {
      standChecked = true;
      const double roof = W.RoofAslAt(lat, lon);
      const double wantAsl = ground + eyeM;
      if (roof > -1.0e29 && wantAsl < roof + kMinStandClearM) {
        const double lift = roof + kMinStandClearM - wantAsl;
        eyeM += lift;
        standLiftM += lift;
        altAsl = ground + eyeM;
        GeoToEcef(lat, lon, altAsl, eye);
        R.SetCameraBasis(eye, fwd, right, up);
        hs.Platform.AltM = (float)altAsl;
        R.SetSceneState(hs);
        Log::Info("walk", "standpoint_roof", {{"roofAslM", roof}, {"liftM", lift},
            {"eyeM", eyeM}, {"totalLiftM", standLiftM}});
      }
    }
    if (!worldTree.BarkIdx.empty() && treeStands.empty() && W.Resident()) {
      double ee = 0.0, nn = 0.0;
      W.Classes().Project(clat, clon, &ee, &nn);
      struct GroundCtx { const World::ClassField *c; } gc{&W.Classes()};
      auto groundAt = [](void *u, double e2, double n2) -> double {
        double la = 0.0, lo = 0.0;
        ((GroundCtx *)u)->c->FromEnu(e2, n2, &la, &lo);
        return fb_stream_ground(la, lo);
      };
      const double gcam = fb_stream_ground(clat, clon);
      /* Der AUGPUNKT, nicht der Boden unter ihm: der Shader legt den Fuss direkt auf die Aufhaengung
       * am Auge, also muss die Augenhoehe hier schon abgezogen sein. */
      treeField.Scatter(W.Classes(), veg, 900.0, ee, nn,
                        (ElevationResolved(gcam) ? gcam : ground) + eyeM, clat, worldTreeSigma,
                        treeCrown, groundAt, &gc, treeStands, treeDist);
      constexpr int kSF = World::TreeField::kStandFloats;
      R.SetTreeStands(treeStands.data(), (uint32_t)(treeStands.size() / (size_t)kSF),
                      treeDist.data());
      double lo[kSF], hi[kSF], sum = 0.0;
      for (int c = 0; c < kSF; c++) { lo[c] = 1e30; hi[c] = -1e30; }
      for (size_t s = 0; s + (size_t)kSF <= treeStands.size(); s += (size_t)kSF)
        for (int c = 0; c < kSF; c++) {
          const double v = treeStands[s + (size_t)c];
          if (v < lo[c]) lo[c] = v;
          if (v > hi[c]) hi[c] = v;
          if (c == 4) sum += v;
        }
      const size_t nst = treeStands.size() / (size_t)kSF;
      if (treeField.Cleared() > 0) {
        Log::Info("walk", "stand_cleared", {{"trees", (double)treeField.Cleared()},
            {"crownHalfM", (double)(treeCrown.HalfWidth * treeCrown.HeightM)},
            {"crownBottomM", (double)(treeCrown.Bottom * treeCrown.HeightM)},
            {"crownTopM", (double)(treeCrown.Top * treeCrown.HeightM)}});
      }
      Log::Info("walk", "treeline",
          {{"speciesLimitAslM", veg.Limit().SpeciesLimitM(clat)},
           {"highestStandAslM", treeField.HighestStandAslM()},
           {"refusedAboveLine", (double)treeField.AboveTreeline()},
           {"refusedTooSteep", (double)treeField.TooSteep()}});
      Log::Debug("walk", "trees", {{"stands", (int)nst},
          {"eastM", lo[0]}, {"eastMax", hi[0]}, {"northM", lo[1]}, {"northMax", hi[1]},
          {"footM", lo[2]}, {"footMax", hi[2]},
          {"scaleMin", lo[4]}, {"scaleMax", hi[4]}, {"scaleMean", nst ? sum / (double)nst : 0.0}});
    }
    R.RenderFrame();
    warmed++;
  }
  if (!W.Resident()) {
    /* A PICTURE OF A HALF-LOADED SCENE IS NOT A MEASUREMENT. The ceiling is a guard against a hung
     * server, never a quiet substitute for the shot that was asked for. */
    Log::Error("walk", "warm_ceiling_reached", {{"passes", warmed}, {"ceiling", warm},
        {"targetTotal", W.TargetTotal()}, {"targetReady", W.TargetReadyN()},
        {"progress", (double)W.LoadProgress()}, {"buildingTilesPending", W.BuildingPendingTiles()},
        {"base", base}});
    return 2;
  }
  Log::Info("walk", "resident", {{"passes", warmed}, {"ceiling", warm},
      {"targetTotal", W.TargetTotal()}, {"progress", (double)W.LoadProgress()}});

  /* THE CLASS AS THE SIMULATION SEES IT: the CPU evaluator over a declared world square, in the
   * class structure's own metric frame. No GPU is involved — this is the answer a headless actor
   * gets, and the picture is judged against it. */
  if (!classDump.empty()) {
    FILE *cf = fopen(classDump.c_str(), "wb");
    if (!cf) { Log::Error("walk", "class_dump_open_failed", {{"path", classDump}}); return 1; }
    double ce = 0, cn = 0;
    W.Classes().ToEnu(clat, clon, &ce, &cn);
    const int n = (int)(classSpan / classStep);
    /* SNAPPED TO THE WORLD, not to the camera: two runs from different standpoints must sample the
     * same points, or a sub-sample offset would show up as a disagreement that is not one. */
    const double e0 = std::floor((ce - classSpan * 0.5) / classStep) * classStep;
    const double n0 = std::floor((cn - classSpan * 0.5) / classStep) * classStep;
    char hdr[256];
    const int hl = snprintf(hdr, sizeof hdr, "OSCLS1 %d %.6f %.6f %.6f %.6f %.6f\n", n, e0, n0,
                            classStep, ce, cn);
    fwrite(hdr, 1, (size_t)hl, cf);
    std::vector<uint8_t> row((size_t)n);
    for (int j = 0; j < n; j++) {
      for (int i = 0; i < n; i++) {
        const int c = W.Classes().ClassAtEnu(e0 + ((double)i + 0.5) * classStep,
                                             n0 + ((double)j + 0.5) * classStep, nullptr, nullptr);
        row[(size_t)i] = (uint8_t)(c < 0 ? 255 : c);
      }
      fwrite(row.data(), 1, row.size(), cf);
    }
    fclose(cf);
    Log::Info("walk", "class_dumped", {{"path", classDump}, {"side", n}, {"stepM", classStep}});
  }

  /* THE ACCUMULATOR IS EMPTIED AND REFILLED FROM THE RESIDENT SCENE ALONE. Without this the picture
   * carries a history built while the tiles were still arriving, and two runs whose tiles arrived in
   * a different order differ in colour (0.44-0.53 % of pixels, measured 2026-08-07) although their
   * depth is bit-identical. The world is NOT stepped below: a settle frame may add nothing new. */
  const int settleFrames = settle >= 0 ? settle : R.TemporalSettleFrames();
  R.ResetTemporal();
  for (int f = 0; f < settleFrames; f++) R.RenderFrame();
  Log::Info("walk", "settled", {{"frames", settleFrames}});
  if (!treeStands.empty()) {
    Log::Info("walk", "trees_lod", {{"stands", (double)(treeStands.size() / 5)},
        {"meshStands", (double)R.TreeMeshStands()}, {"meshRadiusM", R.TreeMeshRadiusM()},
        {"rank0", (double)R.TreeRankStands(0)}, {"rank1", (double)R.TreeRankStands(1)},
        {"rank2", (double)R.TreeRankStands(2)}, {"rank3", (double)R.TreeRankStands(3)}, {"impostors", (double)R.TreeImpostorStands()},
        {"tris", (double)R.TreeTriangleCount()}});
  }

  /* THE FIELD ITSELF. Nothing below the size of a tree answers to it (doc/goal.md), so no stage reads
   * it today; it is published because a branch and a rotor owe the anchor and the field is what they
   * will be driven by. */
  {
    Render::WindField wf;
    wf.SetDeclared(windDeg, windMs);
    Log::Info("walk", "wind", {{"declaredFromDeg", windDeg}, {"declaredMs", windMs},
        {"canopyMs", wf.CanopyMs()}, {"profileRatio", windMs > 0.0 ? wf.CanopyMs() / windMs : 0.0},
        {"eigenHz", wf.EigenHz()}, {"waveLenM", wf.WaveLengthM()},
        {"phaseSpeedMs", wf.PhaseSpeedMs()}, {"gustAmp", wf.GustAmplitude()},
        {"dirE", wf.DirEast()}, {"dirN", wf.DirNorth()},
        {"cauchyMean", wf.CauchyAt(wf.CanopyMs(), 0.527)},
        {"tipDegMean", Render::WindField::TipAngleRad(wf.CauchyAt(wf.CanopyMs(), 0.527)) / kDeg2Rad},
        {"clockS", R.GetWindClock()}});
  }

  float irr[Render::IrradianceStage::kFloats] = {};
  if (R.ReadIrradiance(irr)) {
    const double sunY = 0.2126 * irr[0] + 0.7152 * irr[1] + 0.0722 * irr[2];
    const double skyY = 0.2126 * irr[4] + 0.7152 * irr[5] + 0.0722 * irr[6];
    const double sunUp = std::sin((double)sunEl * kPi / 180.0);
    Log::Info("walk", "irradiance", {{"sunDirectNormalY", sunY}, {"skyDiffuseHorizY", skyY},
        {"sunElDeg", (double)sunEl}, {"totalHorizY", sunY * sunUp + skyY},
        {"sunRGB", std::to_string(irr[0]) + "," + std::to_string(irr[1]) + "," + std::to_string(irr[2])},
        {"skyRGB", std::to_string(irr[4]) + "," + std::to_string(irr[5]) + "," + std::to_string(irr[6])},
        {"sunDeckRGB", std::to_string(irr[8]) + "," + std::to_string(irr[9]) + "," + std::to_string(irr[10])},
        {"deckBaseM", (double)irr[11]}});
  }
  /* THROUGHPUT, and it is the queue's and not the encoder's: the readback blocks on a map, so the
   * whole submitted run has to have finished before the clock is read. */
  if (bench > 0) {
    std::vector<uint8_t> warmRgba;
    R.ReadPixels(warmRgba);
    const auto t0 = std::chrono::steady_clock::now();
    for (int f = 0; f < bench; f++) R.RenderFrame();
    R.ReadPixels(warmRgba);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    Log::Info("walk", "bench", {{"frames", bench}, {"totalMs", ms}, {"msPerFrame", ms / bench}});
  }
  /* THE TURNTABLE, and it is the only price a scattered field has: a still frame prices one azimuth
   * and a forest is not isotropic. One full revolution, one measurement per frame, and the frame is
   * WAITED FOR — an unsynchronised loop times the encoder and not the GPU. */
  if (spin > 0) {
    std::vector<double> ms((size_t)spin);
    long lastMesh = 0, lastImp = 0, lastTris = 0;
    R.SyncGpu();
    for (int f = 0; f < spin; f++) {
      const double yd = yawDeg + 360.0 * (double)f / (double)spin;
      CameraBasisEcef(yd, pitchDeg, 0.0, clat, clon, fwd, right, up);
      R.SetCameraBasis(eye, fwd, right, up);
      const auto s0 = std::chrono::steady_clock::now();
      R.RenderFrame();
      R.SyncGpu();
      ms[(size_t)f] = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - s0).count();
      lastMesh = R.TreeMeshStands();
      lastImp = R.TreeImpostorStands();
      lastTris = R.TreeTriangleCount();
    }
    std::vector<double> so = ms;
    std::sort(so.begin(), so.end());
    const auto pct = [&so](double p) {
      size_t i = (size_t)(p * (double)(so.size() - 1) + 0.5);
      return so[i];
    };
    double sum = 0.0;
    for (double v : ms) sum += v;
    Log::Info("walk", "spin", {{"frames", (double)spin}, {"meanMs", sum / (double)spin},
        {"p50Ms", pct(0.50)}, {"p95Ms", pct(0.95)}, {"p99Ms", pct(0.99)},
        {"minMs", so.front()}, {"maxMs", so.back()},
        {"meshStands", (double)lastMesh}, {"impostorStands", (double)lastImp},
        {"treeTris", (double)lastTris}, {"width", (double)width}, {"height", (double)height}});
  }

  float met[Render::ExposureStage::kMeterFloats] = {};
  if (R.ReadExposure(met)) {
    Log::Info("walk", "exposure", {{"expScale", (double)met[0]}, {"keyLog2", (double)met[1]},
        {"horizE", (double)met[2]}});
  }
  Log::Info("walk", "terrain", {{"targetTotal", W.TargetTotal()}, {"targetReady", W.TargetReadyN()},
      {"progress", (double)W.LoadProgress()}, {"draws", R.DrawCount()},
      {"terrainTiles", R.TerrainVisibleTiles()},
      {"terrainDraws", R.TerrainDrawCalls()}, {"terrainTris", (double)R.TerrainTriangleCount()},
      {"buildingTris", (double)(R.BuildingVertexCount() / 3)},
      {"triangles", (double)R.TriangleCount()},
      {"classVramMB", (double)R.ClassVramBytes() / (1024.0 * 1024.0)},
      {"temporalVramMB", (double)R.TemporalVramBytes() / (1024.0 * 1024.0)},
      {"buildingVerts", (int)R.BuildingVertexCount()}});
  Log::Info("walk", "class", {{"edges", (int)W.Classes().EdgeCount()},
      {"seeds", (int)W.Classes().SeedCount()}, {"bufferKB", W.Classes().BufferBytes() / 1024.0},
      {"noDataFrac", W.Classes().NoDataFraction()},
      {"unknownKinds", (int)W.Classes().UnknownKinds()},
      {"unknownFeatures", (int)W.Classes().UnknownFeatures()},
      {"seedOverflow", W.Classes().SeedOverflow()}, {"buildMs", W.Classes().BuildMs()}});

  /* CPU AGAINST GPU, on the very pixels that were drawn: one geometry, one predicate, two evaluators
   * (doc/architecture.md). The GPU class index is not read back — it is not expressible through the
   * tone curve — so the picture is PARTITIONED by its own colour and each partition is checked
   * against the CPU answer. A bijection is 0 % disagreement; the bound is the fray, because the
   * fragment refines the boundary and the CPU does not. */
  if (classCmp) {
    std::vector<uint8_t> viz;
    std::vector<float> depth;
    if (!R.ReadPixels(viz) || !R.ReadDepth(depth)) {
      Log::Error("walk", "class_cmp_readback_failed");
      return 1;
    }
    const double tanH = std::tan(0.5 * (double)scene.FovDeg() * kDeg2Rad);
    const double aspect = (double)width / (double)height;
    std::map<uint32_t, std::map<int, long>> hist;
    long ground = 0, edgeNear = 0;
    const double *O = W.Classes().OriginEcef(), *Ea = W.Classes().EastEcef(),
                 *No = W.Classes().NorthEcef();
    for (int py = 0; py < height; py++)
      for (int px = 0; px < width; px++) {
        const float d = depth[(size_t)py * width + px];
        if (!(d > 1.0e-6f) || d >= 1.0f) continue;
        const double sx = (2.0 * ((double)px + 0.5) / width - 1.0) * tanH * aspect;
        const double sy = (1.0 - 2.0 * ((double)py + 0.5) / height) * tanH;
        double dir[3];
        for (int k = 0; k < 3; k++) dir[k] = fwd[k] + right[k] * sx + up[k] * sy;
        const double len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
        for (int k = 0; k < 3; k++) dir[k] /= len;
        const double cosOff = 1.0 / len;   /* dir . fwd, since fwd is a unit vector */
        const double range = (double)Render::Renderer::kNearM / ((double)d * cosOff);
        if (range > 2000.0) continue;      /* beyond the near tier the CPU answers from the far one */
        double p[3];
        for (int k = 0; k < 3; k++) p[k] = eye[k] + dir[k] * range - O[k];
        const double pe = p[0] * Ea[0] + p[1] * Ea[1] + p[2] * Ea[2];
        const double pn = p[0] * No[0] + p[1] * No[1] + p[2] * No[2];
        double dist = 0.0;
        const int c = W.Classes().ClassAtEnu(pe, pn, &dist, nullptr);
        if (c < 0) continue;
        ground++;
        /* Within one fray width of an outline the two are ALLOWED to differ: the fragment blends and
         * the point query does not. footM at this range is the fragment's own width. */
        if (dist < 0.05 + range / 688.0) { edgeNear++; continue; }
        const size_t o = ((size_t)py * width + px) * 4;
        const uint32_t key = ((uint32_t)viz[o] << 16) | ((uint32_t)viz[o + 1] << 8) | viz[o + 2];
        hist[key][c]++;
      }
    long agree = 0, total = 0, solidAgree = 0, solidTotal = 0, solidColours = 0;
    for (const auto &kv : hist) {
      long best = 0, sum = 0;
      for (const auto &cc : kv.second) { sum += cc.second; if (cc.second > best) best = cc.second; }
      agree += best;
      total += sum;
      /* A colour carried by a thousand pixels is a CLASS; one carried by a handful is the resolve
       * filter's blend across an edge and says nothing about either evaluator. */
      if (sum >= 1000) { solidAgree += best; solidTotal += sum; solidColours++; }
    }
    Log::Info("walk", "class_cmp", {{"groundPx", (double)ground}, {"comparedPx", (double)total},
        {"withinFrayPx", (double)edgeNear}, {"colours", (int)hist.size()},
        {"agreePct", total ? 100.0 * (double)agree / (double)total : 0.0},
        {"solidColours", (int)solidColours}, {"solidPx", (double)solidTotal},
        {"solidAgreePct", solidTotal ? 100.0 * (double)solidAgree / (double)solidTotal : 0.0}});
  }

  std::vector<uint8_t> rgba;
  if (!R.ReadPixels(rgba)) { Log::Error("walk", "readback_failed"); return 1; }
  if (!stbi_write_png(outPath.c_str(), width, height, 4, rgba.data(), width * 4)) {
    Log::Error("walk", "png_write_failed", {{"path", outPath}});
    return 1;
  }
  Log::Info("walk", "frame_written", {{"path", outPath}, {"w", width}, {"h", height}});
  if (!depthPath.empty()) {
    std::vector<float> d;
    if (!R.ReadDepth(d)) { Log::Error("walk", "depth_readback_failed"); return 1; }
    FILE *f = fopen(depthPath.c_str(), "wb");
    if (!f) { Log::Error("walk", "depth_write_failed", {{"path", depthPath}}); return 1; }
    fwrite(d.data(), sizeof(float), d.size(), f);
    fclose(f);
    Log::Info("walk", "depth_written", {{"path", depthPath}, {"nearM", (double)Render::Renderer::kNearM},
        {"fovDeg", scene.FovDeg()}});
  }

  /* THE STATE CHANNEL (doc/goal.md §4): the declared field read on a world line along the wind at the
   * declared times — no picture, no GPU. */
  if (!windProbe.empty()) {
    Render::WindField wf;
    wf.SetDeclared(windDeg, windMs);
    FILE *pf = fopen(windProbe.c_str(), "wb");
    if (!pf) { Log::Error("walk", "wind_probe_failed", {{"path", windProbe}}); return 1; }
    const int nx = 512;
    const double dx = 0.03;
    const int nt = seqFrames > 0 ? seqFrames : 240;
    fprintf(pf, "# dxM=%.6f nx=%d dtS=%.9f nt=%d bladeLenM=0.527 canopyMs=%.9f eigenHz=%.9f"
                " waveLenM=%.9f phaseSpeedMs=%.9f\n", dx, nx, seqDt, nt,
            wf.CanopyMs(), wf.EigenHz(), wf.WaveLengthM(), wf.PhaseSpeedMs());
    for (int t = 0; t < nt; t++) {
      const double tt = windT + seqDt * (double)t;
      for (int k = 0; k < nx; k++) {
        /* The line runs ALONG the wind through the scene's own point, in absolute world metres. */
        const double xi = (double)k * dx;
        const double u = wf.SpeedAt(xi * wf.DirEast(), xi * wf.DirNorth(), tt);
        const double th = Render::WindField::TipAngleRad(wf.CauchyAt(u, 0.527)) / kDeg2Rad;
        fprintf(pf, "%s%.6f", k ? "," : "", th);
      }
      fputc('\n', pf);
    }
    fclose(pf);
    Log::Info("walk", "wind_probe_written", {{"path", windProbe}, {"nx", nx}, {"nt", nt},
        {"dxM", dx}, {"dtS", seqDt}});
  }

  /* THE SEQUENCE, and the scene keeps ONE declared time. Only the wind clock moves between the
   * frames below — the sun, the streaming state and the standpoint are the ones the run converged
   * with, so a difference between two files can only be the flow. */
  if (seqFrames > 0) {
    /* THE MOVING MEASUREMENT. A throughput mean cannot see a hitch and a minimum cannot see it
     * either; what a stutter is, is the tail of a per-frame series, so the series is what gets
     * written. `--seq-prof` also drops the PNG: stbi's compression time depends on the picture and
     * would sit inside every frame time. */
    FILE *prof = nullptr;
    if (!seqProf.empty()) {
      prof = fopen(seqProf.c_str(), "wb");
      if (!prof) { Log::Error("walk", "seq_prof_open_failed", {{"path", seqProf}}); return 1; }
      fprintf(prof, "frame,distM,frameMs,worldMs,meshMs,albedoMs,uploadMs,buildingMs,bDecodeMs,"
                    "renderMs,gpuMs,"
                    "nodes,drawnLeaves,terrainTiles,draws,terrainTris,"
                    "buildingVerts,built,evicted,classVramMB,temporalVramMB\n");
    }
    if (!prof) mkdir(seqOut.c_str(), 0755);
    const double stepM = std::sqrt(seqStepE * seqStepE + seqStepN * seqStepN);
    for (int f = 0; f < seqFrames; f++) {
      const auto tf0 = std::chrono::steady_clock::now();
      R.SetWindClock(windT + seqDt * (double)f);
      if (seqYawDeg != 0.0 || seqStepE != 0.0 || seqStepN != 0.0) {
        const double flat = lat + seqStepN * (double)f / kMPerDeg;
        const double flon = lon + seqStepE * (double)f * lonPerM;
        const double g = fb_stream_ground(flat, flon);
        GeoToEcef(flat, flon, (ElevationResolved(g) ? g : ground) + eyeM, eye);
        CameraBasisEcef(yawDeg + seqYawDeg * (double)f, pitchDeg, 0.0, flat, flon, fwd, right, up);
        R.SetCameraBasis(eye, fwd, right, up);
        W.Update(flat, flon, eye, fwd, (double)(warmed + settleFrames + f) * 1000.0 / 60.0);
      } else if (prof) {
        W.Update(clat, clon, eye, fwd, (double)(warmed + settleFrames + f) * 1000.0 / 60.0);
      }
      const auto tf1 = std::chrono::steady_clock::now();
      R.RenderFrame();
      const auto tf2 = std::chrono::steady_clock::now();
      if (prof) {
        R.SyncGpu();
        const auto tf3 = std::chrono::steady_clock::now();
        const auto ms = [](auto a, auto b) {
          return std::chrono::duration<double, std::milli>(b - a).count();
        };
        fprintf(prof, "%d,%.3f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,"
                      "%d,%d,%d,%d,%ld,%u,%ld,%ld,%.3f,%.3f\n",
                f, stepM * (double)f, ms(tf0, tf3), W.UpdateMs(),
                W.MeshMs(), W.AlbedoMs(), W.UploadMs(), W.BuildingMs(), W.BuildingDecodeMs(),
                ms(tf1, tf2), ms(tf2, tf3),
                W.NodeCount(), W.DrawnLeafCount(), R.TerrainVisibleTiles(), R.DrawCount(),
                R.TerrainTriangleCount(),
                R.BuildingVertexCount(), W.BuiltCount(), W.EvictedCount(),
                (double)R.ClassVramBytes() / (1024.0 * 1024.0),
                (double)R.TemporalVramBytes() / (1024.0 * 1024.0));
        continue;
      }
      if (!R.ReadPixels(rgba)) { Log::Error("walk", "seq_readback_failed", {{"i", f}}); return 1; }
      char path[512];
      snprintf(path, sizeof path, "%s/%04d.png", seqOut.c_str(), f);
      if (!stbi_write_png(path, width, height, 4, rgba.data(), width * 4)) {
        Log::Error("walk", "seq_write_failed", {{"path", path}});
        return 1;
      }
      if (!depthPath.empty()) {
        std::vector<float> d;
        if (!R.ReadDepth(d)) { Log::Error("walk", "seq_depth_failed", {{"i", f}}); return 1; }
        snprintf(path, sizeof path, "%s/%04d.f32", seqOut.c_str(), f);
        FILE *df = fopen(path, "wb");
        if (!df) { Log::Error("walk", "seq_depth_write_failed", {{"path", path}}); return 1; }
        fwrite(d.data(), sizeof(float), d.size(), df);
        fclose(df);
      }
    }
    if (prof) {
      fclose(prof);
      Log::Info("walk", "seq_prof_written", {{"path", seqProf}, {"frames", seqFrames},
          {"stepM", stepM}});
    } else {
      Log::Info("walk", "seq_written", {{"dir", seqOut}, {"frames", seqFrames}, {"dtS", seqDt},
          {"t0S", windT}});
    }
  }
  return 0;
}
