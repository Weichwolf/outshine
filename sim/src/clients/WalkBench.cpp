#include "WalkBench.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sys/stat.h>

#include "Camera.h"
#include "Forest.h"
#include "Geodesy.h"
#include "Json.h"
#include "Log.h"
#include "Snapshot.h"
#include "SubjectBench.h"
#include "TreeFoliage.h"
#include "TreeGrower.h"
#include "TreeLeaf.h"
#include "TreeMesh.h"
#include "TreeSpecies.h"
#include "Units.h"
#include "WindField.h"
#include "stages/TreeStage.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace outshine::Clients {
namespace {

/* HARD-WIRED, because this is a bench and not a game: there is exactly one scene and choosing it is
 * not a feature. Relative to sim/, where the make targets run. */
const char *kScenePath = "../mods/demo/scene.json";   /* --scene overrides it, bench only */
const char *kVegetationPath = "assets/world/vegetation.json";
const char *kGroundMaterialPath = "assets/world/ground-materials.json";
const char *kStandSpeciesPath = "assets/world/species/buche.json";
const char *kMoonPath = "web/moon.jpg";
const char *kSpeciesDir = "assets/world/species";

/* THE BENCH'S CARD IS A DATUM, not a DEM sample: the subject bench has to run with no network at all,
 * and a plant's verdict may not depend on a tile server answering. 100.6 m is the reference scene's
 * own measured ground (`/elev?lat=52.10602&lon=9.43453&block=1`), so the air column over the bench is
 * the air column over the scene. */
const double kRigGroundAslM = 100.6;

/* THE BENCH'S FLOOR IS THE TEMPLATE'S DECLARED SUBSTRATE, and it has to be read from the class table
 * rather than from the resolved vegetation row: `swardClosure` overwrites a row's ground reflectance
 * with the stand's own aggregate colour — for `wiese` completely. On the bench that would be the
 * subject's colour standing in for the ground it is judged against. */
bool RigSubstrate(const World::GroundMaterials &mats, const std::string &tpl, std::string *name,
                  float rgb[3]) {
  FILE *f = fopen(kVegetationPath, "rb");
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

double MsBetween(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) {
  return std::chrono::duration<double, std::milli>(b - a).count();
}

}  // namespace

void WalkBench::Usage(const char *argv0) {
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

bool WalkBench::Parse(int argc, char **argv) {
  ScenePath_ = kScenePath;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--warm" && i + 1 < argc) Warm_ = atoi(argv[++i]);
    else if (a == "--walk" && i + 1 < argc) WalkPasses_ = atoi(argv[++i]);
    else if (a == "--settle" && i + 1 < argc) Settle_ = atoi(argv[++i]);
    else if (a == "--view" && i + 1 < argc) ViewKm_ = atof(argv[++i]);
    else if (a == "--base" && i + 1 < argc) Base_ = argv[++i];
    else if (a == "--out" && i + 1 < argc) Out_ = argv[++i];
    else if (a == "--depth" && i + 1 < argc) DepthPath_ = argv[++i];
    else if (a == "--bench" && i + 1 < argc) Bench_ = atoi(argv[++i]);
    else if (a == "--spin" && i + 1 < argc) Spin_ = atoi(argv[++i]);
    else if (a == "--ev" && i + 1 < argc) { ManualEv_ = true; EvStops_ = atof(argv[++i]); }
    else if (a == "--scene" && i + 1 < argc) ScenePath_ = argv[++i];
    else if (a == "--ortho" && i + 1 < argc) OrthoM_ = atof(argv[++i]);
    else if (a == "--eye" && i + 1 < argc) EyeOverrideM_ = atof(argv[++i]);
    else if (a == "--eye-asl" && i + 1 < argc) EyeAslM_ = atof(argv[++i]);
    else if (a == "--pitch" && i + 1 < argc) PitchDeg_ = atof(argv[++i]);
    else if (a == "--yaw" && i + 1 < argc) YawDeg_ = atof(argv[++i]);
    else if (a == "--wind-t" && i + 1 < argc) WindT_ = atof(argv[++i]);
    else if (a == "--wind-deg" && i + 1 < argc) WindDeg_ = atof(argv[++i]);
    else if (a == "--wind-ms" && i + 1 < argc) WindMs_ = atof(argv[++i]);
    else if (a == "--seq" && i + 1 < argc) SeqFrames_ = atoi(argv[++i]);
    else if (a == "--seq-dt" && i + 1 < argc) SeqDt_ = atof(argv[++i]);
    else if (a == "--seq-out" && i + 1 < argc) SeqOut_ = argv[++i];
    else if (a == "--seq-yaw" && i + 1 < argc) SeqYawDeg_ = atof(argv[++i]);
    else if (a == "--seq-stepE" && i + 1 < argc) SeqStepE_ = atof(argv[++i]);
    else if (a == "--seq-stepN" && i + 1 < argc) SeqStepN_ = atof(argv[++i]);
    else if (a == "--seq-prof" && i + 1 < argc) SeqProf_ = argv[++i];
    else if (a == "--wind-probe" && i + 1 < argc) WindProbe_ = argv[++i];
    else if (a == "--rig" && i + 1 < argc) RigTemplate_ = argv[++i];
    else if (a == "--rig-species" && i + 1 < argc) RigSpecies_ = argv[++i];
    else if (a == "--rig-no-leaves") RigLeaves_ = false;
    else if (a == "--rig-leaf-mult" && i + 1 < argc) RigLeafMult_ = atoi(argv[++i]);
    else if (a == "--rig-height" && i + 1 < argc) RigHeightM_ = atof(argv[++i]);
    else if (a == "--rig-out" && i + 1 < argc) RigOut_ = argv[++i];
    else if (a == "--rig-turn" && i + 1 < argc) RigTurn_ = atoi(argv[++i]);
    else if (a == "--snapshot" && i + 1 < argc) SnapshotPath_ = argv[++i];
    else if (a == "--stepE" && i + 1 < argc) StepE_ = atof(argv[++i]);
    else if (a == "--stepN" && i + 1 < argc) StepN_ = atof(argv[++i]);
    else if (a == "--walkE" && i + 1 < argc) WalkE_ = atof(argv[++i]);
    else if (a == "--walkN" && i + 1 < argc) WalkN_ = atof(argv[++i]);
    else if (a == "--class-dump" && i + 1 < argc) ClassDump_ = argv[++i];
    else if (a == "--class-span" && i + 1 < argc) ClassSpan_ = atof(argv[++i]);
    else if (a == "--class-step" && i + 1 < argc) ClassStep_ = atof(argv[++i]);
    else if (a == "--class-cmp") ClassCmp_ = true;
    else if (a == "--size" && i + 1 < argc) {
      std::string s = argv[++i];
      const size_t x = s.find('x');
      if (x == std::string::npos) { Usage(argv[0]); return false; }
      Width_ = atoi(s.substr(0, x).c_str());
      Height_ = atoi(s.substr(x + 1).c_str());
    } else { Usage(argv[0]); return false; }
  }
  return true;
}

int WalkBench::Run() {
  Scene scene;
  if (!scene.Load(ScenePath_.c_str())) {
    Log::Error("walk", "scene_load_failed", {{"path", ScenePath_}, {"why", scene.Error()}});
    return 1;
  }
  /* A STANDPOINT SOMEONE ELSE STOOD AT (clients/Snapshot.h). It replaces the standpoint flags rather
   * than combining with them: two ways of saying where the eye is, applied at once, is a picture
   * neither of them describes. */
  Snapshot snap;
  const bool haveSnapshot = !SnapshotPath_.empty();
  if (haveSnapshot) {
    if (StepE_ != 0.0 || StepN_ != 0.0 || YawDeg_ < 1.0e8 || PitchDeg_ < 1.0e8) {
      Log::Error("walk", "snapshot_conflicts_with_standpoint_flags", {{"path", SnapshotPath_}});
      return 1;
    }
    if (!snap.Load(SnapshotPath_.c_str())) {
      Log::Error("walk", "snapshot_load_failed", {{"path", SnapshotPath_}, {"why", snap.Error()}});
      return 1;
    }
    if (!snap.Matches(scene)) {
      Log::Error("walk", "snapshot_scene_mismatch", {{"path", SnapshotPath_}, {"why", snap.Error()}});
      return 1;
    }
  }

  const Outshine::Assets assets{kVegetationPath, kGroundMaterialPath, kStandSpeciesPath, kMoonPath};
  Outshine app(scene, assets);
  Outshine::Stance st;
  st.Lat = haveSnapshot ? snap.Lat() : scene.Lat() + StepN_ / kMPerDeg;
  st.Lon = haveSnapshot ? snap.Lon()
                        : scene.Lon() + StepE_ / (kMPerDeg * std::cos(scene.Lat() * kDeg2Rad));
  st.YawDeg = haveSnapshot ? snap.YawDeg() : (YawDeg_ < 1.0e8 ? YawDeg_ : scene.YawDeg());
  st.PitchDeg = haveSnapshot ? snap.PitchDeg() : (PitchDeg_ < 1.0e8 ? PitchDeg_ : scene.PitchDeg());
  app.SetStance(st);
  app.SetTilesBase(Base_);
  app.SetViewM(ViewKm_ * 1000.0);
  app.SetOrthoM(OrthoM_);
  app.SetWind(WindDeg_ < 1.0e8 ? WindDeg_ : scene.WindDeg(),
              WindMs_ >= 0.0 ? WindMs_ : scene.WindMs());
  if (ManualEv_) app.SetExposureCompEv(EvStops_);
  if (EyeOverrideM_ >= 0.0) app.SetEyeAglM(EyeOverrideM_);
  if (EyeAslM_ > -1.0e8) app.SetLensAslM(EyeAslM_);

  Outshine::Gpu gpu;
  gpu.Width = Width_;
  gpu.Height = Height_;
  if (!app.Prepare(gpu)) return 1;
  app.SetWindClock(WindT_);

  /* THE SUBJECT BENCH takes the binary over completely: no World, no tile stream, no scene light.
   * doc/goal.md §3 — a plant is judged alone before it is allowed into the picture. */
  if (!RigTemplate_.empty() || !RigSpecies_.empty()) return RunRig(app);

  if (!app.Open()) return 1;
  if (haveSnapshot)
    /* WHAT THE OTHER CLIENT ANSWERED AT THE SAME PLACE, subtracted here and nowhere else: a DEM or an
     * ephemeris that disagreed moves the whole picture, and no pixel comparison would name the cause. */
    Log::Info("walk", "snapshot", {{"name", snap.Name()}, {"client", snap.Client()},
        {"lat", app.Lat()}, {"lon", app.Lon()}, {"yawDeg", app.YawDeg()},
        {"pitchDeg", app.PitchDeg()}, {"groundM", app.Standpoint().GroundAslM()},
        {"dGroundM", app.Standpoint().GroundAslM() - snap.GroundM()},
        {"dAltAslM", app.Standpoint().AltAslM() - snap.AltAslM()},
        {"dSunElDeg", (double)app.SunElDeg() - snap.SunElDeg()},
        {"dSunAzDeg", (double)app.SunAzDeg() - snap.SunAzDeg()}});
  return RunScene(app);
}

int WalkBench::RunRig(Outshine &app) {
  SubjectBench bench(app.Renderer(), app.Vegetation());
  /* Held for the whole bench run: the stage borrows nothing, but the arrays are uploaded once and
   * the numbers below are logged off the same objects the picture was drawn from. */
  World::TreeMesh mesh;
  World::TreeFoliage foliage;
  if (!RigSpecies_.empty()) {
    const std::string path = std::string(kSpeciesDir) + "/" + RigSpecies_ + ".json";
    World::TreeSpecies sp;
    if (!World::Forest::LoadSpecies(path.c_str(), &sp)) {
      Log::Error("walk", "rig_species_unreadable", {{"path", path}, {"why", sp.Error()}});
      return 1;
    }
    World::TreeGrower grower;
    const auto t0 = std::chrono::steady_clock::now();
    /* THE SAME MESH THE NEAREST FIELD RANK GETS. Asking for pixel 0 asked for a tube on every 3 mm
     * twig — a mesh nobody draws and, before the budget was solved rather than cut, one that
     * truncated the crown. */
    grower.Grow(sp, mesh, Render::TreeStage::RankPixel(0));
    const auto t1 = std::chrono::steady_clock::now();
    World::TreeLeaf::Build(sp.LeafParams(), mesh);
    const auto t2 = std::chrono::steady_clock::now();
    foliage.Build(mesh, sp, RigLeafMult_);
    const auto t3 = std::chrono::steady_clock::now();

    const double h = RigHeightM_ > 0.0 ? RigHeightM_ : (double)sp.HeightM();
    const double crownX = (double)(mesh.BoxMax.X - mesh.BoxMin.X) * h;
    const double crownZ = (double)(mesh.BoxMax.Z - mesh.BoxMin.Z) * h;
    const double proj = 0.25 * kPi * crownX * crownZ;

    Render::Renderer &r = app.Renderer();
    r.SetTreeLook(World::Forest::LookOf(sp));
    r.SetTreeBark(0, mesh.BarkVerts.data(), (uint32_t)mesh.BarkVertexCount(), mesh.BarkIdx.data(),
                  (uint32_t)mesh.BarkIdx.size());
    r.SetTreeLeaf(mesh.LeafVerts.data(), (uint32_t)mesh.LeafVertexCount(), mesh.LeafIdx.data(),
                  (uint32_t)mesh.LeafIdx.size(), foliage.Instances().data(),
                  (uint32_t)foliage.Count(), foliage.ScaleM());
    r.SetTreeLeavesVisible(RigLeaves_);
    Log::Info("walk", "rig_tree", {{"species", RigSpecies_}, {"heightM", h},
        {"declaredSpreadM", (double)sp.SpreadM()}, {"grownCrownXM", crownX},
        {"grownCrownZM", crownZ},
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
        {"growMs", MsBetween(t0, t1)}, {"leafMs", MsBetween(t1, t2)},
        {"foliageMs", MsBetween(t2, t3)},
        {"leafMult", (double)RigLeafMult_}, {"leavesDrawn", RigLeaves_ ? 1.0 : 0.0}});
    if (!bench.SelectTree(RigSpecies_.c_str(), h)) {
      Log::Error("walk", "rig_species_height_invalid", {{"name", RigSpecies_}});
      return 1;
    }
  } else if (!bench.Select(RigTemplate_.c_str(), RigHeightM_)) {
    Log::Error("walk", "rig_unknown_subject", {{"name", RigTemplate_}});
    return 1;
  }
  /* A TREE DECLARES NO SUBSTRATE. Without a template naming a ground class the floor stays at the
   * bench's 18 % neutral, which is what a subject without a declared substrate deserves. */
  if (!RigTemplate_.empty()) {
    std::string subName;
    float subRgb[3] = {0, 0, 0};
    if (!RigSubstrate(app.Materials(), RigTemplate_, &subName, subRgb)) {
      Log::Error("walk", "rig_substrate_unresolved", {{"template", RigTemplate_}});
      return 1;
    }
    bench.SetSubstrate(subName, subRgb);
  }
  mkdir(RigOut_.c_str(), 0755);
  bench.Stand(app.Declared().Lat(), app.Declared().Lon(), kRigGroundAslM);
  bench.SetOutDir(RigOut_);
  bench.SetTurntableSteps(RigTurn_);
  return bench.Run() ? 0 : 1;
}

bool WalkBench::Warm(Outshine &app) {
  const double lat0 = app.Lat(), lon0 = app.Lon();
  const double lonPerM = 1.0 / (kMPerDeg * std::cos(lat0 * kDeg2Rad));
  /* WARM UNTIL THE WORLD IS THERE, not for a number of passes. The walk, if one was asked for, is a
   * declared length so its end standpoint stays a property of the flags and not of the network. */
  const bool walking = WalkE_ != 0.0 || WalkN_ != 0.0;
  Outshine::Progress p;
  Warmed_ = 0;
  while (Warmed_ < Warm_ && !p.Resident) {
    if (walking && Warmed_ < WalkPasses_) {
      Outshine::Stance s{lat0 + WalkN_ * (double)Warmed_ / kMPerDeg,
                         lon0 + WalkE_ * (double)Warmed_ * lonPerM, app.YawDeg(), app.PitchDeg()};
      app.Look(s);
    }
    p = app.Stream((double)Warmed_ * 1000.0 / 60.0);
    app.Frame();
    Warmed_++;
  }
  if (!p.Resident) {
    /* A PICTURE OF A HALF-LOADED SCENE IS NOT A MEASUREMENT. The ceiling is a guard against a hung
     * server, never a quiet substitute for the shot that was asked for. */
    Log::Error("walk", "warm_ceiling_reached", {{"passes", Warmed_}, {"ceiling", Warm_},
        {"targetTotal", app.Scenery().TargetTotal()}, {"targetReady", app.Scenery().TargetReadyN()},
        {"progress", (double)p.Fraction},
        {"buildingTilesPending", app.Scenery().BuildingPendingTiles()}, {"base", Base_}});
    return false;
  }
  Log::Info("walk", "resident", {{"passes", Warmed_}, {"ceiling", Warm_},
      {"targetTotal", app.Scenery().TargetTotal()}, {"progress", (double)p.Fraction}});
  return true;
}

int WalkBench::RunScene(Outshine &app) {
  if (!Warm(app)) return 2;
  if (!ClassDump_.empty() && !DumpClasses(app)) return 1;

  /* THE ACCUMULATOR IS EMPTIED AND REFILLED FROM THE RESIDENT SCENE ALONE. Without this the picture
   * carries a history built while the tiles were still arriving, and two runs whose tiles arrived in
   * a different order differ in colour although their depth is bit-identical. The world is NOT
   * stepped below: a settle frame may add nothing new. */
  SettleFrames_ = Settle_ >= 0 ? Settle_ : app.Renderer().TemporalSettleFrames();
  app.Renderer().ResetTemporal();
  for (int f = 0; f < SettleFrames_; f++) app.Frame();
  Log::Info("walk", "settled", {{"frames", SettleFrames_}});

  ReportSettled(app);
  if (Spin_ > 0) Spin(app);
  ReportCounters(app);
  if (ClassCmp_ && !CompareClasses(app)) return 1;
  if (!WritePng(app, Out_.c_str())) return 1;
  if (!DepthPath_.empty() && !WriteDepth(app, DepthPath_.c_str())) return 1;
  if (!WindProbe_.empty() && !ProbeWind(app)) return 1;
  if (SeqFrames_ > 0 && !Sequence(app)) return 1;
  return 0;
}

void WalkBench::ReportSettled(Outshine &app) const {
  Render::Renderer &R = app.Renderer();
  if (app.Forest().StandCount() > 0)
    Log::Info("walk", "trees_lod", {{"stands", (double)app.Forest().StandCount()},
        {"meshStands", (double)R.TreeMeshStands()}, {"meshRadiusM", R.TreeMeshRadiusM()},
        {"rank0", (double)R.TreeRankStands(0)}, {"rank1", (double)R.TreeRankStands(1)},
        {"rank2", (double)R.TreeRankStands(2)}, {"rank3", (double)R.TreeRankStands(3)},
        {"impostors", (double)R.TreeImpostorStands()}, {"tris", (double)R.TreeTriangleCount()}});

  /* THE FIELD ITSELF. Nothing below the size of a tree answers to it (doc/goal.md), so no stage reads
   * it today; it is published because a branch and a rotor owe the anchor and the field is what they
   * will be driven by. */
  {
    Render::WindField wf;
    wf.SetDeclared(app.WindDeg(), app.WindMs());
    Log::Info("walk", "wind", {{"declaredFromDeg", app.WindDeg()}, {"declaredMs", app.WindMs()},
        {"canopyMs", wf.CanopyMs()},
        {"profileRatio", app.WindMs() > 0.0 ? wf.CanopyMs() / app.WindMs() : 0.0},
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
    const double sunUp = std::sin((double)app.SunElDeg() * kPi / 180.0);
    Log::Info("walk", "irradiance", {{"sunDirectNormalY", sunY}, {"skyDiffuseHorizY", skyY},
        {"sunElDeg", (double)app.SunElDeg()}, {"totalHorizY", sunY * sunUp + skyY},
        {"sunRGB", std::to_string(irr[0]) + "," + std::to_string(irr[1]) + "," + std::to_string(irr[2])},
        {"skyRGB", std::to_string(irr[4]) + "," + std::to_string(irr[5]) + "," + std::to_string(irr[6])},
        {"sunDeckRGB", std::to_string(irr[8]) + "," + std::to_string(irr[9]) + "," + std::to_string(irr[10])},
        {"deckBaseM", (double)irr[11]}});
  }
  /* THROUGHPUT, and it is the queue's and not the encoder's: the readback blocks on a map, so the
   * whole submitted run has to have finished before the clock is read. */
  if (Bench_ > 0) {
    std::vector<uint8_t> warmRgba;
    R.ReadPixels(warmRgba);
    const auto t0 = std::chrono::steady_clock::now();
    for (int f = 0; f < Bench_; f++) R.RenderFrame();
    R.ReadPixels(warmRgba);
    const double ms = MsBetween(t0, std::chrono::steady_clock::now());
    Log::Info("walk", "bench", {{"frames", Bench_}, {"totalMs", ms}, {"msPerFrame", ms / Bench_}});
  }
}

void WalkBench::ReportCounters(Outshine &app) const {
  Render::Renderer &R = app.Renderer();
  World::World &W = app.Scenery();
  float met[Render::ExposureStage::kMeterFloats] = {};
  if (R.ReadExposure(met))
    Log::Info("walk", "exposure", {{"expScale", (double)met[0]}, {"keyLog2", (double)met[1]},
        {"horizE", (double)met[2]}});
  std::string lvl;
  for (int i = 0; i < Render::TilesStage::kLevelBins; i++)
    if (R.TerrainTrianglesByLevel()[i] > 0)
      lvl += "L" + std::to_string(i) + "=" + std::to_string(R.TerrainTrianglesByLevel()[i]) + " ";
  Log::Info("walk", "terrain", {{"cutLevels", lvl}, {"targetTotal", W.TargetTotal()},
      {"targetReady", W.TargetReadyN()}, {"progress", (double)W.LoadProgress()},
      {"draws", R.DrawCount()}, {"terrainTiles", R.TerrainVisibleTiles()},
      {"terrainDraws", R.TerrainDrawCalls()}, {"terrainTris", (double)R.TerrainTriangleCount()},
      {"buildingTris", (double)(R.BuildingVertexCount() / 3)},
      {"shadowTris", (double)R.ShadowTriangleCount()}, {"shadowDraws", R.ShadowDrawCalls()},
      {"triangles", (double)R.TriangleCount()},
      {"classVramMB", (double)R.ClassVramBytes() / (1024.0 * 1024.0)},
      {"tileMeshMB", (double)R.TileMeshBytes() / (1024.0 * 1024.0)},
      {"temporalVramMB", (double)R.TemporalVramBytes() / (1024.0 * 1024.0)},
      {"buildingVerts", (int)R.BuildingVertexCount()}});
  Log::Info("walk", "class", {{"edges", (int)W.Classes().EdgeCount()},
      {"seeds", (int)W.Classes().SeedCount()}, {"bufferKB", W.Classes().BufferBytes() / 1024.0},
      {"noDataFrac", W.Classes().NoDataFraction()},
      {"unknownKinds", (int)W.Classes().UnknownKinds()},
      {"unknownFeatures", (int)W.Classes().UnknownFeatures()},
      {"seedOverflow", W.Classes().SeedOverflow()}, {"buildMs", W.Classes().BuildMs()}});
}

/* THE CLASS AS THE SIMULATION SEES IT: the CPU evaluator over a declared world square, in the class
 * structure's own metric frame. No GPU is involved — this is the answer a headless actor gets, and
 * the picture is judged against it. */
bool WalkBench::DumpClasses(Outshine &app) const {
  FILE *cf = fopen(ClassDump_.c_str(), "wb");
  if (!cf) { Log::Error("walk", "class_dump_open_failed", {{"path", ClassDump_}}); return false; }
  double ce = 0, cn = 0;
  app.Scenery().Classes().ToEnu(app.Lat(), app.Lon(), &ce, &cn);
  const int n = (int)(ClassSpan_ / ClassStep_);
  /* SNAPPED TO THE WORLD, not to the camera: two runs from different standpoints must sample the
   * same points, or a sub-sample offset would show up as a disagreement that is not one. */
  const double e0 = std::floor((ce - ClassSpan_ * 0.5) / ClassStep_) * ClassStep_;
  const double n0 = std::floor((cn - ClassSpan_ * 0.5) / ClassStep_) * ClassStep_;
  char hdr[256];
  const int hl = snprintf(hdr, sizeof hdr, "OSCLS1 %d %.6f %.6f %.6f %.6f %.6f\n", n, e0, n0,
                          ClassStep_, ce, cn);
  fwrite(hdr, 1, (size_t)hl, cf);
  std::vector<uint8_t> row((size_t)n);
  for (int j = 0; j < n; j++) {
    for (int i = 0; i < n; i++) {
      const int c = app.Scenery().Classes().ClassAtEnu(e0 + ((double)i + 0.5) * ClassStep_,
                                                       n0 + ((double)j + 0.5) * ClassStep_,
                                                       nullptr, nullptr);
      row[(size_t)i] = (uint8_t)(c < 0 ? 255 : c);
    }
    fwrite(row.data(), 1, row.size(), cf);
  }
  fclose(cf);
  Log::Info("walk", "class_dumped", {{"path", ClassDump_}, {"side", n}, {"stepM", ClassStep_}});
  return true;
}

/* CPU AGAINST GPU, on the very pixels that were drawn: one geometry, one predicate, two evaluators
 * (doc/architecture.md). The GPU class index is not read back — it is not expressible through the
 * tone curve — so the picture is PARTITIONED by its own colour and each partition is checked against
 * the CPU answer. A bijection is 0 % disagreement; the bound is the fray, because the fragment
 * refines the boundary and the CPU does not. */
bool WalkBench::CompareClasses(Outshine &app) const {
  std::vector<uint8_t> viz;
  std::vector<float> depth;
  if (!app.Renderer().ReadPixels(viz) || !app.Renderer().ReadDepth(depth)) {
    Log::Error("walk", "class_cmp_readback_failed");
    return false;
  }
  const double tanH = std::tan(0.5 * app.Declared().FovDeg() * kDeg2Rad);
  const double aspect = (double)Width_ / (double)Height_;
  const World::ClassField &cls = app.Scenery().Classes();
  const double *O = cls.OriginEcef(), *Ea = cls.EastEcef(), *No = cls.NorthEcef();
  const double *eye = app.Eye(), *fwd = app.Fwd(), *right = app.Right(), *up = app.Up();
  std::map<uint32_t, std::map<int, long>> hist;
  long ground = 0, edgeNear = 0;
  for (int py = 0; py < Height_; py++)
    for (int px = 0; px < Width_; px++) {
      const float d = depth[(size_t)py * Width_ + px];
      if (!(d > 1.0e-6f) || d >= 1.0f) continue;
      const double sx = (2.0 * ((double)px + 0.5) / Width_ - 1.0) * tanH * aspect;
      const double sy = (1.0 - 2.0 * ((double)py + 0.5) / Height_) * tanH;
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
      const int c = cls.ClassAtEnu(pe, pn, &dist, nullptr);
      if (c < 0) continue;
      ground++;
      /* Within one fray width of an outline the two are ALLOWED to differ: the fragment blends and
       * the point query does not. footM at this range is the fragment's own width. */
      if (dist < 0.05 + range / 688.0) { edgeNear++; continue; }
      const size_t o = ((size_t)py * Width_ + px) * 4;
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
  return true;
}

/* THE TURNTABLE, and it is the only price a scattered field has: a still frame prices one azimuth
 * and a forest is not isotropic. One full revolution, one measurement per frame, and the frame is
 * WAITED FOR — an unsynchronised loop times the encoder and not the GPU. */
void WalkBench::Spin(Outshine &app) const {
  Render::Renderer &R = app.Renderer();
  const double yaw0 = app.YawDeg();
  std::vector<double> ms((size_t)Spin_);
  long lastMesh = 0, lastImp = 0, lastTris = 0;
  R.SyncGpu();
  for (int f = 0; f < Spin_; f++) {
    app.Look({app.Lat(), app.Lon(), yaw0 + 360.0 * (double)f / (double)Spin_, app.PitchDeg()});
    const auto s0 = std::chrono::steady_clock::now();
    app.Frame();
    R.SyncGpu();
    ms[(size_t)f] = MsBetween(s0, std::chrono::steady_clock::now());
    lastMesh = R.TreeMeshStands();
    lastImp = R.TreeImpostorStands();
    lastTris = R.TreeTriangleCount();
  }
  std::vector<double> so = ms;
  std::sort(so.begin(), so.end());
  const auto pct = [&so](double p) {
    return so[(size_t)(p * (double)(so.size() - 1) + 0.5)];
  };
  double sum = 0.0;
  for (double v : ms) sum += v;
  Log::Info("walk", "spin", {{"frames", (double)Spin_}, {"meanMs", sum / (double)Spin_},
      {"p50Ms", pct(0.50)}, {"p95Ms", pct(0.95)}, {"p99Ms", pct(0.99)},
      {"minMs", so.front()}, {"maxMs", so.back()},
      {"meshStands", (double)lastMesh}, {"impostorStands", (double)lastImp},
      {"treeTris", (double)lastTris}, {"width", (double)Width_}, {"height", (double)Height_}});
  app.Look({app.Lat(), app.Lon(), yaw0, app.PitchDeg()});
}

bool WalkBench::WriteImage(Outshine &app, const char *path) const {
  std::vector<uint8_t> rgba;
  if (!app.Renderer().ReadPixels(rgba)) { Log::Error("walk", "readback_failed"); return false; }
  if (!stbi_write_png(path, Width_, Height_, 4, rgba.data(), Width_ * 4)) {
    Log::Error("walk", "png_write_failed", {{"path", std::string(path)}});
    return false;
  }
  return true;
}

bool WalkBench::WritePng(Outshine &app, const char *path) const {
  if (!WriteImage(app, path)) return false;
  Log::Info("walk", "frame_written", {{"path", std::string(path)}, {"w", Width_}, {"h", Height_}});
  return true;
}

bool WalkBench::WriteDepth(Outshine &app, const char *path) const {
  std::vector<float> d;
  if (!app.Renderer().ReadDepth(d)) { Log::Error("walk", "depth_readback_failed"); return false; }
  FILE *f = fopen(path, "wb");
  if (!f) { Log::Error("walk", "depth_write_failed", {{"path", std::string(path)}}); return false; }
  fwrite(d.data(), sizeof(float), d.size(), f);
  fclose(f);
  Log::Info("walk", "depth_written", {{"path", std::string(path)},
      {"nearM", (double)Render::Renderer::kNearM}, {"fovDeg", app.Declared().FovDeg()}});
  return true;
}

/* THE STATE CHANNEL (doc/goal.md §4): the declared field read on a world line along the wind at the
 * declared times — no picture, no GPU. */
bool WalkBench::ProbeWind(Outshine &app) const {
  Render::WindField wf;
  wf.SetDeclared(app.WindDeg(), app.WindMs());
  FILE *pf = fopen(WindProbe_.c_str(), "wb");
  if (!pf) { Log::Error("walk", "wind_probe_failed", {{"path", WindProbe_}}); return false; }
  const int nx = 512;
  const double dx = 0.03;
  const int nt = SeqFrames_ > 0 ? SeqFrames_ : 240;
  fprintf(pf, "# dxM=%.6f nx=%d dtS=%.9f nt=%d bladeLenM=0.527 canopyMs=%.9f eigenHz=%.9f"
              " waveLenM=%.9f phaseSpeedMs=%.9f\n", dx, nx, SeqDt_, nt,
          wf.CanopyMs(), wf.EigenHz(), wf.WaveLengthM(), wf.PhaseSpeedMs());
  for (int t = 0; t < nt; t++) {
    const double tt = WindT_ + SeqDt_ * (double)t;
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
  Log::Info("walk", "wind_probe_written", {{"path", WindProbe_}, {"nx", nx}, {"nt", nt},
      {"dxM", dx}, {"dtS", SeqDt_}});
  return true;
}

/* THE SEQUENCE, and the scene keeps ONE declared time. Only the wind clock moves between the frames
 * below — the sun, the streaming state and the standpoint are the ones the run converged with, so a
 * difference between two files can only be the flow. */
bool WalkBench::Sequence(Outshine &app) {
  /* THE MOVING MEASUREMENT. A throughput mean cannot see a hitch and a minimum cannot see it either;
   * what a stutter is, is the tail of a per-frame series, so the series is what gets written.
   * `--seq-prof` also drops the PNG: stbi's compression time depends on the picture and would sit
   * inside every frame time. */
  World::World &W = app.Scenery();
  Render::Renderer &R = app.Renderer();
  FILE *prof = nullptr;
  if (!SeqProf_.empty()) {
    prof = fopen(SeqProf_.c_str(), "wb");
    if (!prof) { Log::Error("walk", "seq_prof_open_failed", {{"path", SeqProf_}}); return false; }
    fprintf(prof, "frame,distM,frameMs,worldMs,meshMs,albedoMs,uploadMs,buildingMs,bDecodeMs,"
                  "renderMs,gpuMs,"
                  "nodes,drawnLeaves,terrainTiles,draws,terrainTris,"
                  "buildingVerts,built,evicted,classVramMB,temporalVramMB\n");
  }
  if (!prof) mkdir(SeqOut_.c_str(), 0755);
  const double lat0 = app.Lat(), lon0 = app.Lon(), yaw0 = app.YawDeg();
  const double lonPerM = 1.0 / (kMPerDeg * std::cos(lat0 * kDeg2Rad));
  const bool moving = SeqYawDeg_ != 0.0 || SeqStepE_ != 0.0 || SeqStepN_ != 0.0;
  const double stepM = std::sqrt(SeqStepE_ * SeqStepE_ + SeqStepN_ * SeqStepN_);
  for (int f = 0; f < SeqFrames_; f++) {
    const auto tf0 = std::chrono::steady_clock::now();
    app.SetWindClock(WindT_ + SeqDt_ * (double)f);
    if (moving) {
      app.Look({lat0 + SeqStepN_ * (double)f / kMPerDeg, lon0 + SeqStepE_ * (double)f * lonPerM,
                yaw0 + SeqYawDeg_ * (double)f, app.PitchDeg()});
      app.Stream((double)(Warmed_ + SettleFrames_ + f) * 1000.0 / 60.0);
    } else if (prof) {
      app.Stream((double)(Warmed_ + SettleFrames_ + f) * 1000.0 / 60.0);
    }
    const auto tf1 = std::chrono::steady_clock::now();
    app.Frame();
    const auto tf2 = std::chrono::steady_clock::now();
    if (prof) {
      R.SyncGpu();
      const auto tf3 = std::chrono::steady_clock::now();
      fprintf(prof, "%d,%.3f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,"
                    "%d,%d,%d,%d,%ld,%u,%ld,%ld,%.3f,%.3f\n",
              f, stepM * (double)f, MsBetween(tf0, tf3), W.UpdateMs(),
              W.MeshMs(), W.AlbedoMs(), W.UploadMs(), W.BuildingMs(), W.BuildingDecodeMs(),
              MsBetween(tf1, tf2), MsBetween(tf2, tf3),
              W.NodeCount(), W.DrawnLeafCount(), R.TerrainVisibleTiles(), R.DrawCount(),
              R.TerrainTriangleCount(), R.BuildingVertexCount(), W.BuiltCount(), W.EvictedCount(),
              (double)R.ClassVramBytes() / (1024.0 * 1024.0),
              (double)R.TemporalVramBytes() / (1024.0 * 1024.0));
      continue;
    }
    char path[512];
    snprintf(path, sizeof path, "%s/%04d.png", SeqOut_.c_str(), f);
    if (!WriteImage(app, path)) return false;
    if (!DepthPath_.empty()) {
      snprintf(path, sizeof path, "%s/%04d.f32", SeqOut_.c_str(), f);
      if (!WriteDepth(app, path)) return false;
    }
  }
  if (prof) {
    fclose(prof);
    Log::Info("walk", "seq_prof_written", {{"path", SeqProf_}, {"frames", SeqFrames_},
        {"stepM", stepM}});
  } else {
    Log::Info("walk", "seq_written", {{"dir", SeqOut_}, {"frames", SeqFrames_}, {"dtS", SeqDt_},
        {"t0S", WindT_}});
  }
  return true;
}

} // namespace outshine::Clients
