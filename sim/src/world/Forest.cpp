#include "Forest.h"

#include <cmath>
#include <cstdio>

#include "ClassField.h"
#include "ElevationProvider.h"
#include "Log.h"
#include "Renderer.h"
#include "TerrainLoader.h"
#include "TreeFoliage.h"
#include "TreeGrower.h"
#include "TreeLeaf.h"
#include "TreeSpecies.h"
#include "VegetationTemplates.h"

namespace outshine::World {
namespace {

/* THE LAMINA'S BASE REFLECTANCE, and it is not a new number: it is the vegetation table's own
 * `laubmischwald` fresh blade, sRGB (74, 92, 46) linearised — the green the ground shader already
 * draws a broadleaf stand in. A species' declared `leaf_r/g/b` multiplies it, which is what makes
 * that triple a TINT (spruce 0.40/0.74/0.55 is darker and bluer than beech, not a colour of its own). */
const float kLeafBaseLinear[3] = {0.0684f, 0.1072f, 0.0273f};

float SrgbToLinear(float v) {
  return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

struct GroundCtx { const ClassField *Cls; };

double GroundAtEnu(void *user, double e, double n) {
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

Render::TreeLook Forest::LookOf(const TreeSpecies &sp) {
  Render::TreeLook look;
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

bool Forest::Grow(Render::Renderer &r, const char *speciesPath) {
  TreeSpecies sp;
  if (!LoadSpecies(speciesPath, &sp)) {
    Log::Error("forest", "species_unreadable", {{"path", std::string(speciesPath)}});
    return false;
  }
  const double h = (double)sp.HeightM();
  TreeGrower g;
  TreeFoliage foliage;
  std::vector<float> rankCards;
  double crownProjM2 = 0.0;
  /* ONE MESH PER RANK, and the ladder is the stage's own: rank k may be one pixel coarse at its
   * nearest stand. THE NEAREST RANK PAYS THE INDEX IN COUNT — one card per kLeavesPerCard grown
   * laminae, each drawing the species' own leaf. Only the ranks above it thin, by four per step so a
   * card keeps its size on screen, and `CardLeafM` regrows the leaf they draw so the declared index
   * survives the thinning. */
  for (int rank = 0; rank < Render::TreeStage::kRanks; ++rank) {
    g.Grow(sp, Mesh_, Render::TreeStage::RankPixel(rank));
    TreeLeaf::Build(sp.LeafParams(), Mesh_);
    foliage.Build(Mesh_, sp, 1);
    const uint32_t stride = (uint32_t)Render::TreeStage::kLeavesPerCard << (2u * (unsigned)rank);
    rankCards.clear();
    for (size_t i = 0; i < foliage.Count(); i += stride) {
      const float *c = &foliage.Instances()[i * TreeFoliage::kFloats];
      rankCards.insert(rankCards.end(), c, c + TreeFoliage::kFloats);
    }
    const uint32_t nCards = (uint32_t)(rankCards.size() / TreeFoliage::kFloats);
    /* THE CROWN'S PROJECTION IS THE SPECIES', not the rank's. A coarse rank drops the thin shoots
     * that reach furthest out, so its own bark box is smaller — sizing the leaf by that box would
     * shrink the canopy exactly where it is already thinnest. */
    if (rank == 0) crownProjM2 = foliage.CrownProjM2();
    const float cardLeafM = foliage.CardLeafM(Render::TreeStage::kLeavesPerCard, nCards,
                                              (double)sp.Lai(), crownProjM2);
    r.SetTreeBark(rank, Mesh_.BarkVerts.data(), (uint32_t)Mesh_.BarkVertexCount(),
                  Mesh_.BarkIdx.data(), (uint32_t)Mesh_.BarkIdx.size());
    r.SetTreeCards(rank, rankCards.data(), nCards, cardLeafM, kCardFanDeg);
    Log::Info("forest", "trees_grown", {{"rank", (double)rank},
        {"pixelHeightFrac", (double)Render::TreeStage::RankPixel(rank)},
        {"barkVerts", (int)Mesh_.BarkVertexCount()},
        {"barkTris", (double)(Mesh_.BarkIdx.size() / 3)},
        {"heightM", h}, {"bhdCm", 200.0 * (double)Mesh_.BhdRadius * h},
        {"cards", (double)nCards},
        {"leavesPerCard", (double)Render::TreeStage::kLeavesPerCard},
        {"cardLeafM", (double)cardLeafM}, {"declaredLeafM", (double)foliage.ScaleM()},
        {"crownProjM2", crownProjM2}, {"lai", (double)sp.Lai()},
        {"leafPoints", (double)Mesh_.LeafPoints.size()},
        {"laminae", (double)foliage.Count()}, {"perPoint", foliage.PerPoint()},
        {"growHeight", (double)g.GrowHeight()}});
    if (rank == 0) {
      r.SetTreeLook(LookOf(sp));
      /* bpar.z is the tree height and comes from SetTreeStand; without it every instance scales to
       * null. */
      r.SetTreeStand(0.0, 0.0, 0.0, h);
      Crown_.HalfWidth = std::fmax(std::fmax(-Mesh_.BoxMin.X, Mesh_.BoxMax.X),
                                   std::fmax(-Mesh_.BoxMin.Z, Mesh_.BoxMax.Z));
      Crown_.Bottom = Mesh_.BoxMin.Y;
      Crown_.Top = Mesh_.BoxMax.Y;
      Crown_.HeightM = (float)h;
      r.SetTreeCrown(Crown_.HalfWidth, Crown_.Top, Crown_.Bottom);
    }
  }
  r.BakeTreeImpostor();
  HeightSigma_ = sp.HeightSigma();
  return true;
}

void Forest::Scatter(Render::Renderer &r, const ClassField &cls, const VegetationTemplates &veg,
                     double camLat, double camLon, double eyeAglM) {
  if (!Grown() || Scattered_) return;
  double east = 0.0, north = 0.0;
  cls.Project(camLat, camLon, &east, &north);
  GroundCtx gc{&cls};
  const double gcam = fb_stream_ground(camLat, camLon);
  if (!ElevationResolved(gcam)) return;
  /* THE EYE POINT, not the ground under it: the shader hangs the foot straight off the suspension at
   * the eye, so eye height has to be subtracted here already. */
  /* ONE HANDLE FOR THE WHOLE SCATTER: taken here, held to the last sample. The builder may publish a
   * newer structure meanwhile and this one stays exactly as it was read. */
  const std::shared_ptr<const ClassStructure> structure = cls.Read();
  if (!structure) return;
  Field_.Scatter(*structure, veg, kScatterRadiusM, east, north, gcam + eyeAglM, camLat, HeightSigma_,
                 Crown_, GroundAtEnu, &gc, Stands_, Dist_);
  r.SetTreeStands(Stands_.data(), (uint32_t)StandCount(), Dist_.data());
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
  if (Field_.Cleared() > 0)
    Log::Info("forest", "stand_cleared", {{"trees", (double)Field_.Cleared()},
        {"crownHalfM", (double)(Crown_.HalfWidth * Crown_.HeightM)},
        {"crownBottomM", (double)(Crown_.Bottom * Crown_.HeightM)},
        {"crownTopM", (double)(Crown_.Top * Crown_.HeightM)}});
  Log::Info("forest", "treeline", {{"speciesLimitAslM", veg.Limit().SpeciesLimitM(camLat)},
      {"highestStandAslM", Field_.HighestStandAslM()},
      {"refusedAboveLine", (double)Field_.AboveTreeline()},
      {"refusedTooSteep", (double)Field_.TooSteep()}});
  Log::Debug("forest", "trees", {{"stands", (int)n},
      {"eastM", lo[0]}, {"eastMax", hi[0]}, {"northM", lo[1]}, {"northMax", hi[1]},
      {"footM", lo[2]}, {"footMax", hi[2]},
      {"scaleMin", lo[4]}, {"scaleMax", hi[4]}, {"scaleMean", n ? sum / (double)n : 0.0}});
}

} // namespace outshine::World
