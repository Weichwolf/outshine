/* THE DECIDABLE CLASS, of which this tree had nothing: a geometric invariant needs no reference and no
 * device, and it is true or false. TreeGrower declares the bark mesh "watertight and connected by
 * construction" — that is a claim about every declaration under src/assets/world/species/, and until now
 * nothing retook it. Every species is grown at full detail and its bark is held to five invariants:
 * indices in range, no degenerate triangle, unit normals, every edge used exactly twice in opposite
 * directions, and a positive signed volume.
 *
 * THE DECLARATIONS ARE WHAT IS ON DISK. A hand-written list here would measure the species it was
 * written for and silently skip every plant added since, which is how a form nobody grew looks green.
 * The working directory is the repository root; the harness runs it there. */
#include <dirent.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "Check.h"
#include "GrowthForm.h"
#include "TreeGrower.h"
#include "TreeMesh.h"
#include "TreeSpecies.h"

using namespace outshine::Generators;

namespace {

const char *const kSpeciesDir = "src/assets/world/species";

/* What one bark mesh answered. Metres are absent on purpose: the mesh is delivered normalised, so
 * every length here is a fraction of the tree's own height and every area a fraction of its square. */
struct BarkVerdict {
  size_t Triangles = 0;
  size_t IndexOutOfRange = 0;
  size_t RepeatedIndex = 0;
  size_t ZeroArea = 0;
  size_t EdgeNotUsedTwice = 0;
  size_t EdgeNotOpposed = 0;
  double WorstNormalOff = 0.0;    /* |n| - 1 */
  double SmallestArea = 1.0;      /* fraction of height^2 */
  double SignedVolume = 0.0;      /* fraction of height^3, positive if wound outward */
  double LowestY = 0.0;           /* fraction of height, 0 if the base plane is the floor */
  size_t LooseIndices = 0;        /* indices past the last whole triangle */
  double DeclaredExtent = 0.0;    /* the mesh's extent along the axis height_m is measured on */
  long EulerCharacteristic = 0;
};

std::vector<std::string> DeclaredSpecies() {
  std::vector<std::string> out;
  DIR *dir = opendir(kSpeciesDir);
  if (dir == nullptr) { return out; }
  for (const dirent *e; (e = readdir(dir)) != nullptr;) {
    const std::string name = e->d_name;
    if (name.size() > 5 && name.compare(name.size() - 5, 5, ".json") == 0) {
      out.push_back(name.substr(0, name.size() - 5));
    }
  }
  closedir(dir);
  std::sort(out.begin(), out.end());
  return out;
}

[[nodiscard]] bool ReadFile(const std::string &path, std::string &out) {
  FILE *f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) { return false; }
  std::fseek(f, 0, SEEK_END);
  const long bytes = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (bytes <= 0) {
    std::fclose(f);
    return false;
  }
  out.resize((size_t)bytes);
  const size_t got = std::fread(&out[0], 1, (size_t)bytes, f);
  std::fclose(f);
  return got == (size_t)bytes;
}

/* Undirected key, with the traversal direction kept apart: a closed orientable surface uses every
 * edge exactly twice and in opposite directions, and the two failures are different defects — a hole
 * versus a flipped triangle. */
uint64_t EdgeKey(uint32_t a, uint32_t b) {
  return a < b ? ((uint64_t)a << 32 | b) : ((uint64_t)b << 32 | a);
}

void MeasureVertices(const TreeMesh &mesh, BarkVerdict &v) {
  const size_t count = mesh.BarkVertexCount();
  for (size_t i = 0; i < count; ++i) {
    const float *p = &mesh.BarkVerts[i * TreeMesh::kBarkFloats];
    const double length = std::sqrt((double)p[3] * p[3] + (double)p[4] * p[4] + (double)p[5] * p[5]);
    v.WorstNormalOff = std::max(v.WorstNormalOff, std::fabs(length - 1.0));
    v.LowestY = std::min(v.LowestY, (double)p[1]);
  }
}

