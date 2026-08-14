/* WHAT COVERS THE PIXEL, ON BOTH SIDES (board:1138). The picture bound asks `is this pixel covered`
 * and the coverage predicate answers it from depth on our side and from alpha on the oracle's. Two
 * sides can both be covered and still disagree about WHAT covers them, and until this existed that
 * disagreement had no spelling: it was read out of the colour, where a declared colour and a shaded
 * one look the same and a surface swap arrives as a number of codes.
 *
 * THE CORRESPONDENCE IS THE CLAIM AND NOT THE EXISTENCE OF THE NUMBERS. Cycles' index pass carries
 * `material.pass_index`, which the preparer assigns in name order over `bpy.data.materials` -- a
 * numbering that includes the factory file's own materials and therefore cannot be reproduced from
 * the glTF. So the pass's integers mean nothing until they are mapped back to the file's materials,
 * and the map is DERIVED from the preparer's own record and CHECKED against the file rather than
 * assumed to be the identity: index n on their side is not material n on ours, and on this corpus it
 * never is.
 *
 * THE VACUITY CHECK COMES BEFORE ANY VERDICT RESTS ON THE PASS. board:1130 spent a round on
 * `objectIndex` over an asset carrying one object index and one material index, where "the neighbour
 * is the same surface" is true by construction and 6 038 of 6 069 pixels said nothing. A pass that
 * cannot discriminate must produce a REFUSAL, not a clean-looking agreement, so the number of
 * DISTINCT indices over the covered region is published and one is a refusal.
 *
 * NOTHING HERE IS A METRIC WITH A BOUND. Every count this produces is reported; what it feeds is the
 * picture bound's ROUTER (board:1144), and the mask it hands over carries the attributable
 * disagreements alone -- the ones this file could show were about us rather than about the oracle's
 * pass semantics or about a pixel with two surfaces on it.
 *
 * OUR SIDE'S IDENTITY IS THE SURFACE SLOT AND THE SLOT IS NOT A MATERIAL. `Resource::SceneSurfaceIdentity`
 * carries which slot the fragment's draw bound, because the renderer has no spelling for a content
 * noun; which material a slot is, is the runner's own table, and it is the runner that closes the
 * loop back to the file. */
#ifndef RENDER_SURFACEIDENTITY_H
#define RENDER_SURFACEIDENTITY_H

#include <array>
#include <string>
#include <vector>

#include "Json.h"

#include "Mask.h"
#include "OracleProduct.h"
#include "RawF32.h"

