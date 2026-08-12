/* The plant generator's measuring bench: grows every declaration under the species directory, prints
 * counts, box, timings and memory as CSV, and can dump the raw buffers so the numbers can be diffed
 * against the reference implementation vertex by vertex. It draws nothing — the first rendered tree
 * is gpu_walk --rig, one layer on. */
#include <dirent.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "LeafAngleDistribution.h"
#include "TreeFoliage.h"
#include "TreeGrower.h"
#include "TreeLeaf.h"
#include "TreeMesh.h"
#include "TreeSpecies.h"

using namespace outshine::Generators;

namespace {

/* THE DECLARATIONS ARE WHAT IS ON DISK. A hand-written list here measured sixteen of them and
 * silently skipped every plant added since — which is how a form nobody grew would look green. */
std::vector<std::string> PlantsIn(const std::string &dir) {
  std::vector<std::string> out;
  DIR *d = opendir(dir.c_str());
  if (!d) { return out; }
  for (const dirent *e; (e = readdir(d)) != nullptr;) {
    const std::string name = e->d_name;
    if (name.size() > 5 && name.compare(name.size() - 5, 5, ".json") == 0) {
      out.push_back(name.substr(0, name.size() - 5));
    }
  }
  closedir(d);
  std::sort(out.begin(), out.end());
  return out;
}

[[nodiscard]] bool ReadFile(const std::string &path, std::string &out) {
  FILE *f = fopen(path.c_str(), "rb");
  if (!f) { return false; }
  fseek(f, 0, SEEK_END);
  const long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n <= 0) {
    fclose(f);
    return false;
  }
  out.resize((size_t)n);
  const size_t got = fread(&out[0], 1, (size_t)n, f);
  fclose(f);
  return got == (size_t)n;
}

void Dump(const std::string &path, const void *p, size_t bytes) {
  FILE *f = fopen(path.c_str(), "wb");
  if (!f) { return; }
  if (bytes > 0) { fwrite(p, 1, bytes, f); }
  fclose(f);
}

double Millis(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) {
  return std::chrono::duration<double, std::milli>(b - a).count();
}

} // namespace