/* THE OTHER HALF OF TreeMesh.h's CONTRACT — "base at y = 0, height exactly 1". The extent is taken
 * over the box, which the grower normalises by, and not over the bark: the finest shoots are leaf
 * points at a coarse rank, so the topmost bark vertex is below the top of the plant. A lying form is
 * measured along its own run, because `height_m` is its LENGTH. */
double DeclaredExtent(const TreeSpecies &declaration, const TreeMesh &mesh) {
  if (GrowthForm::Lying(declaration.Form().Arch)) {
    return std::max((double)mesh.BoxMax.X - mesh.BoxMin.X, (double)mesh.BoxMax.Z - mesh.BoxMin.Z);
  }
  return (double)mesh.BoxMax.Y;
}

void MeasureTriangles(const TreeMesh &mesh, BarkVerdict &v) {
  const size_t vertices = mesh.BarkVertexCount();
  std::unordered_map<uint64_t, int> uses, directions;
  uses.reserve(mesh.BarkIdx.size());
  directions.reserve(mesh.BarkIdx.size());
  for (size_t i = 0; i + 2 < mesh.BarkIdx.size(); i += 3) {
    const uint32_t t[3] = {mesh.BarkIdx[i], mesh.BarkIdx[i + 1], mesh.BarkIdx[i + 2]};
    if (t[0] >= vertices || t[1] >= vertices || t[2] >= vertices) {
      ++v.IndexOutOfRange;
      continue;
    }
    if (t[0] == t[1] || t[1] == t[2] || t[0] == t[2]) {
      ++v.RepeatedIndex;
      continue;
    }
    const float *a = &mesh.BarkVerts[t[0] * TreeMesh::kBarkFloats];
    const float *b = &mesh.BarkVerts[t[1] * TreeMesh::kBarkFloats];
    const float *c = &mesh.BarkVerts[t[2] * TreeMesh::kBarkFloats];
    const double u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const double w[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    const double n[3] = {u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2],
                         u[0] * w[1] - u[1] * w[0]};
    const double area = 0.5 * std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (area <= 0.0) { ++v.ZeroArea; }
    v.SmallestArea = std::min(v.SmallestArea, area);
    /* The divergence theorem over the closed surface: one number that catches a globally inverted
     * mesh, where a per-triangle test cannot. */
    v.SignedVolume += ((double)a[0] * ((double)b[1] * c[2] - (double)b[2] * c[1]) -
                       (double)a[1] * ((double)b[0] * c[2] - (double)b[2] * c[0]) +
                       (double)a[2] * ((double)b[0] * c[1] - (double)b[1] * c[0])) /
                      6.0;
    for (int e = 0; e < 3; ++e) {
      const uint64_t key = EdgeKey(t[e], t[(e + 1) % 3]);
      ++uses[key];
      directions[key] += t[e] < t[(e + 1) % 3] ? 1 : -1;
    }
  }
  for (const auto &used : uses) {
    if (used.second != 2) { ++v.EdgeNotUsedTwice; }
  }
  for (const auto &direction : directions) {
    if (direction.second != 0) { ++v.EdgeNotOpposed; }
  }
  /* An index buffer whose length is not a multiple of three has a tail no triangle can use, and the
   * loop above steps over it in silence; counting it is what turns that into a verdict. */
  v.LooseIndices = mesh.BarkIdx.size() % 3;
  v.Triangles = mesh.BarkIdx.size() / 3;
  v.EulerCharacteristic = (long)vertices - (long)uses.size() + (long)v.Triangles;
}

/* How many declarations broke each invariant, and the extremes that make a passing run worth reading.
 * Counted per species and checked once at the end, so one bad declaration names itself instead of
 * ending the sweep. */