namespace outshine::Render::Parity {

/* WHICH OF THE ORACLE'S TWO INDEX PASSES (`Enum.2`). They are one reader because they are one
 * format and one numbering rule; they are two members because they partition the frame by different
 * things, and a caller that could pass "the index pass" would have to remember which. */
enum class IndexPass { Material, Object };

[[nodiscard]] inline const char *PassName(IndexPass which) {
  return which == IndexPass::Material ? "materialIndex" : "objectIndex";
}

/* The file the preparer wrote the pass into, and the key its provenance records the mapping under.
 * Both are the preparer's spellings and they stand here together so a third pass is one row. The
 * frame is the animated case's (board:1169) and is absent for a still. */
[[nodiscard]] inline std::string PassRawName(IndexPass which, std::optional<int> frame) {
  return OracleProduct{PassName(which), "default", frame}.Raw();
}

[[nodiscard]] inline const char *PassProductKey(IndexPass which) {
  return which == IndexPass::Material ? "materialIndexRaw" : "objectIndexRaw";
}

[[nodiscard]] inline const char *PassIndexedKey(IndexPass which) {
  return which == IndexPass::Material ? "materials" : "objects";
}

/* THE NAMES BEHIND THE PASS'S INTEGERS, read from the provenance document that stands beside the
 * pass file. It is bound to the FILE and not to a recipe name: the entry taken is the one whose
 * products include this pass's own raw, so a corpus whose passes came from a recipe this reader
 * never heard of still reads its own mapping rather than another render's.
 *
 * A CACHE HIT LEAVES `provenance` NULL AND THAT IS A REFUSAL, not an empty table. The preparer
 * records the mapping on the render that produced the pass; a run that reused a cached render
 * publishes the products and no mapping, so the integers are on disk with nothing to say what they
 * name. An empty table would read as "the pass names nothing", which is an answer. */
class IndexNames {
public:
  [[nodiscard]] bool ReadFile(const std::string &directory, IndexPass which,
                              std::optional<int> frame) {
    Error_.clear();
    ByPassIndex_.clear();
    const std::string path = directory + "provenance.json";
    std::string text;
    if (!Slurp(path, text)) { return Refuse(path + ": no provenance to name the pass's indices"); }
    if (!Document_.Parse(text.c_str(), text.size())) {
      return Refuse(path + ": is not valid JSON at byte " + std::to_string(Document_.StoppedAt()));
    }
    const Json::Ref renders = Document_.Root()["report"]["render"];
    const std::string wanted = PassRawName(which, frame);
    for (size_t at = 0; at < renders.Size(); ++at) {
      const Json::Ref entry = renders[at];
      if (!EndsWith(entry["products"][PassProductKey(which)]["path"].Str(""), wanted)) { continue; }
      const Json::Ref indexed = entry["provenance"]["quantities"]["indices"][PassIndexedKey(which)];
      if (indexed.GetKind() != Json::Kind::Array) {
        return Refuse(path + ": the render that produced " + wanted +
                      " records no index mapping, so the pass's integers name nothing -- its "
                      "products were reused from the store and the mapping was not");
      }
      return Take(indexed, path, wanted);
    }
    return Refuse(path + ": no render in it produced " + wanted);
  }

  [[nodiscard]] const std::string &Error() const { return Error_; }
  [[nodiscard]] size_t Count() const { return ByPassIndex_.size(); }

  /* The name at a pass index, empty where the preparer gave that integer to nothing. Index 0 is
   * never named: Cycles writes it where no surface was hit. */
  [[nodiscard]] std::string At(int passIndex) const {
    if (passIndex < 0 || (size_t)passIndex >= ByPassIndex_.size()) { return std::string(); }
    return ByPassIndex_[(size_t)passIndex];
  }

private:
  [[nodiscard]] bool Take(const Json::Ref &indexed, const std::string &path,
                          const std::string &wanted) {
    for (size_t at = 0; at < indexed.Size(); ++at) {
      const int passIndex = indexed[at]["passIndex"].Int(-1);
      const std::string name = indexed[at]["name"].Str("");
      if (passIndex < 0 || name.empty()) {
        return Refuse(path + ": an entry of " + wanted + "'s mapping carries no index or no name");
      }
      if ((size_t)passIndex >= ByPassIndex_.size()) { ByPassIndex_.resize((size_t)passIndex + 1u); }
      if (!ByPassIndex_[(size_t)passIndex].empty()) {
        return Refuse(path + ": pass index " + std::to_string(passIndex) + " of " + wanted +
                      " is given to both " + ByPassIndex_[(size_t)passIndex] + " and " + name);
      }
      ByPassIndex_[(size_t)passIndex] = name;
    }
    return true;
  }

  [[nodiscard]] bool Refuse(const std::string &why) {
    Error_ = why;
    return false;
  }

  [[nodiscard]] static bool EndsWith(const std::string &text, const std::string &tail) {
    return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
  }

  [[nodiscard]] static bool Slurp(const std::string &path, std::string &out) {
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) { return false; }
    char block[1 << 14];
    for (size_t read = std::fread(block, 1, sizeof block, file); read > 0;
         read = std::fread(block, 1, sizeof block, file)) {
      out.append(block, read);
    }
    std::fclose(file);
    return true;
  }

  Json Document_;
  std::vector<std::string> ByPassIndex_;
  std::string Error_;
};

