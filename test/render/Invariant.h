/* WHAT A `stated-invariant` CASE IS SCORED ON: the relation the ASSET states about a render of
 * itself, evaluated on the scene-referred linear tap and on nothing else.
 *
 * THE INSTRUMENT IS DECLARED, NOT CHOSEN HERE. Each check names its kind, carries Khronos's own
 * words and the file they came from, and states every number with an origin -- so a case cannot move
 * from one instrument to another without a quotation moving with it, and cannot acquire a threshold
 * nobody derived. The three kinds below are the whole set this runner has, and an unknown one is a
 * refusal naming what it may be.
 *
 * `linear-ceiling` -- no covered pixel is above a declared per-channel value. `DirectionalLight`:
 * "it should never be the case that light contribution go above (Color * Intensity)".
 *
 * `hue-of-brightest` -- over the brightest declared fraction of the covered pixels, each channel's
 * share of the pixel's own sum equals a declared hue. `DirectionalLight` states the arithmetic
 * itself: "redHue = R / R + G + B", and gives the number for its own light. IT IS A RATIO AND NOT A
 * MAGNITUDE, which is exactly why it survives the precision caveat the asset states in the same
 * paragraph.
 *
 * `region-compare` -- two rectangles of the frame, congruent by construction, agree per channel
 * after a declared scale. `PointLightIntensityTest`: "the expectation for the bottom row of this
 * test is that the 'Red + Green + Blue' test will be identical (or very nearly so) to the 'White'
 * test". THE TWO RECTANGLES ARE PIXELS AND THEY ARE DERIVED: the case declares an orthographic
 * camera precisely so that two bodies 2.25 m apart project to an exact integer pixel offset, which
 * is what turns "identical" into a decidable claim rather than a resampling.
 *
 * WHY THE COMPARISON IS IN ULPS AND NOT IN A RELATIVE TOLERANCE: both sides come out of one shader
 * over one surface, so the only thing between them is the order f32 rounded a sum of the same terms
 * at two positions. An ulp count says exactly that and a relative epsilon hides how much room it
 * left. */
#ifndef RENDER_INVARIANT_H
#define RENDER_INVARIANT_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "Acceptance.h"
#include "Json.h"
#include "Metric.h"