struct Sweep {
  size_t Declarations = 0;
  size_t Unreadable = 0, BadIndices = 0, Degenerate = 0, BadNormals = 0;
  size_t Unclosed = 0, MisWound = 0, Inverted = 0;
  size_t Ragged = 0, MisScaled = 0;
  size_t Triangles = 0;
  double WorstNormalOff = 0.0;
  double WorstExtentOff = 0.0;
  double SmallestArea = 1.0;
  double LowestY = 0.0;
  std::string SmallestIn, LowestIn, WorstExtentIn;
  long LowestEuler = 0, HighestEuler = 0;
};

void ReportOffender(const std::string &species, const char *what, double value) {
  const std::string line = species + ": " + what;
  outshine::Test::Note(line.c_str(), value, "count");
}

[[nodiscard]] bool GrowDeclared(const std::string &name, TreeGrower &grower, TreeMesh &mesh,
                                TreeSpecies &declaration) {
  std::string text;
  if (!ReadFile(std::string(kSpeciesDir) + "/" + name + ".json", text)) {
    ReportOffender(name, "declaration could not be read", 0.0);
    return false;
  }
  if (!declaration.Parse(text.c_str(), text.size())) {
    ReportOffender(name, "declaration did not parse", 0.0);
    return false;
  }
  grower.Grow(declaration, mesh, 0.0f);
  return true;
}

void Judge(const std::string &name, const BarkVerdict &v, Sweep &sweep) {
  if (v.IndexOutOfRange > 0) {
    ReportOffender(name, "indices past the end of the vertex buffer", (double)v.IndexOutOfRange);
    ++sweep.BadIndices;
  }
  if (v.RepeatedIndex > 0 || v.ZeroArea > 0) {
    ReportOffender(name, "degenerate triangles", (double)(v.RepeatedIndex + v.ZeroArea));
    ++sweep.Degenerate;
  }
  if (v.WorstNormalOff > 1e-4) {
    ReportOffender(name, "worst normal length off by more than 1e-4", v.WorstNormalOff);
    ++sweep.BadNormals;
  }
  if (v.EdgeNotUsedTwice > 0) {
    ReportOffender(name, "edges not used by exactly two triangles", (double)v.EdgeNotUsedTwice);
    ++sweep.Unclosed;
  }
  if (v.EdgeNotOpposed > 0) {
    ReportOffender(name, "edges traversed the same way twice", (double)v.EdgeNotOpposed);
    ++sweep.MisWound;
  }
  if (!(v.SignedVolume > 0.0)) {
    ReportOffender(name, "signed volume, which must be positive", v.SignedVolume);
    ++sweep.Inverted;
  }
  if (v.LooseIndices > 0) {
    ReportOffender(name, "indices past the last whole triangle", (double)v.LooseIndices);
    ++sweep.Ragged;
  }
  if (std::fabs(v.DeclaredExtent - 1.0) > 1e-5) {
    ReportOffender(name, "extent along the axis height_m is measured on, which must be 1",
                   v.DeclaredExtent);
    ++sweep.MisScaled;
  }
}

void Accumulate(const std::string &name, const BarkVerdict &v, Sweep &sweep) {
  ++sweep.Declarations;
  sweep.Triangles += v.Triangles;
  sweep.WorstNormalOff = std::max(sweep.WorstNormalOff, v.WorstNormalOff);
  if (std::fabs(v.DeclaredExtent - 1.0) > sweep.WorstExtentOff) {
    sweep.WorstExtentOff = std::fabs(v.DeclaredExtent - 1.0);
    sweep.WorstExtentIn = name;
  }
  if (v.SmallestArea < sweep.SmallestArea) {
    sweep.SmallestArea = v.SmallestArea;
    sweep.SmallestIn = name;
  }
  if (v.LowestY < sweep.LowestY) {
    sweep.LowestY = v.LowestY;
    sweep.LowestIn = name;
  }
  sweep.LowestEuler = sweep.Declarations == 1 ? v.EulerCharacteristic
                                              : std::min(sweep.LowestEuler, v.EulerCharacteristic);
  sweep.HighestEuler = std::max(sweep.HighestEuler, v.EulerCharacteristic);
}