/* WHAT ONE SIDE PUTS AT ONE PIXEL. `FileMaterial` is an index into the glTF document's own material
 * list, which is the one currency the two sides can be stated in: the oracle reaches it through the
 * preparer's mapping and its material's NAME, and we reach it through the surface slot our draw
 * bound. `Why` is filled where there is no answer, and it is the whole of the refusal for that
 * pixel -- an unresolved identity is never quietly a -1 that reads as a material. */
struct SurfaceAt {
  bool Covered = false;
  int FileMaterial = -1;
  std::string Name;
  std::string Why;
};

/* THE ORACLE'S SIDE: the pass, the mapping and the file's material names, resolved once. */
class OracleSurfaces {
public:
  [[nodiscard]] bool Read(const std::string &directory, IndexPass which, std::optional<int> frame,
                          const std::vector<std::string> &fileMaterialNames) {
    Names_ = fileMaterialNames;
    if (!Pass_.ReadFile(directory + PassRawName(which, frame))) { return Refuse(Pass_.Error()); }
    if (!Mapping_.ReadFile(directory, which, frame)) { return Refuse(Mapping_.Error()); }
    /* THE WHOLE MAP RESOLVED ONCE, HERE, so the per-pixel path is a table lookup and allocates
     * nothing (`Per.15`). The names are what the correspondence is derived FROM; a comparison over
     * two million pixels that rebuilt a `std::string` per pixel would be paying for the derivation
     * once per sample of it. */
    ByPassIndex_.assign(Mapping_.Count(), -1);
    std::string ignored;
    for (size_t index = 0; index < ByPassIndex_.size(); ++index) {
      const std::string named = Mapping_.At((int)index);
      if (named.empty()) { continue; }
      ByPassIndex_[index] = FileMaterialNamed(named, ignored);
    }
    return true;
  }

  [[nodiscard]] const std::string &Error() const { return Error_; }
  [[nodiscard]] int Width() const { return Pass_.Width(); }
  [[nodiscard]] int Height() const { return Pass_.Height(); }

  /* The pass's own integer at a pixel, which is `material.pass_index` and not a material. */
  [[nodiscard]] int IndexAt(int x, int y) const { return (int)Pass_.At(x, y, 0); }

  /* THE FILE MATERIAL AT A PIXEL, -1 where the pass's integer resolves to none. It is the same
   * answer `At` gives and it is the one the counting loop uses; `At` exists to say WHY, and only the
   * pixels that get printed pay for the sentence. */
  [[nodiscard]] int FileMaterialAt(int x, int y) const {
    const int index = IndexAt(x, y);
    if (index < 0 || (size_t)index >= ByPassIndex_.size()) { return -1; }
    return ByPassIndex_[(size_t)index];
  }

  [[nodiscard]] SurfaceAt At(int x, int y, bool covered) const {
    SurfaceAt out;
    out.Covered = covered;
    if (!covered) {
      out.Why = "the oracle draws nothing here";
      return out;
    }
    const int index = IndexAt(x, y);
    const std::string named = Mapping_.At(index);
    if (named.empty()) {
      out.Why = "the oracle's " + std::string("pass index ") + std::to_string(index) +
                " is given to no material by the preparer";
      return out;
    }
    out.Name = named;
    out.FileMaterial = FileMaterialNamed(named, out.Why);
    return out;
  }

  /* WHICH FILE MATERIAL CARRIES THIS BLENDER NAME, and it must be exactly one. The importer carries
   * a glTF material's name across unchanged and appends a suffix on collision, so a name that
   * resolves to none is a material Blender renamed or one the manifest's own arm created; a name
   * that resolves to two is a file whose materials are not distinguishable by name at all. Both are
   * refusals and neither is a nearest match. */
  [[nodiscard]] int FileMaterialNamed(const std::string &name, std::string &why) const {
    int found = -1;
    for (size_t at = 0; at < Names_.size(); ++at) {
      if (Names_[at] != name) { continue; }
      if (found >= 0) {
        why = "the file carries two materials named " + name;
        return -1;
      }
      found = (int)at;
    }
    if (found < 0) { why = "the file carries no material named " + name; }
    return found;
  }

private:
  [[nodiscard]] bool Refuse(const std::string &why) {
    Error_ = why;
    return false;
  }

