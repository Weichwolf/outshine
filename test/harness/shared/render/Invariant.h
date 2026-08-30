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

enum class InvariantKind { HueOfBrightest, RegionCompare };

struct PixelRect {
  int X = 0, Y = 0, Width = 0, Height = 0;
};

struct Invariant {
  InvariantKind Kind = InvariantKind::HueOfBrightest;
  std::string Name;
  double Rgb[3] = {0, 0, 0};
  double Fraction = 0;
  double Tolerance = 0;
  PixelRect From, To;
  double Scale = 1.0;
  double MaxUlps = 0;
  double MaxP95Relative = 0;

  enum class Currency { Ulps, P95Relative };
  Currency Judged = Currency::Ulps;

  bool Channel[3] = {true, true, true};
};

[[nodiscard]] inline bool
ReadInvariantKind(const std::string &spelling, InvariantKind &out, std::string &error) {
  if (spelling == "hue-of-brightest") {
    out = InvariantKind::HueOfBrightest;
    return true;
  }
  if (spelling == "region-compare") {
    out = InvariantKind::RegionCompare;
    return true;
  }
  error =
      "statedInvariants[].kind '" + spelling + "' is neither hue-of-brightest nor region-compare";
  return false;
}

[[nodiscard]] inline bool
ReadTriple(const Json::Ref &entry, const char *name, double out[3], std::string &error) {
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

[[nodiscard]] inline bool
ReadRect(const Json::Ref &entry, const char *name, PixelRect &out, std::string &error) {
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

[[nodiscard]] inline bool
ReadInvariants(const Json::Ref &declared, std::vector<Invariant> &out, std::string &error) {
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
      case InvariantKind::HueOfBrightest:
        if (!ReadTriple(entry["hue"], (where + ".hue").c_str(), check.Rgb, error)) { return false; }
        if (!ReadDeclaredNumber(entry["brightestFraction"],
                                (where + ".brightestFraction").c_str(),
                                check.Fraction,
                                error) ||
            !ReadDeclaredNumber(
                entry["maxHueError"], (where + ".maxHueError").c_str(), check.Tolerance, error)) {
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
        if (!ReadDeclaredNumber(entry["scale"], (where + ".scale").c_str(), check.Scale, error)) {
          return false;
        }
        {
          const bool ulps = entry["maxUlps"].Valid();
          const bool quantile = entry["maxP95Relative"].Valid();
          if (ulps == quantile) {
            error = where + " declares " +
                    (ulps ? "both maxUlps and maxP95Relative"
                          : "neither "
                            "maxUlps nor maxP95Relative") +
                    ", and one comparison is judged in one currency";
            return false;
          }
          check.Judged = ulps ? Invariant::Currency::Ulps : Invariant::Currency::P95Relative;
          if (ulps && !ReadDeclaredNumber(
                          entry["maxUlps"], (where + ".maxUlps").c_str(), check.MaxUlps, error)) {
            return false;
          }
          if (quantile && !ReadDeclaredNumber(entry["maxP95Relative"],
                                              (where + ".maxP95Relative").c_str(),
                                              check.MaxP95Relative,
                                              error)) {
            return false;
          }
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

struct LinearFrame {
  const std::vector<float> *Samples = nullptr;
  int Width = 0;
  int Height = 0;

  [[nodiscard]] bool Holds() const {
    return Samples != nullptr && Samples->size() >= (size_t)Width * (size_t)Height * 4u &&
           Width > 0 && Height > 0;
  }

  [[nodiscard]] float At(int x, int y, int channel) const {
    return (*Samples)[((size_t)y * (size_t)Width + (size_t)x) * 4u + (size_t)channel];
  }

  [[nodiscard]] bool Covered(int x, int y) const { return At(x, y, 3) > 0.0f; }
};

inline int64_t UlpsBetween(float a, float b) {
  int32_t left = 0, right = 0;
  std::memcpy(&left, &a, sizeof left);
  std::memcpy(&right, &b, sizeof right);
  return left > right ? (int64_t)left - (int64_t)right : (int64_t)right - (int64_t)left;
}

inline void
Evaluate(const Invariant &check, const LinearFrame &frame, std::vector<Metric> &metrics) {
  switch (check.Kind) {
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
          const double sum =
              (double)frame.At(x, y, 0) + (double)frame.At(x, y, 1) + (double)frame.At(x, y, 2);
          if (!(sum >= floorSum) || !(sum > 0)) { continue; }
          ++judged;
          bool off = false;
          for (int channel = 0; channel < 3; ++channel) {
            const double error =
                std::fabs((double)frame.At(x, y, channel) / sum - check.Rgb[channel]);
            if (error > worst) { worst = error; }
            off = off || error > check.Tolerance;
          }
          missed += off ? 1u : 0u;
        }
      }
      metrics.push_back(
          {check.Name + "_samples_off_hue", (double)missed, 0.0, "px", Direction::AtMost});
      metrics.push_back(
          {check.Name + "_samples_judged", (double)judged, 1.0, "px", Direction::AtLeast});
      metrics.push_back(
          {check.Name + "_worst_hue_error", worst, 0.0, "dimensionless", Direction::Reported});
      break;
    }
    case InvariantKind::RegionCompare: {
      size_t apart = 0, compared = 0;
      int64_t worst = 0;

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

      metrics.push_back(
          {check.Name + "_channels_apart",
           (double)apart,
           0.0,
           "channels",
           check.Judged == Invariant::Currency::Ulps ? Direction::AtMost : Direction::Reported});
      size_t read = 0;
      for (const bool channel : check.Channel) { read += channel ? 1u : 0u; }
      metrics.push_back({check.Name + "_channels_compared",
                         (double)compared,
                         (double)read * (double)check.From.Width * (double)check.From.Height,
                         "channels",
                         Direction::AtLeast});
      std::sort(spread.begin(), spread.end());
      std::sort(relative.begin(), relative.end());

      metrics.push_back({check.Name + "_from_mean",
                         compared > 0 ? fromTotal / (double)compared : 0.0,
                         0.0,
                         "linear, scene-referred",
                         Direction::Reported});
      metrics.push_back({check.Name + "_to_mean",
                         compared > 0 ? toTotal / (double)compared : 0.0,
                         0.0,
                         "linear, scene-referred",
                         Direction::Reported});
      metrics.push_back({check.Name + "_p50_relative",
                         Percentile(relative, 0.50),
                         0.0,
                         "dimensionless",
                         Direction::Reported});
      metrics.push_back({check.Name + "_p95_relative",
                         Percentile(relative, 0.95),
                         check.MaxP95Relative,
                         "dimensionless",
                         check.Judged == Invariant::Currency::P95Relative ? Direction::AtMost
                                                                          : Direction::Reported});
      metrics.push_back({check.Name + "_p50_ulps",
                         Percentile(spread, 0.50),
                         0.0,
                         "f32 ulps",
                         Direction::Reported});
      metrics.push_back({check.Name + "_p95_ulps",
                         Percentile(spread, 0.95),
                         0.0,
                         "f32 ulps",
                         Direction::Reported});
      metrics.push_back({check.Name + "_p99_ulps",
                         Percentile(spread, 0.99),
                         0.0,
                         "f32 ulps",
                         Direction::Reported});
      metrics.push_back(
          {check.Name + "_worst_ulps", (double)worst, 0.0, "f32 ulps", Direction::Reported});
      break;
    }
  }
}

} // namespace outshine::Render::Parity
#endif