void Publish(const Sweep &sweep) {
  using outshine::Test::Note;
  Note("species grown at full detail", (double)sweep.Declarations, "declarations");
  Note("bark triangles over all declarations", (double)sweep.Triangles, "triangles");
  Note("worst normal length deviation", sweep.WorstNormalOff, "of 1");
  Note(("worst unit-height deviation, in " + sweep.WorstExtentIn).c_str(), sweep.WorstExtentOff,
       "of 1");
  Note(("smallest triangle area, and it is not zero, in " + sweep.SmallestIn).c_str(),
       sweep.SmallestArea, "of height^2");
  /* chi = 2C - 2G over a closed orientable surface: a multi-stemmed shrub is several closed shells in
   * one buffer, so this is the number that says how many, not a failure. */
  Note("Euler characteristic, lowest over the declarations", (double)sweep.LowestEuler, "2C - 2G");
  Note("Euler characteristic, highest over the declarations", (double)sweep.HighestEuler, "2C - 2G");
  /* NOT A CHECK, and deliberately so: TreeMesh.h declares "base at y = 0" and seven declarations put
   * geometry below it. Which of the two is wrong is not decided here (doc/bugs.md). */
  const std::string lowest =
      sweep.LowestIn.empty() ? std::string("no bark vertex below the declared base plane")
                             : "lowest bark vertex below the declared base plane, in " + sweep.LowestIn;
  Note(lowest.c_str(), sweep.LowestY, "of height");
}

} // namespace

int main() {
  outshine::Test::Covers("I.20 Every index in range and no degenerate triangle — the bark mesh, "
                         "every declared species");
  outshine::Test::Covers("I.20 Every normal unit to 1e-4 — the bark mesh, every declared species");
  outshine::Test::Covers("I.20 Consistent outward winding by the divergence-theorem volume — the "
                         "bark mesh, every declared species");
  outshine::Test::Covers("I.20 Closed — the bark mesh, every declared species");
  outshine::Test::Covers("I.20 Delivered normalised at unit height — every declared species");

  const std::vector<std::string> species = DeclaredSpecies();
  CHECK(!species.empty(), "there is a plant declaration under src/assets/world/species to grow");

  TreeGrower grower;
  TreeMesh mesh;
  Sweep sweep;
  for (const std::string &name : species) {
    TreeSpecies declaration;
    if (!GrowDeclared(name, grower, mesh, declaration)) {
      ++sweep.Unreadable;
      continue;
    }
    BarkVerdict verdict;
    MeasureVertices(mesh, verdict);
    MeasureTriangles(mesh, verdict);
    verdict.DeclaredExtent = DeclaredExtent(declaration, mesh);
    Judge(name, verdict, sweep);
    Accumulate(name, verdict, sweep);
  }

  CHECK(sweep.Unreadable == 0, "every plant declaration on disk could be read and grown");
  CHECK(sweep.BadIndices == 0, "every bark index points at a vertex that exists");
  CHECK(sweep.Degenerate == 0, "no bark triangle repeats an index or encloses no area");
  CHECK(sweep.BadNormals == 0, "every bark normal is unit to 1e-4");
  CHECK(sweep.Unclosed == 0, "every bark edge is used by exactly two triangles — the mesh is closed");
  CHECK(sweep.MisWound == 0, "every bark edge is traversed once each way — the winding is consistent");
  CHECK(sweep.Inverted == 0, "every bark mesh has positive signed volume — it is wound outward");
  CHECK(sweep.Ragged == 0, "every bark index buffer is a whole number of triangles");
  CHECK(sweep.MisScaled == 0,
        "every mesh is delivered at extent 1 along the axis its height is measured on");
  Publish(sweep);

  return outshine::Test::Report();
}