  RawF32 Pass_;
  IndexNames Mapping_;
  std::vector<std::string> Names_;
  std::vector<int> ByPassIndex_;
  std::string Error_;
};

/* OUR SIDE: the identity attachment, and the runner's own slot table behind it. The attachment
 * carries the slot ONE HIGHER than it is, so 0 is "no subject fragment reached this pixel" and
 * cannot be read as the first slot. */
class OurSurfaces {
public:
  OurSurfaces(const std::vector<float> &identity, int width, const std::vector<int> &slotMaterial,
              const std::vector<std::string> &fileMaterialNames)
      : Identity_(identity), Width_(width), SlotMaterial_(slotMaterial), Names_(fileMaterialNames) {}

  [[nodiscard]] int SlotAt(int x, int y) const {
    const size_t at = ((size_t)y * (size_t)Width_ + (size_t)x) * 4u;
    if (at >= Identity_.size()) { return -1; }
    return (int)Identity_[at] - 1;
  }

  /* WHETHER THE ATTACHMENT NAMES A SLOT THIS SUBJECT HAS, which is a claim about the plumbing and
   * not about the material: a pixel our depth says we drew must carry a slot of our own table, or
   * the attachment, the pass and the encoder are not the same three things they are declared to be.
   * A slot that names no glTF material still passes it -- that is a fact about the file. */
  [[nodiscard]] bool NamesASlot(int x, int y) const {
    const int slot = SlotAt(x, y);
    return slot >= 0 && (size_t)slot < SlotMaterial_.size();
  }

  /* The file's own name for one of its materials, empty where the index names none of them. It is
   * here rather than beside the oracle because both sides state an identity in the FILE's currency,
   * so one table answers for both and a second copy could disagree with this one. */
  [[nodiscard]] std::string NameOfFileMaterial(int fileMaterial) const {
    if (fileMaterial < 0 || (size_t)fileMaterial >= Names_.size()) { return std::string(); }
    return Names_[(size_t)fileMaterial];
  }

  /* The file material at a pixel, -1 where the slot names none. `At` says why; this is what counts. */
  [[nodiscard]] int FileMaterialAt(int x, int y) const {
    const int slot = SlotAt(x, y);
    if (slot < 0 || (size_t)slot >= SlotMaterial_.size()) { return -1; }
    return SlotMaterial_[(size_t)slot];
  }

  [[nodiscard]] SurfaceAt At(int x, int y, bool covered) const {
    SurfaceAt out;
    out.Covered = covered;
    if (!covered) {
      out.Why = "we draw nothing here";
      return out;
    }
    const int slot = SlotAt(x, y);
    if (slot < 0 || (size_t)slot >= SlotMaterial_.size()) {
      out.Why = "the identity attachment holds " + std::to_string(slot + 1) +
                " here, which names no surface slot of this subject";
      return out;
    }
    out.FileMaterial = SlotMaterial_[(size_t)slot];
    if (out.FileMaterial < 0) {
      out.Why = "surface slot " + std::to_string(slot) +
                " is the engine's declared default, which is no material of the file";
      return out;
    }
    if ((size_t)out.FileMaterial < Names_.size()) { out.Name = Names_[(size_t)out.FileMaterial]; }
    return out;
  }

private:
  const std::vector<float> &Identity_;
  int Width_ = 0;
  const std::vector<int> &SlotMaterial_;
  const std::vector<std::string> &Names_;
};

/* ONE PIXEL THE TWO SIDES NAME DIFFERENTLY, kept so a report can print what each of them had rather
 * than only how many there were. */
struct Disagreement {
  int X = 0, Y = 0;
  SurfaceAt Oracle;
  SurfaceAt Ours;
};

