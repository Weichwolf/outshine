#ifndef RENDER_SURFACEIDENTITY_H
#define RENDER_SURFACEIDENTITY_H

#include <cctype>
#include <array>
#include <string>
#include <vector>

#include "Json.h"

#include "Mask.h"
#include "OracleProduct.h"
#include "RawF32.h"

namespace outshine::Render::Parity {

enum class IndexPass { Material, Object };

[[nodiscard]] inline const char *PassName(IndexPass which) {
  return which == IndexPass::Material ? "materialIndex" : "objectIndex";
}

[[nodiscard]] inline std::string PassRawName(IndexPass which, std::optional<int> frame) {
  return OracleProduct{PassName(which), "default", frame}.Raw();
}

[[nodiscard]] inline const char *PassProductKey(IndexPass which) {
  return which == IndexPass::Material ? "materialIndexRaw" : "objectIndexRaw";
}

[[nodiscard]] inline const char *PassIndexedKey(IndexPass which) {
  return which == IndexPass::Material ? "materials" : "objects";
}

class IndexNames {
public:
  [[nodiscard]] bool
  ReadFile(const std::string &directory, IndexPass which, std::optional<int> frame) {
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
                      " records no index mapping, so the pass's integers name nothing");
      }
      return Take(indexed, path, wanted);
    }
    return Refuse(path + ": no render in it produced " + wanted);
  }

  [[nodiscard]] const std::string &Error() const { return Error_; }

  [[nodiscard]] size_t Count() const { return ByPassIndex_.size(); }

  [[nodiscard]] std::string At(int passIndex) const {
    if (passIndex < 0 || (size_t)passIndex >= ByPassIndex_.size()) { return std::string(); }
    return ByPassIndex_[(size_t)passIndex];
  }

