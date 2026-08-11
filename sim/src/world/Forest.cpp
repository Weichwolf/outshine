#include "Forest.h"

#include <cmath>
#include <cstdio>

#include "ClassField.h"
#include "GroundSample.h"
#include "Log.h"
#include "TerrainLoader.h"
#include "TreeFoliage.h"
#include "TreeGrower.h"
#include "TreeLeaf.h"
#include "TreeMesh.h"
#include "TreeRanks.h"
#include "TreeSpecies.h"
#include "VegetationTemplates.h"

namespace outshine::World {
namespace {

/* THE LAMINA'S BASE REFLECTANCE, and it is not a new number: it is the vegetation table's own
 * `mixed_broadleaf` fresh blade, sRGB (74, 92, 46) linearised — the green the ground shader already
 * draws a broadleaf stand in. A species' declared `leaf_r/g/b` multiplies it, which is what makes
 * that triple a TINT (spruce 0.40/0.74/0.55 is darker and bluer than beech, not a colour of its own). */
const float kLeafBaseLinear[3] = {0.0684f, 0.1072f, 0.0273f};

float SrgbToLinear(float v) {
  return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

struct GroundCtx { const ClassField *Cls; };

GroundSample GroundAtEnu(void *user, double e, double n) {
  double lat = 0.0, lon = 0.0;
  ((GroundCtx *)user)->Cls->FromEnu(e, n, &lat, &lon);
  return fb_stream_ground(lat, lon);
}

}  // namespace

bool Forest::LoadSpecies(const char *path, TreeSpecies *out) {
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  std::string text;
  char buf[8192];
  for (size_t n; (n = fread(buf, 1, sizeof buf, f)) > 0;) text.append(buf, n);
  fclose(f);
  return !text.empty() && out->Parse(text.c_str(), text.size());
}

TreeLook Forest::LookOf(const TreeSpecies &sp) {
  TreeLook look;
  const TreeSpecies::Shading &sh = sp.ShadingParams();
  const TreeSpecies::Leaf &lf = sp.LeafParams();
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
  look.NeedleWidth = lf.Kind == TreeSpecies::LeafKind::Needle ? lf.NeedleWidth : 0.0f;
  return look;
}

std::optional<Forest::Prototype> Forest::Grow(const char *speciesPath) {
  TreeSpecies sp;
  if (!LoadSpecies(speciesPath, &sp)) {
    Log::Error("forest", "species_unreadable", {{"path", std::string(speciesPath)}});
    return std::nullopt;
  }
  const double h = (double)sp.HeightM();
  TreeMesh mesh;
  TreeGrower g;
  TreeFoliage foliage;
  Prototype proto;
  proto.HeightM = h;
  proto.Look = LookOf(sp);
  proto.Ranks.resize((size_t)TreeRank::kCount);
  double crownProjM2 = 0.0;
  /* ONE MESH PER RANK, and the ladder is the stage's own: rank k may be one pixel coarse at its
   * nearest stand. THE NEAREST RANK PAYS THE INDEX IN COUNT — one card per kLeavesPerCard grown
   * laminae, each drawing the species' own leaf. Only the ranks above it thin, by four per step so a
   * card keeps its size on screen, and `CardLeafM` regrows the leaf they draw so the declared index
   * survives the thinning. */
  for (int rank = 0; rank < TreeRank::kCount; ++rank) {
    Prototype::Rank &out = proto.Ranks[(size_t)rank];
    g.Grow(sp, mesh, TreeRank::Pixel(rank));
    TreeLeaf::Build(sp.LeafParams(), mesh);
    foliage.Build(mesh, sp, 1);
    const uint32_t stride = (uint32_t)kLeavesPerCard << (2u * (unsigned)rank);
    for (size_t i = 0; i < foliage.Count(); i += stride) {
      const float *c = &foliage.Instances()[i * TreeFoliage::kFloats];
      out.Cards.insert(out.Cards.end(), c, c + TreeFoliage::kFloats);
    }
    const uint32_t nCards = (uint32_t)(out.Cards.size() / TreeFoliage::kFloats);
    /* THE CROWN'S PROJECTION IS THE SPECIES', not the rank's. A coarse rank drops the thin shoots
     * that reach furthest out, so its own bark box is smaller — sizing the leaf by that box would
     * shrink the canopy exactly where it is already thinnest. */
    if (rank == 0) crownProjM2 = foliage.CrownProjM2();
    out.CardCount = nCards;
    out.CardLeafM = foliage.CardLeafM(kLeavesPerCard, nCards, (double)sp.Lai(), crownProjM2);
    out.BarkVerts = mesh.BarkVerts;
    out.BarkVertCount = (uint32_t)mesh.BarkVertexCount();
    out.BarkIdx = mesh.BarkIdx;
    Log::Info("forest", "trees_grown", {{"rank", (double)rank},
        {"pixelHeightFrac", (double)TreeRank::Pixel(rank)},
        {"barkVerts", (int)mesh.BarkVertexCount()},
        {"barkTris", (double)(mesh.BarkIdx.size() / 3)},
        {"heightM", h}, {"dbhCm", 200.0 * (double)mesh.DbhRadius * h},
        {"cards", (double)nCards},
        {"leavesPerCard", (double)kLeavesPerCard},
        {"cardLeafM", (double)out.CardLeafM}, {"declaredLeafM", (double)foliage.ScaleM()},
        {"crownProjM2", crownProjM2}, {"lai", (double)sp.Lai()},
        {"leafPoints", (double)mesh.LeafPoints.size()},
        {"laminae", (double)foliage.Count()}, {"perPoint", foliage.PerPoint()},
        {"growHeight", (double)g.GrowHeight()}});
    if (rank == 0) {
      Crown_.HalfWidth = std::fmax(std::fmax(-mesh.BoxMin.X, mesh.BoxMax.X),
                                   std::fmax(-mesh.BoxMin.Z, mesh.BoxMax.Z));
      Crown_.Bottom = mesh.BoxMin.Y;
      Crown_.Top = mesh.BoxMax.Y;
      Crown_.HeightM = (float)h;
    }
  }
  proto.Crown = Crown_;
  HeightSigma_ = sp.HeightSigma();
  Grown_ = true;
  return proto;
}

void Forest::Scatter(const ClassField &cls, const VegetationTemplates &veg, double camLat,
                     double camLon, double eyeAglM) {
  if (!Grown() || Scattered_) return;
  double east = 0.0, north = 0.0;
  cls.Project(camLat, camLon, &east, &north);
  GroundCtx gc{&cls};
  double gcam = 0.0;
  if (!fb_stream_ground(camLat, camLon).TryAslM(&gcam)) return;
  /* THE EYE POINT, not the ground under it: the shader hangs the foot straight off the suspension at
   * the eye, so eye height has to be subtracted here already. */
  /* ONE HANDLE FOR THE WHOLE SCATTER: taken here, held to the last sample. The builder may publish a
   * newer structure meanwhile and this one stays exactly as it was read. */
  const std::shared_ptr<const ClassStructure> structure = cls.Read();
  if (!structure) return;
  const TreeField::GroundOracle oracle{GroundAtEnu, &gc, fb_stream_ground_post_m(camLat)};
  Field_.Scatter(*structure, veg, kScatterRadiusM, east, north, gcam + eyeAglM, camLat, HeightSigma_,
                 Crown_, oracle, Stands_, Dist_);
  Scattered_ = true;

  constexpr int kSF = TreeField::kStandFloats;
  double lo[kSF], hi[kSF], sum = 0.0;
  for (int c = 0; c < kSF; c++) { lo[c] = 1e30; hi[c] = -1e30; }
  for (size_t s = 0; s + (size_t)kSF <= Stands_.size(); s += (size_t)kSF)
    for (int c = 0; c < kSF; c++) {
      const double v = Stands_[s + (size_t)c];
      if (v < lo[c]) lo[c] = v;
      if (v > hi[c]) hi[c] = v;
      if (c == 4) sum += v;
    }
  const long n = StandCount();
  if (Field_.Cells(TreeField::Refusal::InCrown) > 0)
    Log::Info("forest", "stand_cleared",
        {{"trees", (double)Field_.Cells(TreeField::Refusal::InCrown)},
        {"crownHalfM", (double)(Crown_.HalfWidth * Crown_.HeightM)},
        {"crownBottomM", (double)(Crown_.Bottom * Crown_.HeightM)},
        {"crownTopM", (double)(Crown_.Top * Crown_.HeightM)}});
  /* THE WHOLE PARTITION IN ONE LINE: every cell of the disc appears under exactly one name, so a
   * count that does not sum to the disc is a case nobody wrote — and a tree-line finding can be
   * attributed instead of guessed. */
  std::vector<LogField> cells{{"speciesLimitAslM", veg.Limit().SpeciesLimitM(camLat)},
      {"highestStandAslM", Field_.HighestStandAslM()}};
  for (int k = 0; k < (int)TreeField::Refusal::Count; k++)
    cells.push_back({TreeField::RefusalName((TreeField::Refusal)k),
                     (double)Field_.Cells((TreeField::Refusal)k)});
  Log::Info("forest", "treeline", cells);
  Log::Debug("forest", "trees", {{"stands", (int)n},
      {"eastM", lo[0]}, {"eastMax", hi[0]}, {"northM", lo[1]}, {"northMax", hi[1]},
      {"footM", lo[2]}, {"footMax", hi[2]},
      {"scaleMin", lo[4]}, {"scaleMax", hi[4]}, {"scaleMean", n ? sum / (double)n : 0.0}});
}

} // namespace outshine::World