/* WHAT THE ORACLE'S OWN PICTURE MUST HOLD WHERE ITS INDEX PASS NAMES A MATERIAL, one colour per
 * file material (board:1138). It exists because the index pass and the picture are two products of
 * one render and they DO NOT AGREE everywhere -- [MEASURED] on `coverage/negative-scale`, 189 of
 * 47 532 covered pixels carry `ChecksAndXMaterial` in the index while the picture carries
 * `BackgroundMaterial`'s declared colour bit-for-bit, in a strip along the plates' left edges.
 * Cycles writes the index passes at the first camera-ray intersection, and the arms that express
 * glTF's front-face rule put a Transparent BSDF on `Backfacing` -- so where a face is culled the ray
 * continues and colours the pixel from behind while the index keeps naming the face it passed
 * through. A pixel like that says nothing about US, and counting it as a disagreement would be a
 * number about the oracle's two products wearing our name.
 *
 * IT IS COMPUTABLE ONLY WHERE THE CASE DECLARES ITS COLOURS. Where the appearance comes from the
 * file's own images or from a light there is no closed form to hold the picture against, and this
 * says so rather than standing in with something looser: `Computable` false with `Why` filled is the
 * whole of that, and the counts that depend on it are published as absent.
 *
 * THE PREDICATE IS EXACT EQUALITY AND CARRIES NO TOLERANCE. [MEASURED] on the same case: 47 343
 * pixels agree BIT-FOR-BIT and 189 differ, with nothing in between at any tolerance down to 1e-6, so
 * a threshold here would be a free parameter with nothing to fit. */
struct DeclaredColours {
  bool Computable = false;
  std::string Why;
  std::vector<std::array<float, 3>> ByFileMaterial;
  std::vector<uint8_t> Known;

  [[nodiscard]] bool IsPictureOf(int fileMaterial, const RawF32 &picture, int x, int y) const {
    if (fileMaterial < 0 || (size_t)fileMaterial >= Known.size() || !Known[(size_t)fileMaterial]) {
      return false;
    }
    const std::array<float, 3> &declared = ByFileMaterial[(size_t)fileMaterial];
    for (int channel = 0; channel < 3; ++channel) {
      if (picture.At(x, y, channel) != declared[(size_t)channel]) { return false; }
    }
    return true;
  }
};

/* WHICH OF THE FILE'S MATERIALS COMPOSITE RATHER THAN COVER (board:1144), one flag per file
 * material, read from the glTF's own `alphaMode` and from nothing derived.
 *
 * IT IS A SECOND EXCLUSION AND IT IS NOT `board:1155`'s. That one is about the oracle contradicting
 * its own picture; this one is about a pixel that HAS NO SINGLE SURFACE. Where a BLEND surface
 * stands at a pixel, the colour there is a composite of it and of everything behind it, and both
 * index passes are single-valued by construction -- so ours names the blended quad and Cycles' names
 * whatever its ray came to rest on, and the two are answers to different questions rather than a
 * disagreement about one. [MEASURED] on `materials/alpha-blend-mode`, whose one BLEND material is
 * `MatBlend`: of 21 739 disagreements, 21 130 are `MatBed` against `MatBlend` and 575 are
 * `MatOpaque` against `MatBlend` -- 21 705, or 99.84 per cent -- while the remaining 34 name a MASK
 * quad on the oracle's side and the bed on ours, which is a different mechanism and is NOT excluded
 * here.
 *
 * IT IS COMPUTABLE WHERE THE DECLARED COLOURS ARE NOT, which is the reason it is worth having
 * separately: `alpha-blend-mode` takes its appearance from the file's own images, so nothing there
 * can be held against a declared colour, and without this predicate its whole disagreeing population
 * is `attribution not known` -- a NaN standing where a mechanism was available for the asking. */
[[nodiscard]] inline bool CompositesRatherThanCovers(const std::vector<uint8_t> &blended,
                                                     int fileMaterial) {
  return fileMaterial >= 0 && (size_t)fileMaterial < blended.size() &&
         blended[(size_t)fileMaterial] != 0u;
}

/* WHAT THE READING IS ASKED ABOUT, as one parameter object rather than seven arguments (`I.23`): the
 * two sides, the two coverage masks, the oracle's own picture with the colours that let its index
 * pass be held against it, and which of the file's materials composite. */
