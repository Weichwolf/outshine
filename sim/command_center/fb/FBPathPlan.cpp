#include "FBPathPlan.h"
#include "FBTerrainField.h"
#include <cmath>
#include <queue>
#include <unordered_map>

namespace FlightBox {

static const double kPi = 3.14159265358979323846;
static const double kMpd = 111320.0;   /* metres per degree latitude */

FBPathPlan::FBPathPlan(FBTerrainField *field, double centerLat, double centerLon, double fenceRadiusM, unsigned seed)
  : CellM(1500), HeightW(1.5), CaptureM(3000), FencePad(20000), MaxExpand(300000),
    Field(field), CLat(centerLat), CLon(centerLon), FenceM(fenceRadiusM),
    Rng(seed ? seed : 1u), GLat(centerLat), GLon(centerLon), HaveGoal(false), Fixed(false),
    Active(1), ReplanCount(0), LastExpandedN(0) {}

void FBPathPlan::SetFixedGoal(double lat, double lon) {
  GLat = lat; GLon = lon; Fixed = true; HaveGoal = true;
}

/* xorshift32 — deterministic wander for reproducible proof runs (FB_LL_SEED). */
static inline uint32_t NextRng(uint32_t &s) {
  s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s;
}
static inline double Rand01(uint32_t &s) { return (NextRng(s) >> 8) * (1.0 / 16777216.0); }

void FBPathPlan::NewGoal(void) {
  if (Fixed) { HaveGoal = true; return; }
  /* Uniform-ish point in the fence disc (sqrt for area-uniform radius), min 60 km so a goal is a real trip. */
  double ang = Rand01(Rng) * 2.0 * kPi;
  double r = (60000.0 + std::sqrt(Rand01(Rng)) * (FenceM - 60000.0));
  double coslat = std::cos(CLat * kPi / 180.0);
  GLat = CLat + (r * std::cos(ang)) / kMpd;
  GLon = CLon + (r * std::sin(ang)) / (kMpd * (coslat > 1e-3 ? coslat : 1e-3));
  HaveGoal = true;
}

double FBPathPlan::GoalDistM(double lat, double lon) const {
  double coslat = std::cos(CLat * kPi / 180.0);
  double dx = (lon - GLon) * kMpd * coslat, dy = (lat - GLat) * kMpd;
  return std::hypot(dx, dy);
}

/* Perpendicular distance (m) of P from segment AB, in the local ENU metric — for path simplification. */
static double SegDist(double px, double py, double ax, double ay, double bx, double by) {
  double dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
  if (l2 < 1e-6) return std::hypot(px - ax, py - ay);
  double t = ((px - ax) * dx + (py - ay) * dy) / l2;
  if (t < 0) t = 0; else if (t > 1) t = 1;
  return std::hypot(px - (ax + t * dx), py - (ay + t * dy));
}

/* Douglas-Peucker on an ENU polyline; keeps corners within `tol` metres. */
static void Simplify(const std::vector<std::pair<double, double>> &in, int i0, int i1, double tol,
                     std::vector<int> &keep) {
  if (i1 <= i0 + 1) return;
  double maxd = -1; int idx = -1;
  for (int i = i0 + 1; i < i1; i++) {
    double d = SegDist(in[i].first, in[i].second, in[i0].first, in[i0].second, in[i1].first, in[i1].second);
    if (d > maxd) { maxd = d; idx = i; }
  }
  if (maxd > tol && idx > 0) {
    Simplify(in, i0, idx, tol, keep);
    keep.push_back(idx);
    Simplify(in, idx, i1, tol, keep);
  }
}

bool FBPathPlan::Plan(double slat, double slon) {
  if (!Field || !HaveGoal) return false;
  const double coslat = std::cos(slat * kPi / 180.0);
  const double clc = coslat > 1e-3 ? coslat : 1e-3;
  auto cellLL = [&](int ix, int iy, double &la, double &lo) {
    lo = slon + (ix * CellM) / (kMpd * clc);
    la = slat + (iy * CellM) / kMpd;
  };
  /* fence check in the LOCAL frame (metres from home) */
  /* start's offset from HOME (fence centre); a start-local cell (x,y) is (x+homeX, y+homeY) from home. */
  const double homeX = (slon - CLon) * kMpd * clc, homeY = (slat - CLat) * kMpd;
  auto inFence = [&](double x, double y, double &over) {
    double d = std::hypot(x + homeX, y + homeY);
    over = d - (FenceM - FencePad);
    return d <= FenceM;
  };

  double gxm = (GLon - slon) * kMpd * clc, gym = (GLat - slat) * kMpd;
  int gix = (int)std::lround(gxm / CellM), giy = (int)std::lround(gym / CellM);

  auto key = [](int ix, int iy) -> long long { return ((long long)(iy + 500000) << 21) | (long long)(ix + 500000); };
  struct QN { double f; int ix, iy; };
  struct Cmp { bool operator()(const QN &a, const QN &b) const { return a.f > b.f; } };
  std::priority_queue<QN, std::vector<QN>, Cmp> open;
  std::unordered_map<long long, double> g;
  std::unordered_map<long long, long long> from;
  std::unordered_map<long long, float> hcache;   /* one HeightAt per cell (a cell is visited by up to 8 neighbours) */
  g[key(0, 0)] = 0.0;
  open.push({0.0, 0, 0});
  const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
  long long expanded = 0;
  bool reached = false;
  int rix = gix, riy = giy;

  while (!open.empty() && expanded < MaxExpand) {
    QN cur = open.top(); open.pop();
    long long ck = key(cur.ix, cur.iy);
    double gc = g[ck];
    if (cur.f - gc > 1e9) continue;               /* stale */
    if (std::abs(cur.ix - gix) <= 1 && std::abs(cur.iy - giy) <= 1) { reached = true; rix = cur.ix; riy = cur.iy; break; }
    expanded++;
    for (int k = 0; k < 8; k++) {
      int nix = cur.ix + DX[k], niy = cur.iy + DY[k];
      double nx = nix * CellM, ny = niy * CellM, over;
      if (!inFence(nx, ny, over)) continue;
      double step = (DX[k] && DY[k]) ? CellM * 1.41421356 : CellM;
      long long nk = key(nix, niy);
      float h;
      auto hit = hcache.find(nk);
      if (hit != hcache.end()) h = hit->second;
      else { double la, lo; cellLL(nix, niy, la, lo); double hh = Field->HeightAt(la, lo); if (hh < 0) hh = 0; h = (float)hh; hcache[nk] = h; }
      double cost = step * (1.0 + HeightW * h / 1000.0);
      if (over > 0) cost += step * 50.0;          /* soft fence: strongly discourage the outer pad ring */
      double ng = gc + cost;
      auto it = g.find(nk);
      if (it == g.end() || ng < it->second) {
        g[nk] = ng;
        from[nk] = ck;
        double hx = std::abs(nix - gix), hy = std::abs(niy - giy);
        double heur = std::hypot(hx, hy) * CellM;   /* admissible: min edge cost/m == 1 */
        open.push({ng + heur, nix, niy});
      }
    }
  }
  LastExpandedN = expanded;
  if (!reached && from.empty()) return false;

  /* Reconstruct cells goal->start, then to lat/lon, then simplify to flyable waypoints. */
  std::vector<std::pair<double, double>> raw;   /* ENU metres, start..goal order after reverse */
  long long ck = key(rix, riy);
  std::vector<std::pair<int, int>> cells;
  cells.push_back({rix, riy});
  while (from.count(ck)) {
    long long pk = from[ck];
    int pix = (int)((pk & 0x1FFFFF) - 500000);
    int piy = (int)(((pk >> 21) & 0x1FFFFF) - 500000);
    cells.push_back({pix, piy});
    ck = pk;
    if (cells.size() > 100000) break;
  }
  for (auto it = cells.rbegin(); it != cells.rend(); ++it)
    raw.push_back({(double)it->first * CellM, (double)it->second * CellM});
  raw.push_back({gxm, gym});   /* the true goal (not snapped) as the final point */

  std::vector<int> keep;
  keep.push_back(0);
  Simplify(raw, 0, (int)raw.size() - 1, CellM * 0.9, keep);
  keep.push_back((int)raw.size() - 1);

  Route.clear();
  for (int i : keep) {
    double la = slat + raw[i].second / kMpd, lo = slon + raw[i].first / (kMpd * clc);
    Route.push_back({la, lo});
  }
  Active = 1;
  ReplanCount++;
  return Route.size() >= 2;
}

void FBPathPlan::Update(double lat, double lon) {
  if (!HaveGoal) { NewGoal(); Plan(lat, lon); return; }
  /* Reached the goal? -> pick + plan the next one (perpetual wander). A FIXED proof goal just holds
   * (no replan spam once arrived). */
  if (GoalDistM(lat, lon) < CaptureM * 2.0) {
    if (Fixed) return;
    NewGoal();
    Plan(lat, lon);
    return;
  }
  if (!HasRoute()) { Plan(lat, lon); return; }
  /* Advance the active waypoint once captured. */
  double coslat = std::cos(lat * kPi / 180.0), clc = coslat > 1e-3 ? coslat : 1e-3;
  while (Active < (int)Route.size() - 1) {
    double dx = (Route[Active].second - lon) * kMpd * clc, dy = (Route[Active].first - lat) * kMpd;
    if (std::hypot(dx, dy) < CaptureM) Active++; else break;
  }
}

double FBPathPlan::DesiredTrackDeg(double lat, double lon) const {
  if (!HasRoute()) {   /* no plan yet -> steer straight at the goal */
    double coslat = std::cos(lat * kPi / 180.0), clc = coslat > 1e-3 ? coslat : 1e-3;
    double e = (GLon - lon) * kMpd * clc, n = (GLat - lat) * kMpd;
    return std::atan2(e, n) * 180.0 / kPi;
  }
  int a = Active < (int)Route.size() ? Active : (int)Route.size() - 1;
  double coslat = std::cos(lat * kPi / 180.0), clc = coslat > 1e-3 ? coslat : 1e-3;
  double e = (Route[a].second - lon) * kMpd * clc, n = (Route[a].first - lat) * kMpd;
  return std::atan2(e, n) * 180.0 / kPi;   /* pure-pursuit bearing to the active waypoint (deg, 0=N,90=E) */
}

} // namespace FlightBox