private:
  [[nodiscard]] bool
  Take(const Json::Ref &indexed, const std::string &path, const std::string &wanted) {
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
    return text.size() >= tail.size() &&
           text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
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

struct SurfaceAt {
  bool Covered = false;
  int FileMaterial = -1;
  std::string Name;
  std::string Why;
};

class OracleSurfaces {
public:
  [[nodiscard]] bool Read(const std::string &directory,
                          IndexPass which,
                          std::optional<int> frame,
                          const std::vector<std::string> &fileMaterialNames) {
    Names_ = fileMaterialNames;
    if (!Pass_.ReadFile(directory + PassRawName(which, frame))) { return Refuse(Pass_.Error()); }
    if (!Mapping_.ReadFile(directory, which, frame)) { return Refuse(Mapping_.Error()); }

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

  [[nodiscard]] int IndexAt(int x, int y) const { return (int)Pass_.At(x, y, 0); }

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

  [[nodiscard]] int FileMaterialNamed(const std::string &name, std::string &why) const {
    std::string wanted = name;
    bool present = false;
    for (const std::string &known : Names_) { present |= known == wanted; }
    if (!present && wanted.size() > 4) {
      const std::string tail = wanted.substr(wanted.size() - 4);
      if (tail[0] == '.' && std::isdigit((unsigned char)tail[1]) &&
          std::isdigit((unsigned char)tail[2]) && std::isdigit((unsigned char)tail[3])) {
        wanted = wanted.substr(0, wanted.size() - 4);
      }
    }
    int found = -1;
    for (size_t at = 0; at < Names_.size(); ++at) {
      if (Names_[at] != wanted) { continue; }
      if (found >= 0) {
        why = "the file carries two materials named " + wanted;
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

class OurSurfaces {
public:
  OurSurfaces(const std::vector<float> &identity,
              int width,
              const std::vector<int> &slotMaterial,
              const std::vector<std::string> &fileMaterialNames)
      : Identity_(identity),
        Width_(width),
        SlotMaterial_(slotMaterial),
        Names_(fileMaterialNames) {}

  [[nodiscard]] int SlotAt(int x, int y) const {
    const size_t at = ((size_t)y * (size_t)Width_ + (size_t)x) * 4u;
    if (at >= Identity_.size()) { return -1; }
    return (int)Identity_[at] - 1;
  }

  [[nodiscard]] bool NamesASlot(int x, int y) const {
    const int slot = SlotAt(x, y);
    return slot >= 0 && (size_t)slot < SlotMaterial_.size();
  }

  [[nodiscard]] std::string NameOfFileMaterial(int fileMaterial) const {
    if (fileMaterial < 0 || (size_t)fileMaterial >= Names_.size()) { return std::string(); }
    return Names_[(size_t)fileMaterial];
  }

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

struct Disagreement {
  int X = 0, Y = 0;
  SurfaceAt Oracle;
  SurfaceAt Ours;
};

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

[[nodiscard]] inline bool CompositesRatherThanCovers(const std::vector<uint8_t> &blended,
                                                     int fileMaterial) {
  return fileMaterial >= 0 && (size_t)fileMaterial < blended.size() &&
         blended[(size_t)fileMaterial] != 0u;
}

struct IdentityQuestion {
  const OracleSurfaces &Oracle;
  const OurSurfaces &Ours;
  const Mask &TheirCoverage;
  const Mask &OurCoverage;
  const RawF32 &OraclePicture;
  const DeclaredColours &Declared;
  const std::vector<uint8_t> &BlendedFileMaterial;
};

struct Swap {
  int Oracle = -1;
  int Ours = -1;
  std::string OracleName;
  std::string OursName;
  size_t Pixels = 0;
};

struct IdentityReading {
  bool Adjudicated = false;
  std::string Refusal;

  size_t OracleDistinct = 0;
  size_t OursDistinct = 0;
  size_t BothCovered = 0;
  size_t Compared = 0;
  size_t Agreeing = 0;
  size_t Disagreeing = 0;
  size_t Unresolved = 0;

  size_t OursNamingNoSlot = 0;

  size_t OracleSplit = 0;

  size_t Composite = 0;

  size_t Attributable = 0;
  bool AttributionKnown = true;

  Mask AttributableAt;

  std::string Unresolvable;
  std::vector<Disagreement> Disagreements;
  std::vector<Disagreement> Splits;
  std::vector<Swap> Swaps;
};

constexpr size_t kKeptDisagreements = 8;

[[nodiscard]] inline size_t DistinctOracleIndices(const OracleSurfaces &oracle,
                                                  const Mask &theirs) {
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

inline void Count(std::vector<Swap> &swaps, int oracle, int ours, const IdentityQuestion &asked) {
  for (Swap &row : swaps) {
    if (row.Oracle != oracle || row.Ours != ours) { continue; }
    ++row.Pixels;
    return;
  }
  Swap row;
  row.Oracle = oracle;
  row.Ours = ours;

  row.OracleName = asked.Ours.NameOfFileMaterial(oracle);
  row.OursName = asked.Ours.NameOfFileMaterial(ours);
  row.Pixels = 1;
  swaps.push_back(row);
}

inline void Refuse(IdentityReading &reading, const std::string &why) {
  reading.Refusal = why;
  reading.AttributableAt = Mask{};
}

} // namespace Detail

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
          out.Unresolvable =
              cycles < 0 ? asked.Oracle.At(x, y, true).Why : asked.Ours.At(x, y, true).Why;
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

  if (!out.AttributionKnown) { out.AttributableAt = Mask{}; }

  if (out.Compared == 0) {
    Detail::Refuse(out,
                   "no pixel both sides cover carries an identity both of them can resolve, so "
                   "the oracle's materials are not this file's and the two partitions are not "
                   "comparable -- " +
                       out.Unresolvable);
    return out;
  }
  if (out.Unresolved > 0) {
    Detail::Refuse(out,
                   std::to_string(out.Unresolved) + " of " + std::to_string(out.BothCovered) +
                       " pixels both sides cover carry an identity one of them cannot resolve "
                       "-- " +
                       out.Unresolvable +
                       " -- so the two partitions are not the same "
                       "one");
    return out;
  }
  out.Adjudicated = true;
  return out;
}

} // namespace outshine::Render::Parity
#endif