struct IdentityQuestion {
  const OracleSurfaces &Oracle;
  const OurSurfaces &Ours;
  const Mask &TheirCoverage;
  const Mask &OurCoverage;
  const RawF32 &OraclePicture;
  const DeclaredColours &Declared;
  const std::vector<uint8_t> &BlendedFileMaterial;
};

/* HOW OFTEN ONE SURFACE STOOD WHERE THE OTHER SIDE PUT ANOTHER, over the whole disagreeing
 * population rather than over the eight pixels that fit in a printout (board:1144). A list of
 * coordinates says a disagreement exists; this says WHICH TWO SURFACES are being swapped, and that
 * is what separates one mechanism from another without opening a raw file: `MatBed` against
 * `MatBlend` twenty-one thousand times is a blend, `LabelMat` against `BackgroundMaterial` once is
 * an edge. */
struct Swap {
  int Oracle = -1;
  int Ours = -1;
  std::string OracleName;
  std::string OursName;
  size_t Pixels = 0;
};

/* WHAT THE PASS SAID, OVER THE POPULATION IT WAS ASKED ABOUT. `Refusal` is non-empty exactly where
 * `Adjudicated` is false, and there is no third state: a count published beside an empty refusal
 * would be a verdict that had quietly decided nothing. */
struct IdentityReading {
  bool Adjudicated = false;
  std::string Refusal;
  /* DISTINCT indices over the covered region, which is the vacuity check. One is a refusal. */
  size_t OracleDistinct = 0;
  size_t OursDistinct = 0;
  size_t BothCovered = 0;
  size_t Compared = 0;
  size_t Agreeing = 0;
  size_t Disagreeing = 0;
  size_t Unresolved = 0;
  /* Pixels we drew whose identity attachment names no slot of our own table. It is not a comparison
   * with the oracle and it is not gated by the vacuity check: it is counted over everything WE
   * cover, because a broken attachment must be loud on a case whose oracle decides nothing. */
  size_t OursNamingNoSlot = 0;
  /* Pixels where the oracle's index pass names a material its own picture is not showing. Meaningful
   * only where the colours were computable, which `Split.Computable` is what says. */
  size_t OracleSplit = 0;
  /* Pixels where one side names a BLEND surface, so the colour there is a composite and neither
   * single-valued pass is naming "the" surface. Computable on every case, because it is read off the
   * file's own `alphaMode`. */
  size_t Composite = 0;
  /* The disagreements left once those are set aside: what a routing decision may act on. It is
   * KNOWN only where every disagreement could be attributed to one side or the other, which is why
   * the flag stands beside it: a case whose colours are not computable and whose sides disagree has
   * an attributable count of "not this instrument's to say", and a number there would be the whole
   * disagreement wearing the word `attributable`. Where nothing disagrees it is known and zero,
   * because that needs no attribution at all. */
  size_t Attributable = 0;
  bool AttributionKnown = true;
  /* WHERE THOSE PIXELS ARE, which is what the picture bound's router spends (board:1144). It carries
   * the attributable population and never the disagreeing one, and it is empty on a case that
   * refused -- so a reading nobody could adjudicate routes nothing rather than routing everything it
   * happened to see before it stopped. */
  Mask AttributableAt;
  /* Why an identity could not be resolved, taken from the last pixel that could not: a count of
   * unresolved pixels without the reason sends its reader back to the pass file. */
  std::string Unresolvable;
  std::vector<Disagreement> Disagreements;
  std::vector<Disagreement> Splits;
  std::vector<Swap> Swaps;
};

/* HOW MANY DISAGREEING PIXELS ARE KEPT. [SET] 8: the record exists so a reader can see WHAT each
 * side had, and a case whose whole subject is swapped would otherwise print two million lines. The
 * COUNT is the population and is never truncated; this bounds the printing alone. */
constexpr size_t kKeptDisagreements = 8;