namespace outshine::Render::Parity {

enum class InvariantKind { LinearCeiling, HueOfBrightest, RegionCompare };

/* A rectangle of the frame, in pixels, with its origin at the top-left the tap itself uses. */
struct PixelRect {
  int X = 0, Y = 0, Width = 0, Height = 0;
};

struct Invariant {
  InvariantKind Kind = InvariantKind::LinearCeiling;
  std::string Name;
  double Rgb[3] = {0, 0, 0};
  double Fraction = 0;      /* HueOfBrightest: the share of covered pixels judged */
  double Tolerance = 0;     /* HueOfBrightest: the largest admissible hue error */
  PixelRect From, To;       /* RegionCompare */
  double Scale = 1.0;       /* RegionCompare: what `To` is multiplied by before comparing */
  double MaxUlps = 0;       /* RegionCompare: the widest admissible f32 disagreement */
  /* WHICH CHANNELS THE COMPARISON READS, and it is declared because the asset states two different
   * claims: the bottom row must match in ALL THREE -- "the 'Red + Green + Blue' test will be
   * identical to the 'White' test" -- while the top row matches ONE at a time -- "when viewing the
   * red channel of the output in isolation, the 'Red' test and the 'White' test should be very
   * similar". A check that always read three would fail the second on the two channels the asset
   * says nothing about. */
  bool Channel[3] = {true, true, true};
};

[[nodiscard]] inline bool ReadInvariantKind(const std::string &spelling, InvariantKind &out,
                                            std::string &error) {
  if (spelling == "linear-ceiling") {
    out = InvariantKind::LinearCeiling;
    return true;
  }
  if (spelling == "hue-of-brightest") {
    out = InvariantKind::HueOfBrightest;
    return true;
  }
  if (spelling == "region-compare") {
    out = InvariantKind::RegionCompare;
    return true;
  }
  error = "statedInvariants[].kind '" + spelling +
          "' is none of linear-ceiling, hue-of-brightest, region-compare";
  return false;
}

[[nodiscard]] inline bool ReadTriple(const Json::Ref &entry, const char *name, double out[3],
                                     std::string &error) {
  if (entry["value"].Size() != 3) {
    error = std::string(name) + " carries no three-channel value";
    return false;
  }
  double first = 0;
  const Json::Ref probe = entry["value"][size_t{0}];
  if (probe.GetKind() != Json::Kind::Number) {
    error = std::string(name) + " carries a non-numeric channel";
    return false;
  }
  first = probe.Num(0.0);
  (void)first;
  const std::string origin = entry["origin"].Str("");
  if (origin != "SET" && origin != "derived" && origin != "measured") {
    error = std::string(name) + " has origin '" + origin +
            "', and a number's origin is SET, derived or measured";
    return false;
  }
  if (origin == "derived" && entry["derivation"].Str("").empty()) {
    error = std::string(name) + " is derived and states no derivation";
    return false;
  }
  if (entry["unit"].Str("").empty()) {
    error = std::string(name) + " states no unit";
    return false;
  }
  for (size_t channel = 0; channel < 3; ++channel) {
    out[channel] = entry["value"][channel].Num(0.0);
  }
  return true;
}

[[nodiscard]] inline bool ReadRect(const Json::Ref &entry, const char *name, PixelRect &out,
                                   std::string &error) {
  if (entry["value"].Size() != 4) {
    error = std::string(name) + " is not four numbers -- x, y, width, height, in pixels";
    return false;
  }
  const std::string origin = entry["origin"].Str("");
  if (origin != "SET" && origin != "derived" && origin != "measured") {
    error = std::string(name) + " has origin '" + origin + "'";
    return false;
  }
  if (origin == "derived" && entry["derivation"].Str("").empty()) {
    error = std::string(name) + " is derived and states no derivation";
    return false;
  }
  out.X = (int)entry["value"][size_t{0}].Num(0.0);
  out.Y = (int)entry["value"][size_t{1}].Num(0.0);
  out.Width = (int)entry["value"][size_t{2}].Num(0.0);
  out.Height = (int)entry["value"][size_t{3}].Num(0.0);
  return out.Width > 0 && out.Height > 0;
}

/* Every check the case declares, in the order it declares them. A `stated-invariant` case that
 * declares none is a refusal: its whole acceptance would then be a picture nobody compares. */
[[nodiscard]] inline bool ReadInvariants(const Json::Ref &declared, std::vector<Invariant> &out,
                                         std::string &error) {
  out.clear();
  for (size_t at = 0; at < declared.Size(); ++at) {
    const Json::Ref entry = declared[at];
    Invariant check;
    if (!ReadInvariantKind(entry["kind"].Str(""), check.Kind, error)) { return false; }
    check.Name = entry["name"].Str("");
    if (check.Name.empty()) {
      error = "statedInvariants[" + std::to_string(at) + "] carries no name to report under";
      return false;
    }
    if (entry["says"].Str("").empty() || entry["statedAt"].Str("").empty()) {
      error = "statedInvariants[" + check.Name +
              "] states no `says` or no `statedAt`, so the invariant is ours and not the asset's";
      return false;
    }
    const std::string where = "statedInvariants[" + check.Name + "]";
    switch (check.Kind) {
      case InvariantKind::LinearCeiling:
        if (!ReadTriple(entry["ceilingLinear"], (where + ".ceilingLinear").c_str(), check.Rgb,
                        error)) {
          return false;
        }
        break;
      case InvariantKind::HueOfBrightest:
        if (!ReadTriple(entry["hue"], (where + ".hue").c_str(), check.Rgb, error)) { return false; }
        if (!ReadDeclaredNumber(entry["brightestFraction"],
                                (where + ".brightestFraction").c_str(), check.Fraction, error) ||
            !ReadDeclaredNumber(entry["maxHueError"], (where + ".maxHueError").c_str(),
                                check.Tolerance, error)) {
          return false;
        }
        break;
      case InvariantKind::RegionCompare:
        if (!ReadRect(entry["fromPx"], (where + ".fromPx").c_str(), check.From, error) ||
            !ReadRect(entry["toPx"], (where + ".toPx").c_str(), check.To, error)) {
          return false;
        }
        if (check.From.Width != check.To.Width || check.From.Height != check.To.Height) {
          error = where + " compares a " + std::to_string(check.From.Width) + "x" +
                  std::to_string(check.From.Height) + " rectangle with a " +
                  std::to_string(check.To.Width) + "x" + std::to_string(check.To.Height) +
                  " one, and two rectangles of different shapes have no pixel correspondence";
          return false;
        }
        if (!ReadDeclaredNumber(entry["scale"], (where + ".scale").c_str(), check.Scale, error) ||
            !ReadDeclaredNumber(entry["maxUlps"], (where + ".maxUlps").c_str(), check.MaxUlps,
                                error)) {
          return false;
        }
        if (entry["channels"].Valid()) {
          for (bool &channel : check.Channel) { channel = false; }
          for (size_t which = 0; which < entry["channels"].Size(); ++which) {
            const int channel = entry["channels"][which].Int(-1);
            if (channel < 0 || channel > 2) {
              error = where + ".channels names channel " + std::to_string(channel) +
                      ", and a linear tap has three";
              return false;
            }
            check.Channel[channel] = true;
          }
        }
        break;
    }
    out.push_back(std::move(check));
  }
  if (out.empty()) {
    error = "the criterion is stated-invariant and the manifest declares no statedInvariants, so "
            "nothing the asset says would be checked";
    return false;
  }
  return true;
}

/* THE TAP AS A GRID: four channels per pixel, row-major, top row first -- the same shape the oracle
 * dump has, so a rectangle means the same thing on both sides. */
struct LinearFrame {
  const std::vector<float> *Samples = nullptr;
  int Width = 0;
  int Height = 0;