int main(int argc, char **argv) {
  std::string assets = "assets/world/species";
  std::string dumpDir;
  int reps = 1;
  bool angles = false;
  float pixel = 0.0f;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--assets") && i + 1 < argc) {
      assets = argv[++i];
    } else if (!strcmp(argv[i], "--pixel") && i + 1 < argc) {
      pixel = (float)atof(argv[++i]);
    } else if (!strcmp(argv[i], "--dump") && i + 1 < argc) {
      dumpDir = argv[++i];
    } else if (!strcmp(argv[i], "--reps") && i + 1 < argc) {
      reps = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--angles")) {
      angles = true;
    }
  }
  if (reps < 1) { reps = 1; }

  const std::vector<std::string> kSpecies = PlantsIn(assets);
  if (kSpecies.empty()) {
    fprintf(stderr, "treebench: no plant declaration under %s\n", assets.c_str());
    return 1;
  }

  TreeGrower grower;
  TreeMesh mesh;
  printf("species,bark_v,bark_i,leaf_v,leaf_i,leaf_pts,bbmin_x,bbmin_y,bbmin_z,bbmax_x,bbmax_y,"
         "bbmax_z,grow_ms,leaf_ms,bark_kb,leaf_kb,pts_kb,g_nadir,g0,g1,gp,g_resid,stalk_el_deg,"
         "angle_ms,foot_d_cm,dbh_cm,dbh_want_cm,hd,passes,laminae,leaf_m2,crown_m2,lai,lai_want,per_pt\n");
  for (size_t i = 0; i < kSpecies.size(); ++i) {
    std::string text;
    const std::string path = assets + "/" + kSpecies[i] + ".json";
    if (!ReadFile(path, text)) {
      fprintf(stderr, "cannot read %s\n", path.c_str());
      return 1;
    }
    TreeSpecies sp;
    if (!sp.Parse(text.c_str(), text.size())) {
      fprintf(stderr, "%s: %s\n", path.c_str(), sp.Error().c_str());
      return 1;
    }

    double growMs = 1e30, leafMs = 1e30;
    for (int r = 0; r < reps; ++r) {
      const auto t0 = std::chrono::steady_clock::now();
      grower.Grow(sp, mesh, pixel);
      const auto t1 = std::chrono::steady_clock::now();
      TreeLeaf::Build(sp.LeafParams(), mesh);
      const auto t2 = std::chrono::steady_clock::now();
      if (Millis(t0, t1) < growMs) { growMs = Millis(t0, t1); }
      if (Millis(t1, t2) < leafMs) { leafMs = Millis(t1, t2); }
    }

    LeafAngleDistribution lad;
    const auto a0 = std::chrono::steady_clock::now();
    lad.Measure(mesh);
    const auto a1 = std::chrono::steady_clock::now();

    TreeFoliage foliage;
    foliage.Build(mesh, sp, 1);
    const double crownM2 = foliage.CrownProjM2();

    const double dbhCm = 2.0 * (double)mesh.DbhRadius * (double)sp.HeightM() * 100.0;
    printf("%s,%zu,%zu,%zu,%zu,%zu,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.4f,%.4f,%.1f,%.1f,%.1f,"
           "%.4f,%.4f,%.4f,%.4f,%.5f,%.2f,%.2f,%.2f,%.2f,%.2f,%.3f,%d,%zu,%.1f,%.1f,%.3f,%.3f,%.3f\n",
           kSpecies[i].c_str(), mesh.BarkVertexCount(), mesh.BarkIdx.size(), mesh.LeafVertexCount(),
           mesh.LeafIdx.size(), mesh.LeafPoints.size(), (double)mesh.BoxMin.X, (double)mesh.BoxMin.Y,
           (double)mesh.BoxMin.Z, (double)mesh.BoxMax.X, (double)mesh.BoxMax.Y, (double)mesh.BoxMax.Z,
           growMs, leafMs,
           (double)(mesh.BarkVerts.size() * 4 + mesh.BarkIdx.size() * 4) / 1024.0,
           (double)(mesh.LeafVerts.size() * 4 + mesh.LeafIdx.size() * 4) / 1024.0,
           (double)(mesh.LeafPoints.size() * sizeof(TreeMesh::LeafPoint)) / 1024.0,
           (double)lad.Sampled(90), (double)lad.G0(), (double)lad.G1(), (double)lad.Gp(),
           (double)lad.MaxResidual(), (double)lad.MeanStalkElevationDeg(), Millis(a0, a1),
           2.0 * (double)mesh.FootRadius * (double)sp.HeightM() * 100.0, dbhCm,
           (double)sp.DbhM() * 100.0, dbhCm > 0.0 ? (double)sp.HeightM() / dbhCm : -1.0,
           grower.Passes(), foliage.Count(), foliage.LeafAreaM2(), crownM2,
           crownM2 > 0.0 ? foliage.LeafAreaM2() / crownM2 : -1.0, (double)sp.Lai(),
           foliage.PerPoint());

    if (angles) {
      fprintf(stderr, "# %s G(el) per degree\n", kSpecies[i].c_str());
      for (int d = 0; d <= 90; ++d) { fprintf(stderr, "G %s %d %.6f\n", kSpecies[i].c_str(), d, (double)lad.Sampled(d)); }
      const auto &h = lad.StalkHistogram();
      for (size_t b = 0; b < h.size(); ++b) {
        fprintf(stderr, "H %s %d %d %.6f\n", kSpecies[i].c_str(), (int)b * 5, (int)b * 5 + 5, (double)h[b]);
      }
    }

    if (!dumpDir.empty()) {
      const std::string base = dumpDir + "/" + kSpecies[i].c_str();
      Dump(base + ".barkv.bin", mesh.BarkVerts.data(), mesh.BarkVerts.size() * 4);
      Dump(base + ".barki.bin", mesh.BarkIdx.data(), mesh.BarkIdx.size() * 4);
      Dump(base + ".leafv.bin", mesh.LeafVerts.data(), mesh.LeafVerts.size() * 4);
      Dump(base + ".leafi.bin", mesh.LeafIdx.data(), mesh.LeafIdx.size() * 4);
      Dump(base + ".pts.bin", mesh.LeafPoints.data(), mesh.LeafPoints.size() * sizeof(TreeMesh::LeafPoint));
    }
  }
  return 0;
}