[[nodiscard]] inline size_t DistinctOracleIndices(const OracleSurfaces &oracle, const Mask &theirs) {
  std::vector<int> seen;
  for (int y = 0; y < theirs.Height; ++y) {
    for (int x = 0; x < theirs.Width; ++x) {
      if (!theirs.At(x, y)) { continue; }
      const int index = oracle.IndexAt(x, y);
      bool known = false;
      for (const int had : seen) { known = known || had == index; }
      if (!known) { seen.push_back(index); }
    }
  }
  return seen.size();
}

[[nodiscard]] inline size_t DistinctOurSlots(const OurSurfaces &ours, const Mask &mask) {
  std::vector<int> seen;
  for (int y = 0; y < mask.Height; ++y) {
    for (int x = 0; x < mask.Width; ++x) {
      if (!mask.At(x, y)) { continue; }
      const int slot = ours.SlotAt(x, y);
      bool known = false;
      for (const int had : seen) { known = known || had == slot; }
      if (!known) { seen.push_back(slot); }
    }
  }
  return seen.size();
}

namespace Detail {

/* ONE MORE PIXEL ON THE ROW FOR THIS PAIR OF SURFACES, the row created the first time the pair is
 * seen. Linear over the rows because the number of rows is bounded by the file's material count
 * squared and every corpus subject carries under a dozen; the NAMES are resolved once per row rather
 * than once per pixel, which is what keeps a twenty-one-thousand-pixel census down to one
 * allocation per PAIR rather than one per pixel (`Per.15`). */
inline void Count(std::vector<Swap> &swaps, int oracle, int ours, const IdentityQuestion &asked) {
  for (Swap &row : swaps) {
    if (row.Oracle != oracle || row.Ours != ours) { continue; }
    ++row.Pixels;
    return;
  }
  Swap row;
  row.Oracle = oracle;
  row.Ours = ours;
  /* BOTH INDICES ARE THE FILE'S OWN, so one table names both and the oracle's side is not asked
   * through its Blender name a second time -- that name was already spent resolving the index. */
  row.OracleName = asked.Ours.NameOfFileMaterial(oracle);
  row.OursName = asked.Ours.NameOfFileMaterial(ours);
  row.Pixels = 1;
  swaps.push_back(row);
}

/* A REFUSAL EMPTIES THE ROUTING MASK, and that is the whole reason this is a function rather than
 * two assignments (board:1144). A reading that could not be adjudicated has pixels marked on it from
 * before the refusal was reached, and a router that spent them would be routing on a population the
 * instrument had just declined to stand behind. */
inline void Refuse(IdentityReading &reading, const std::string &why) {
  reading.Refusal = why;
  reading.AttributableAt = Mask{};
}

} // namespace Detail

/* THE PER-PIXEL PREDICATE, beside the coverage predicate that already exists: the two sides agree
 * about which surface is here. It runs over the pixels BOTH sides cover, because a pixel one of them
 * leaves empty is a coverage disagreement and is already counted as one -- reading it here would
 * count the same defect twice under a second name.
 *
 * A PIXEL WHOSE IDENTITY EITHER SIDE CANNOT RESOLVE IS COUNTED AS UNRESOLVED AND NEVER AS AGREEING.
 * That is the whole difference between this and an instrument that reports what it could not see as
 * a pass. */
