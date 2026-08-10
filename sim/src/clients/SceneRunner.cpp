#include "SceneRunner.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>

#include "Forest.h"
#include "Geodesy.h"
#include "Json.h"
#include "Log.h"
#include "SubjectBench.h"
#include "TreeFoliage.h"
#include "TreeGrower.h"
#include "TreeLeaf.h"
#include "TreeMesh.h"
#include "TreeRanks.h"
#include "TreeSpecies.h"
#include "Units.h"
#include "WindField.h"

namespace outshine::Clients {
namespace {

using Target = Animation::Target;

/* THE BENCH'S CARD IS A DATUM, not a DEM sample: the subject bench has to run with no network at
 * all, and a plant's verdict may not depend on a tile server answering. 100.6 m is the reference
 * scene's own measured ground (`/elev?lat=52.10602&lon=9.43453&block=1`), so the air column over the
 * bench is the air column over the scene. */
const double kSubjectGroundAslM = 100.6;
const char *kSpeciesDir = "assets/world/species";

double MsBetween(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) {
  return std::chrono::duration<double, std::milli>(b - a).count();
}

double Percentile(const std::vector<double> &sorted, double p) {
  if (sorted.empty()) return 0.0;
  return sorted[(size_t)(p * (double)(sorted.size() - 1) + 0.5)];
}

/* THE BENCH'S FLOOR IS THE TEMPLATE'S DECLARED SUBSTRATE, and it has to be read from the class table
 * rather than from the resolved vegetation row: `swardClosure` overwrites a row's ground reflectance
 * with the stand's own aggregate colour — for `meadow` completely. On the bench that would be the
 * subject's colour standing in for the ground it is judged against. */
bool SubjectSubstrate(const World::GroundMaterials &mats, const std::string &vegPath,
                      const std::string &tpl, std::string *name, float rgb[3]) {
  FILE *f = fopen(vegPath.c_str(), "rb");
  if (!f) return false;
  std::string text;
  char buf[8192];
  for (size_t n; (n = fread(buf, 1, sizeof buf, f)) > 0;) text.append(buf, n);
  fclose(f);

  Json doc;
  if (!doc.Parse(text.c_str(), text.size())) return false;
  const Json::Ref ts = doc.Root()["templates"];
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

}  // namespace

bool SceneRunner::IsSubjectBench() const {
  return !Scene_.Runs().empty() && Scene_.Runs().front().What == Scene::Run::Kind::Subject;
}

std::string SceneRunner::FrameName(const std::string &path, int frame, const char *ext) const {
  char buf[32];
  snprintf(buf, sizeof buf, "/%04d.%s", frame, ext);
  return path + buf;
}

/* THE COUNTERS COME AFTER THE PRODUCTS, because they describe what was DRAWN and the load draws no
 * world at all — read before the first scene frame they would all be zero. Every run restores the
 * declared standpoint and the declared clocks, so what they describe is still the declared scene. */
int SceneRunner::Run() {
  if (!App_.Load()) return 2;
  int rc = 0;
  for (const Scene::Run &r : Scene_.Runs()) {
    rc = Dispatch(r);
    if (rc != 0) break;
  }
  ReportSettled();
  ReportCounters();
  return rc;
}

int SceneRunner::Dispatch(const Scene::Run &run) {
  switch (run.What) {
    case Scene::Run::Kind::Motion: return Motion(run.Motion);
    case Scene::Run::Kind::ClassDump: return DumpClasses(run.ClassDump);
    case Scene::Run::Kind::ClassCompare: return CompareClasses();
    case Scene::Run::Kind::WindProbe: return ProbeWind(run.WindProbe);
    case Scene::Run::Kind::Subject: return RunSubject();
  }
  return 1;
}

/* THE RECORDING, and a still is its one-frame case. Every delivered frame carries the SAME
 * `settleFrames` frames of temporal history: the accumulator is emptied, `settleFrames - 1` frames
 * are rendered at the run's frame-0 state, and the run's own first render completes the count.
 * Without that emptying the picture carries a history built while the tiles were still arriving, and
 * two runs whose tiles arrived in a different order differ in colour although their depth is
 * bit-identical. */
int SceneRunner::Motion(const Scene::Run::MotionRun &m) {
  Render::Renderer &R = App_.Renderer();
  World::World &W = App_.Scenery();
  /* The origin is where the run STARTS, not what the file says: a snapshot may have moved the eye
   * before any run began, and a channel in metres is measured from the standpoint it moves. */
  const Outshine::Stance base{App_.Lat(), App_.Lon(), App_.YawDeg(), App_.PitchDeg()};
  const double lonPerM = 1.0 / (kMPerDeg * std::cos(base.Lat * kDeg2Rad));
  const bool moves = m.Move.Drives(Target::CameraEastM) || m.Move.Drives(Target::CameraNorthM) ||
                     m.Move.Drives(Target::CameraYawDeg) || m.Move.Drives(Target::CameraPitchDeg);

  const auto apply = [&](double f) {
    if (moves) {
      const double e = m.Move.Drives(Target::CameraEastM) ? m.Move.At(Target::CameraEastM, f) : 0.0;
      const double n = m.Move.Drives(Target::CameraNorthM) ? m.Move.At(Target::CameraNorthM, f) : 0.0;
      const double yaw = m.Move.Drives(Target::CameraYawDeg) ? m.Move.At(Target::CameraYawDeg, f)
                                                             : base.YawDeg;
      const double pitch = m.Move.Drives(Target::CameraPitchDeg)
                               ? m.Move.At(Target::CameraPitchDeg, f)
                               : base.PitchDeg;
      /* The accumulated angle becomes a bearing HERE and nowhere else (core/Keyframes.h). */
      App_.Look({base.Lat + n / kMPerDeg, base.Lon + e * lonPerM, std::fmod(yaw, 360.0), pitch});
    }
    if (m.Move.Drives(Target::CameraFovDeg)) App_.SetFovDeg(m.Move.At(Target::CameraFovDeg, f));
    if (m.Move.Drives(Target::SkyClockS)) App_.SetSkyOffsetS(m.Move.At(Target::SkyClockS, f));
    if (m.Move.Drives(Target::ExposureCompEv))
      App_.SetExposureCompEv(m.Move.At(Target::ExposureCompEv, f));
    App_.SetWindClock(m.Move.Drives(Target::WindClockS) ? m.Move.At(Target::WindClockS, f)
                                                        : Scene_.WindClockS());
  };

  Settled_ = Scene_.SettleFrames() >= 0 ? Scene_.SettleFrames() : R.TemporalSettleFrames();
  apply(0.0);
  R.ResetTemporal();
  for (int f = 1; f < Settled_; f++) {
    App_.Frame();
    Outshine::Pump();
  }
  /* THE SETTLE FRAMES ARE SUBMITTED AND NOT WAITED FOR, so without this the first timed frame pays
   * the whole queue: measured 503.8 ms of which 496.0 was GPU, against a 11.3 ms median over the
   * same 240 frames. That is a number about the encoder, not about the frame. */
  R.SyncGpu();
  Log::Info("run", "settled", {{"frames", Settled_}, {"path", m.Path},
      {"channels", (double)m.Move.ChannelCount()}});

  const bool profile = m.Give == Scene::Run::Product::Profile;
  std::string csv;
  if (profile)
    csv = "frame,timeS,distM,frameMs,worldMs,meshMs,albedoMs,uploadMs,buildingMs,bDecodeMs,"
          "classMs,renderMs,gpuMs,nodes,drawnLeaves,terrainTiles,draws,terrainTris,"
          "buildingVerts,built,evicted,classVramMB,temporalVramMB\n";
  else if (m.Frames > 1 && !Out_.MakeDir(m.Path))
    return 1;

  std::vector<double> ms;
  ms.reserve((size_t)m.Frames);
  for (int f = 0; f < m.Frames; f++) {
    const auto t0 = std::chrono::steady_clock::now();
    apply((double)f);
    if (m.World == Scene::Run::Stream::Streaming)
      /* The virtual clock is the streaming PASS index at 60 Hz — monotonic across the load and the
       * run, which is what the world's 1 Hz counters are read against. */
      App_.Stream((double)App_.Loading().PassCount() * 1000.0 / 60.0);
    const auto t1 = std::chrono::steady_clock::now();
    App_.Frame();
    const auto t2 = std::chrono::steady_clock::now();
    Outshine::Pump();
    if (!profile) {
      const std::string name = m.Frames == 1 ? m.Path : FrameName(m.Path, f, "png");
      if (!WritePng(name)) return 1;
      if (!m.Depth.empty() &&
          !WriteDepth(m.Frames == 1 ? m.Depth : FrameName(m.Depth, f, "f32")))
        return 1;
      continue;
    }
    R.SyncGpu();
    const auto t3 = std::chrono::steady_clock::now();
    ms.push_back(MsBetween(t0, t3));
    const double e = m.Move.Drives(Target::CameraEastM) ? m.Move.At(Target::CameraEastM, f) : 0.0;
    const double n = m.Move.Drives(Target::CameraNorthM) ? m.Move.At(Target::CameraNorthM, f) : 0.0;
    char row[512];
    snprintf(row, sizeof row,
             "%d,%.6f,%.3f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,"
             "%d,%d,%d,%d,%ld,%u,%ld,%ld,%.3f,%.3f\n",
             f, (double)f / m.Fps, std::sqrt(e * e + n * n), MsBetween(t0, t3), W.UpdateMs(),
             W.MeshMs(), W.AlbedoMs(), W.UploadMs(), W.BuildingMs(), W.BuildingDecodeMs(),
             W.ClassMs(), MsBetween(t1, t2), MsBetween(t2, t3), W.NodeCount(), W.DrawnLeafCount(),
             R.TerrainVisibleTiles(), R.DrawCount(), R.TerrainTriangleCount(),
             R.BuildingVertexCount(), W.BuiltCount(), W.EvictedCount(),
             (double)R.ClassVramBytes() / (1024.0 * 1024.0),
             (double)R.TemporalVramBytes() / (1024.0 * 1024.0));
    csv += row;
  }

  if (profile) {
    if (!Out_.Text(m.Path, csv)) return 1;
    std::vector<double> sorted = ms;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double v : ms) sum += v;
    /* THE DISTRIBUTION, never the mean alone: a stutter is the tail of a series and a mean cannot
     * see it (CLAUDE.md). The mean rides along only because it prices the whole run. */
    Log::Info("run", "motion", {{"path", m.Path}, {"frames", (double)m.Frames},
        {"fps", m.Fps}, {"meanMs", ms.empty() ? 0.0 : sum / (double)ms.size()},
        {"p50Ms", Percentile(sorted, 0.50)}, {"p95Ms", Percentile(sorted, 0.95)},
        {"p99Ms", Percentile(sorted, 0.99)},
        {"minMs", sorted.empty() ? 0.0 : sorted.front()},
        {"maxMs", sorted.empty() ? 0.0 : sorted.back()},
        {"meshStands", (double)R.TreeMeshStands()},
        {"impostorStands", (double)R.TreeImpostorStands()},
        {"treeTris", (double)R.TreeTriangleCount()},
        {"width", (double)Scene_.RenderResolution().Width},
        {"height", (double)Scene_.RenderResolution().Height}});
  } else {
    Log::Info("run", "stills", {{"path", m.Path}, {"frames", (double)m.Frames},
        {"w", Scene_.RenderResolution().Width}, {"h", Scene_.RenderResolution().Height}});
  }
  apply(0.0);
  if (!moves) App_.Look(base);
  return 0;
}

void SceneRunner::ReportSettled() const {
  Render::Renderer &R = App_.Renderer();
  if (App_.Forest().StandCount() > 0)
    Log::Info("run", "trees_lod", {{"stands", (double)App_.Forest().StandCount()},
        {"meshStands", (double)R.TreeMeshStands()}, {"meshRadiusM", R.TreeMeshRadiusM()},
        {"rank0", (double)R.TreeRankStands(0)}, {"rank1", (double)R.TreeRankStands(1)},
        {"rank2", (double)R.TreeRankStands(2)}, {"rank3", (double)R.TreeRankStands(3)},
        {"impostors", (double)R.TreeImpostorStands()}, {"tris", (double)R.TreeTriangleCount()}});

  /* THE FIELD ITSELF. Nothing below the size of a tree answers to it, so no stage
   * reads it today; it is published because a branch and a rotor owe the anchor. */
  Render::WindField wf;
  wf.SetDeclared(App_.WindDeg(), App_.WindMs());
  Log::Info("run", "wind", {{"declaredFromDeg", App_.WindDeg()}, {"declaredMs", App_.WindMs()},
      {"canopyMs", wf.CanopyMs()},
      {"profileRatio", App_.WindMs() > 0.0 ? wf.CanopyMs() / App_.WindMs() : 0.0},
      {"eigenHz", wf.EigenHz()}, {"waveLenM", wf.WaveLengthM()},
      {"phaseSpeedMs", wf.PhaseSpeedMs()}, {"gustAmp", wf.GustAmplitude()},
      {"dirE", wf.DirEast()}, {"dirN", wf.DirNorth()},
      {"cauchyMean", wf.CauchyAt(wf.CanopyMs(), 0.527)},
      {"tipDegMean", Render::WindField::TipAngleRad(wf.CauchyAt(wf.CanopyMs(), 0.527)) / kDeg2Rad},
      {"clockS", R.GetWindClock()}});

  float irr[Render::IrradianceStage::kFloats] = {};
  if (R.ReadIrradiance(irr)) {
    const double sunY = 0.2126 * irr[0] + 0.7152 * irr[1] + 0.0722 * irr[2];
    const double skyY = 0.2126 * irr[4] + 0.7152 * irr[5] + 0.0722 * irr[6];
    const double sunUp = std::sin((double)App_.SunElDeg() * kPi / 180.0);
    Log::Info("run", "irradiance", {{"sunDirectNormalY", sunY}, {"skyDiffuseHorizY", skyY},
        {"sunElDeg", (double)App_.SunElDeg()}, {"totalHorizY", sunY * sunUp + skyY},
        {"sunRGB", std::to_string(irr[0]) + "," + std::to_string(irr[1]) + "," + std::to_string(irr[2])},
        {"skyRGB", std::to_string(irr[4]) + "," + std::to_string(irr[5]) + "," + std::to_string(irr[6])}});
  }
}

void SceneRunner::ReportCounters() const {
  Render::Renderer &R = App_.Renderer();
  World::World &W = App_.Scenery();
  float met[Render::ExposureStage::kMeterFloats] = {};
  if (R.ReadExposure(met))
    Log::Info("run", "exposure", {{"expScale", (double)met[0]}, {"keyLog2", (double)met[1]},
        {"horizE", (double)met[2]}});
  std::string lvl;
  for (int i = 0; i < Render::TilesStage::kLevelBins; i++)
    if (R.TerrainTrianglesByLevel()[i] > 0)
      lvl += "L" + std::to_string(i) + "=" + std::to_string(R.TerrainTrianglesByLevel()[i]) + " ";
  Log::Info("run", "terrain", {{"cutLevels", lvl}, {"targetTotal", W.TargetTotal()},
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
  if (const std::shared_ptr<const World::ClassStructure> cls = W.Classes().Read())
    Log::Info("run", "class", {{"version", (double)cls->Version()},
        {"edges", (int)cls->Measured().Edges}, {"seeds", (int)cls->Measured().Seeds},
        {"bufferKB", cls->Bytes() / 1024.0}, {"noDataFrac", cls->NoDataFraction()},
        {"unknownKinds", (int)W.Classes().UnknownKinds()},
        {"unknownFeatures", (int)W.Classes().UnknownFeatures()},
        {"seedOverflow", cls->Measured().Overflow}, {"buildMs", W.Classes().MaxBuildMs()},
        {"fineSubmits", (int)W.Classes().FineSubmits()},
        {"coarseSubmits", (int)W.Classes().CoarseSubmits()}});
}

/* THE CLASS AS THE SIMULATION SEES IT: the CPU evaluator over a declared world square, in the class
 * structure's own metric frame. No GPU is involved — this is the answer a headless actor gets, and
 * the picture is judged against it. */
int SceneRunner::DumpClasses(const Scene::Run::ClassDumpRun &d) const {
  const std::shared_ptr<const World::ClassStructure> cls = App_.Scenery().Classes().Read();
  if (!cls) { Log::Error("run", "class_dump_without_structure"); return 1; }
  double ce = 0, cn = 0;
  App_.Scenery().Classes().ToEnu(App_.Lat(), App_.Lon(), &ce, &cn);
  const int n = (int)(d.SpanM / d.StepM);
  /* SNAPPED TO THE WORLD, not to the camera: two runs from different standpoints must sample the
   * same points, or a sub-sample offset would show up as a disagreement that is not one. */
  const double e0 = std::floor((ce - d.SpanM * 0.5) / d.StepM) * d.StepM;
  const double n0 = std::floor((cn - d.SpanM * 0.5) / d.StepM) * d.StepM;
  char hdr[256];
  const int hl = snprintf(hdr, sizeof hdr, "OSCLS1 %d %.6f %.6f %.6f %.6f %.6f\n", n, e0, n0,
                          d.StepM, ce, cn);
  std::vector<uint8_t> blob((size_t)hl + (size_t)n * (size_t)n);
  for (int i = 0; i < hl; i++) blob[(size_t)i] = (uint8_t)hdr[i];
  for (int j = 0; j < n; j++)
    for (int i = 0; i < n; i++) {
      const int c = cls->Evaluate(e0 + ((double)i + 0.5) * d.StepM,
                                  n0 + ((double)j + 0.5) * d.StepM, nullptr, nullptr);
      blob[(size_t)hl + (size_t)j * (size_t)n + (size_t)i] = (uint8_t)(c < 0 ? 255 : c);
    }
  if (!Out_.Bytes(d.Path, blob.data(), blob.size())) return 1;
  Log::Info("run", "class_dumped", {{"path", d.Path}, {"side", n}, {"stepM", d.StepM}});
  return 0;
}

/* CPU AGAINST GPU, on the very pixels that were drawn: one geometry, one predicate, two evaluators
 * (doc/architecture.md). The GPU class index is not read back — it is not expressible through the
 * tone curve — so the picture is PARTITIONED by its own colour and each partition is checked against
 * the CPU answer. A bijection is 0 % disagreement; the bound is the fray, because the fragment
 * refines the boundary and the CPU does not. */
int SceneRunner::CompareClasses() const {
  const int width = Scene_.RenderResolution().Width, height = Scene_.RenderResolution().Height;
  std::vector<uint8_t> viz;
  std::vector<float> depth;
  if (!App_.Renderer().ReadPixels(viz) || !App_.Renderer().ReadDepth(depth)) {
    Log::Error("run", "class_cmp_readback_failed");
    return 1;
  }
  const double tanH = std::tan(0.5 * Scene_.FovDeg() * kDeg2Rad);
  const double aspect = (double)width / (double)height;
  const World::ClassField &field = App_.Scenery().Classes();
  const std::shared_ptr<const World::ClassStructure> cls = field.Read();
  if (!cls) { Log::Error("run", "class_cmp_without_structure"); return 1; }
  const double *O = field.OriginEcef(), *Ea = field.EastEcef(), *No = field.NorthEcef();
  const double *eye = App_.Eye(), *fwd = App_.Fwd(), *right = App_.Right(), *up = App_.Up();
  std::map<uint32_t, std::map<int, long>> hist;
  long ground = 0, edgeNear = 0;
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
      const int c = cls->Evaluate(pe, pn, &dist, nullptr);
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
  Log::Info("run", "class_cmp", {{"groundPx", (double)ground}, {"comparedPx", (double)total},
      {"withinFrayPx", (double)edgeNear}, {"colours", (int)hist.size()},
      {"agreePct", total ? 100.0 * (double)agree / (double)total : 0.0},
      {"solidColours", (int)solidColours}, {"solidPx", (double)solidTotal},
      {"solidAgreePct", solidTotal ? 100.0 * (double)solidAgree / (double)solidTotal : 0.0}});
  return 0;
}

/* THE STATE CHANNEL: the declared field read on a world line along the wind at the
 * declared times — no picture, no GPU. */
int SceneRunner::ProbeWind(const Scene::Run::WindProbeRun &w) const {
  Render::WindField wf;
  wf.SetDeclared(App_.WindDeg(), App_.WindMs());
  std::string csv;
  char line[256];
  snprintf(line, sizeof line,
           "# dxM=%.6f nx=%d dtS=%.9f nt=%d bladeLenM=0.527 canopyMs=%.9f eigenHz=%.9f"
           " waveLenM=%.9f phaseSpeedMs=%.9f\n", w.DxM, w.Samples, w.DtS, w.Frames, wf.CanopyMs(),
           wf.EigenHz(), wf.WaveLengthM(), wf.PhaseSpeedMs());
  csv = line;
  for (int t = 0; t < w.Frames; t++) {
    const double tt = Scene_.WindClockS() + w.DtS * (double)t;
    for (int k = 0; k < w.Samples; k++) {
      /* The line runs ALONG the wind through the scene's own point, in absolute world metres. */
      const double xi = (double)k * w.DxM;
      const double u = wf.SpeedAt(xi * wf.DirEast(), xi * wf.DirNorth(), tt);
      const double th = Render::WindField::TipAngleRad(wf.CauchyAt(u, 0.527)) / kDeg2Rad;
      snprintf(line, sizeof line, "%s%.6f", k ? "," : "", th);
      csv += line;
    }
    csv += "\n";
  }
  if (!Out_.Text(w.Path, csv)) return 1;
  Log::Info("run", "wind_probe_written", {{"path", w.Path}, {"nx", w.Samples}, {"nt", w.Frames},
      {"dxM", w.DxM}, {"dtS", w.DtS}});
  return 0;
}

bool SceneRunner::WritePng(const std::string &name) {
  if (!App_.Renderer().ReadPixels(Rgba_)) {
    Log::Error("run", "readback_failed");
    return false;
  }
  return Out_.Png(name, Rgba_.data(), Scene_.RenderResolution().Width, Scene_.RenderResolution().Height);
}

bool SceneRunner::WriteDepth(const std::string &name) const {
  std::vector<float> d;
  if (!App_.Renderer().ReadDepth(d)) {
    Log::Error("run", "depth_readback_failed");
    return false;
  }
  if (!Out_.Bytes(name, d.data(), d.size() * sizeof(float))) return false;
  Log::Info("run", "depth_written", {{"path", name},
      {"nearM", (double)Render::Renderer::kNearM}, {"fovDeg", Scene_.FovDeg()}});
  return true;
}

/* THE SUBJECT BENCH takes the binary over completely: no World, no tile stream, no scene light. */
int SceneRunner::RunSubject() {
  const Scene::Run::SubjectRun &s = Scene_.Runs().front().Subject;
  SubjectBench bench(App_.Renderer(), App_.Vegetation(), Out_);
  /* Held for the whole bench run: the arrays are uploaded once and the numbers below are logged off
   * the same objects the picture was drawn from. */
  World::TreeMesh mesh;
  World::TreeFoliage foliage;
  if (!s.Species.empty()) {
    const std::string path = std::string(kSpeciesDir) + "/" + s.Species + ".json";
    World::TreeSpecies sp;
    if (!World::Forest::LoadSpecies(path.c_str(), &sp)) {
      Log::Error("run", "subject_species_unreadable", {{"path", path}, {"why", sp.Error()}});
      return 1;
    }
    World::TreeGrower grower;
    const auto t0 = std::chrono::steady_clock::now();
    /* THE SAME MESH THE NEAREST FIELD RANK GETS. Asking for pixel 0 asked for a tube on every 3 mm
     * twig — a mesh nobody draws and one that truncated the crown. */
    grower.Grow(sp, mesh, TreeRank::Pixel(0));
    const auto t1 = std::chrono::steady_clock::now();
    World::TreeLeaf::Build(sp.LeafParams(), mesh);
    const auto t2 = std::chrono::steady_clock::now();
    foliage.Build(mesh, sp, s.LeafMult);
    const auto t3 = std::chrono::steady_clock::now();

    const double h = s.HeightM > 0.0 ? s.HeightM : (double)sp.HeightM();
    const double crownX = (double)(mesh.BoxMax.X - mesh.BoxMin.X) * h;
    const double crownZ = (double)(mesh.BoxMax.Z - mesh.BoxMin.Z) * h;
    const double proj = 0.25 * kPi * crownX * crownZ;
    const bool leaves = s.Leaf == Scene::Run::Foliage::Leaves;

    Render::Renderer &r = App_.Renderer();
    r.SetTreeLook(World::Forest::LookOf(sp));
    r.SetTreeBark(0, mesh.BarkVerts.data(), (uint32_t)mesh.BarkVertexCount(), mesh.BarkIdx.data(),
                  (uint32_t)mesh.BarkIdx.size());
    r.SetTreeLeaf(mesh.LeafVerts.data(), (uint32_t)mesh.LeafVertexCount(), mesh.LeafIdx.data(),
                  (uint32_t)mesh.LeafIdx.size(), foliage.Instances().data(),
                  (uint32_t)foliage.Count(), foliage.ScaleM());
    r.SetTreeLeavesVisible(leaves);
    Log::Info("run", "subject_tree", {{"species", s.Species}, {"heightM", h},
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
        {"leafMult", (double)s.LeafMult}, {"leavesDrawn", leaves ? 1.0 : 0.0}});
    if (!bench.SelectTree(s.Species.c_str(), h)) {
      Log::Error("run", "subject_species_height_invalid", {{"name", s.Species}});
      return 1;
    }
  } else if (!bench.Select(s.Template.c_str(), s.HeightM)) {
    Log::Error("run", "subject_unknown", {{"name", s.Template}});
    return 1;
  }
  /* A TREE DECLARES NO SUBSTRATE. Without a template naming a ground class the floor stays at the
   * bench's 18 % neutral, which is what a subject without a declared substrate deserves. */
  if (!s.Template.empty()) {
    std::string subName;
    float subRgb[3] = {0, 0, 0};
    if (!SubjectSubstrate(App_.Materials(), App_.Files().Vegetation, s.Template, &subName, subRgb)) {
      Log::Error("run", "subject_substrate_unresolved", {{"template", s.Template}});
      return 1;
    }
    bench.SetSubstrate(subName, subRgb);
  }
  Out_.MakeDir(s.Dir);
  bench.Stand(Scene_.Lat(), Scene_.Lon(), kSubjectGroundAslM);
  bench.SetOutDir(s.Dir);
  bench.SetTurntableSteps(s.TurnSteps);
  return bench.Run() ? 0 : 1;
}

}  // namespace outshine::Clients