  [[nodiscard]] bool Holds() const {
    return Samples != nullptr &&
           Samples->size() >= (size_t)Width * (size_t)Height * 4u && Width > 0 && Height > 0;
  }
  [[nodiscard]] float At(int x, int y, int channel) const {
    return (*Samples)[((size_t)y * (size_t)Width + (size_t)x) * 4u + (size_t)channel];
  }
  [[nodiscard]] bool Covered(int x, int y) const { return At(x, y, 3) > 0.0f; }
};

/* Ordered as integers, which is what an ulp distance IS for IEEE-754 binary32 of the same sign. Both
 * sides of every comparison here are radiances, so neither is negative and no sign-magnitude
 * correction is needed; a negative would be a defect the caller sees as a huge count. */
inline int64_t UlpsBetween(float a, float b) {
  int32_t left = 0, right = 0;
  std::memcpy(&left, &a, sizeof left);
  std::memcpy(&right, &b, sizeof right);
  return left > right ? (int64_t)left - (int64_t)right : (int64_t)right - (int64_t)left;
}

/* One check against one render, as the metrics it produces. The count is what is enforced; the
 * worst observed value goes beside it as a `Reported` metric so a case that passes narrowly is
 * visible before it starts failing. */
inline void Evaluate(const Invariant &check, const LinearFrame &frame,
                     std::vector<Metric> &metrics) {
  switch (check.Kind) {
    case InvariantKind::LinearCeiling: {
      size_t above = 0;
      double worst = 0;
      for (int y = 0; y < frame.Height; ++y) {
        for (int x = 0; x < frame.Width; ++x) {
          if (!frame.Covered(x, y)) { continue; }
          bool over = false;
          for (int channel = 0; channel < 3; ++channel) {
            const double value = (double)frame.At(x, y, channel);
            const double excess = check.Rgb[channel] > 0 ? value / check.Rgb[channel] : 0.0;
            if (excess > worst) { worst = excess; }
            over = over || value > check.Rgb[channel];
          }
          above += over ? 1u : 0u;
        }
      }
      metrics.push_back({check.Name + "_samples_above_ceiling", (double)above, 0.0, "px",
                         Direction::AtMost});
      metrics.push_back({check.Name + "_worst_fraction_of_ceiling", worst, 0.0, "dimensionless",
                         Direction::Reported});
      break;
    }
    case InvariantKind::HueOfBrightest: {
      std::vector<double> sums;
      for (int y = 0; y < frame.Height; ++y) {
        for (int x = 0; x < frame.Width; ++x) {
          if (!frame.Covered(x, y)) { continue; }
          sums.push_back((double)frame.At(x, y, 0) + (double)frame.At(x, y, 1) +
                         (double)frame.At(x, y, 2));
        }
      }
      std::sort(sums.begin(), sums.end());
      const double floorSum = Percentile(sums, 1.0 - check.Fraction);
      size_t judged = 0, missed = 0;
      double worst = 0;
      for (int y = 0; y < frame.Height; ++y) {
        for (int x = 0; x < frame.Width; ++x) {
          if (!frame.Covered(x, y)) { continue; }
          const double sum = (double)frame.At(x, y, 0) + (double)frame.At(x, y, 1) +
                             (double)frame.At(x, y, 2);
          if (!(sum >= floorSum) || !(sum > 0)) { continue; }
          ++judged;
          bool off = false;
          for (int channel = 0; channel < 3; ++channel) {
            const double error = std::fabs((double)frame.At(x, y, channel) / sum -
                                           check.Rgb[channel]);
            if (error > worst) { worst = error; }
            off = off || error > check.Tolerance;
          }
          missed += off ? 1u : 0u;
        }
      }
      metrics.push_back({check.Name + "_samples_off_hue", (double)missed, 0.0, "px",
                         Direction::AtMost});
      metrics.push_back({check.Name + "_samples_judged", (double)judged, 1.0, "px",
                         Direction::AtLeast});
      metrics.push_back({check.Name + "_worst_hue_error", worst, 0.0, "dimensionless",
                         Direction::Reported});
      break;
    }
    case InvariantKind::RegionCompare: {
      size_t apart = 0, compared = 0;
      int64_t worst = 0;
      /* THE DISAGREEMENT AS A DISTRIBUTION AND NOT AS ITS MAXIMUM (`CLAUDE.md`). Two radiances that
       * differ in their exponent are millions of ulps apart while being physically the same number
       * near zero, so a worst case alone says nothing about whether the panels agree. */
      std::vector<double> spread, relative;
      double fromTotal = 0, toTotal = 0;
      for (int row = 0; row < check.From.Height; ++row) {
        for (int column = 0; column < check.From.Width; ++column) {
          const int fromX = check.From.X + column, fromY = check.From.Y + row;
          const int toX = check.To.X + column, toY = check.To.Y + row;
          if (fromX < 0 || fromY < 0 || toX < 0 || toY < 0 || fromX >= frame.Width ||
              toX >= frame.Width || fromY >= frame.Height || toY >= frame.Height) {
            continue;
          }
          for (int channel = 0; channel < 3; ++channel) {
            if (!check.Channel[channel]) { continue; }
            ++compared;
            const float mine = frame.At(fromX, fromY, channel);
            const float theirs = (float)((double)frame.At(toX, toY, channel) * check.Scale);
            const int64_t off = UlpsBetween(mine, theirs);
            if (off > worst) { worst = off; }
            spread.push_back((double)off);
            const double scale = std::fabs((double)mine) > std::fabs((double)theirs)
                                     ? std::fabs((double)mine)
                                     : std::fabs((double)theirs);
            relative.push_back(scale > 0 ? std::fabs((double)mine - (double)theirs) / scale : 0.0);
            fromTotal += (double)mine;
            toTotal += (double)theirs;
            apart += (double)off > check.MaxUlps ? 1u : 0u;
          }
        }
      }
      metrics.push_back({check.Name + "_channels_apart", (double)apart, 0.0, "channels",
                         Direction::AtMost});
      size_t read = 0;
      for (const bool channel : check.Channel) { read += channel ? 1u : 0u; }
      metrics.push_back({check.Name + "_channels_compared", (double)compared,
                         (double)read * (double)check.From.Width * (double)check.From.Height,
                         "channels", Direction::AtLeast});
      std::sort(spread.begin(), spread.end());
      std::sort(relative.begin(), relative.end());
      /* THE TWO REGIONS AS QUANTITIES, so a disagreement in ulps can be read against the radiances
       * it is a disagreement between -- an ulp count near zero is a large number about nothing. */
      metrics.push_back({check.Name + "_from_mean", compared > 0 ? fromTotal / (double)compared : 0.0,
                         0.0, "linear, scene-referred", Direction::Reported});
      metrics.push_back({check.Name + "_to_mean", compared > 0 ? toTotal / (double)compared : 0.0,
                         0.0, "linear, scene-referred", Direction::Reported});
      metrics.push_back({check.Name + "_p50_relative", Percentile(relative, 0.50), 0.0,
                         "dimensionless", Direction::Reported});
      metrics.push_back({check.Name + "_p95_relative", Percentile(relative, 0.95), 0.0,
                         "dimensionless", Direction::Reported});
      metrics.push_back({check.Name + "_p50_ulps", Percentile(spread, 0.50), 0.0, "f32 ulps",
                         Direction::Reported});
      metrics.push_back({check.Name + "_p95_ulps", Percentile(spread, 0.95), 0.0, "f32 ulps",
                         Direction::Reported});
      metrics.push_back({check.Name + "_p99_ulps", Percentile(spread, 0.99), 0.0, "f32 ulps",
                         Direction::Reported});
      metrics.push_back({check.Name + "_worst_ulps", (double)worst, 0.0, "f32 ulps",
                         Direction::Reported});
      break;
    }
  }
}

} // namespace outshine::Render::Parity
#endif