[[nodiscard]] inline IdentityReading ReadSurfaceIdentity(const IdentityQuestion &asked) {
  const Mask &theirs = asked.TheirCoverage;
  const Mask &oursMask = asked.OurCoverage;
  IdentityReading out;
  out.OracleDistinct = DistinctOracleIndices(asked.Oracle, theirs);
  out.OursDistinct = DistinctOurSlots(asked.Ours, oursMask);
  for (int y = 0; y < oursMask.Height; ++y) {
    for (int x = 0; x < oursMask.Width; ++x) {
      if (oursMask.At(x, y) && !asked.Ours.NamesASlot(x, y)) { ++out.OursNamingNoSlot; }
    }
  }
  if (out.OracleDistinct <= 1) {
    out.Refusal = "the oracle's index pass carries " + std::to_string(out.OracleDistinct) +
                  " distinct index over the region it covers, so agreeing about which surface is "
                  "here is true by construction and decides nothing";
    return out;
  }
  out.AttributableAt.Width = theirs.Width;
  out.AttributableAt.Height = theirs.Height;
  out.AttributableAt.In.assign((size_t)theirs.Width * (size_t)theirs.Height, 0u);
  for (int y = 0; y < theirs.Height; ++y) {
    for (int x = 0; x < theirs.Width; ++x) {
      const bool bothCovered = theirs.At(x, y) && oursMask.At(x, y);
      if (!bothCovered) { continue; }
      ++out.BothCovered;
      const int cycles = asked.Oracle.FileMaterialAt(x, y);
      const int mine = asked.Ours.FileMaterialAt(x, y);
      if (cycles < 0 || mine < 0) {
        ++out.Unresolved;
        if (out.Unresolvable.empty()) {
          out.Unresolvable = cycles < 0 ? asked.Oracle.At(x, y, true).Why : asked.Ours.At(x, y, true).Why;
        }
        continue;
      }
      ++out.Compared;
      if (cycles == mine) {
        ++out.Agreeing;
        continue;
      }
      ++out.Disagreeing;
      Detail::Count(out.Swaps, cycles, mine, asked);
      if (out.Disagreements.size() < kKeptDisagreements) {
        out.Disagreements.push_back({x, y, asked.Oracle.At(x, y, true), asked.Ours.At(x, y, true)});
      }
      /* WHOSE DISAGREEMENT IT IS, DECIDED BEFORE IT IS COUNTED AS OURS, AND THE CHEAPER PREDICATE
       * RUNS FIRST -- not for speed but because it is the one that needs no colour: a case whose
       * appearance comes from an image can still say "this pixel has two surfaces on it", and asking
       * the colour question first would answer `not known` on a pixel that has a mechanism. */
      if (CompositesRatherThanCovers(asked.BlendedFileMaterial, cycles) ||
          CompositesRatherThanCovers(asked.BlendedFileMaterial, mine)) {
        ++out.Composite;
        continue;
      }
      if (!asked.Declared.Computable) {
        out.AttributionKnown = false;
        continue;
      }
      if (asked.Declared.IsPictureOf(cycles, asked.OraclePicture, x, y)) {
        ++out.Attributable;
        out.AttributableAt.In[(size_t)y * (size_t)theirs.Width + (size_t)x] = 1u;
        continue;
      }
      ++out.OracleSplit;
      if (out.Splits.size() < kKeptDisagreements) {
        out.Splits.push_back({x, y, asked.Oracle.At(x, y, true), asked.Ours.At(x, y, true)});
      }
    }
  }
  /* AN ATTRIBUTION NOBODY KNOWS ROUTES NOTHING, stated rather than derived from the fact that the
   * two arms in the loop are mutually exclusive per case: they are exclusive because `Computable` is
   * a property of the CASE, and a later arm that decided it per pixel would silently make a mask of
   * half-attributed pixels routable. */
  if (!out.AttributionKnown) { out.AttributableAt = Mask{}; }
  /* AN UNRESOLVED POPULATION IS A REFUSAL AND NOT A SMALLER SAMPLE. Where the oracle's materials are
   * not the file's -- the arms that replace every material with one emitter, or one per NODE -- the
   * two sides partition the frame by different things, and a count over the pixels that happened to
   * resolve would be a number about a population nobody chose. */
  if (out.Compared == 0) {
    Detail::Refuse(out, "no pixel both sides cover carries an identity both of them can resolve, so "
                        "the oracle's materials are not this file's and the two partitions are not "
                        "comparable -- " + out.Unresolvable);
    return out;
  }
  if (out.Unresolved > 0) {
    Detail::Refuse(out, std::to_string(out.Unresolved) + " of " + std::to_string(out.BothCovered) +
                            " pixels both sides cover carry an identity one of them cannot resolve "
                            "-- " + out.Unresolvable + " -- so the two partitions are not the same "
                            "one");
    return out;
  }
  out.Adjudicated = true;
  return out;
}

} // namespace outshine::Render::Parity
#endif
