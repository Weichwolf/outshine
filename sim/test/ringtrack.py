#!/usr/bin/env python3
"""Ring-track generator + validator — the rigorous mission test.

A mission is FEW parameters (takeoff runway, waypoints AGL, landing runway; optional speeds). From those
this computes the dense DIRECTED ring track the aircraft must thread, using iNav's OWN navigation
mathematics so the rings ARE the path iNav is expected to fly:

  * takeoff climb-out — a corridor of rings rising at nav_fw_climb_angle along the departure runway heading
  * enroute — a Catmull-Rom spline through the waypoints, rounded at the coordinated turn radius
    R = V^2 / (g * tan(nav_fw_bank_angle)) (V = fw_reference_airspeed), rings every fixed spacing along it
  * landing approach — a glideslope of rings descending at the approach angle, aligned with the landing
    runway centreline into the threshold

Every ring is a directed hoop (centre lat/lon/ASL, plane normal = local flight tangent, radius). "Passing"
means the flown track pierces the ring plane from the correct side within the radius. >=100 rings total.

Same module drives the E2E validation (test/e2e.py) and the CC route rendering (spline + rings).
"""
import math, re
from pathlib import Path

G = 9.80665
ROOT = Path(__file__).resolve().parents[1]

def _clat(lat): return math.cos(math.radians(lat))
def enu_off(lat, lon, clat0, clon0):
    return (lon-clon0)*111320.0*_clat(clat0), (lat-clat0)*111320.0
def off_ll(clat0, clon0, e, n):
    return clat0 + n/111320.0, clon0 + e/(111320.0*_clat(clat0))
def bearing(a, b, c, d):
    y = math.sin(math.radians(d-b))*math.cos(math.radians(c))
    x = math.cos(math.radians(a))*math.sin(math.radians(c)) - math.sin(math.radians(a))*math.cos(math.radians(c))*math.cos(math.radians(d-b))
    return math.degrees(math.atan2(y, x)) % 360
def dest(lat, lon, brg_deg, dist_m):
    br = math.radians(brg_deg)
    return off_ll(lat, lon, math.sin(br)*dist_m, math.cos(br)*dist_m)
def dist_ll(a, b):
    e, n = enu_off(b[0], b[1], a[0], a[1]); return math.hypot(e, n)

def inav_params(ac):
    """Read the aircraft's nav math from its inav.diff (falling back to iNav defaults). These are the same
    numbers iNav flies with, so the generated corridor is what iNav itself is expected to produce."""
    p = {"climb_angle":6.0, "dive_angle":6.0, "bank_angle":20.0, "cruise":15.0,
         "approach_len":350.0, "glide_angle":3.0}
    f = ROOT/"aircraft"/"models"/ac/"inav.diff"
    if f.exists():
        t = f.read_text()
        def g(name, d, scale=1.0):
            m = re.search(rf"set {name}\s*=\s*(-?\d+(?:\.\d+)?)", t)
            return float(m.group(1))*scale if m else d
        p["climb_angle"] = g("nav_fw_climb_angle", p["climb_angle"])
        p["dive_angle"]  = g("nav_fw_dive_angle",  p["dive_angle"])
        p["bank_angle"]  = g("nav_fw_bank_angle",  p["bank_angle"])
        p["cruise"]      = g("fw_reference_airspeed", p["cruise"]*100)/100.0
        p["approach_len"]= g("nav_fw_land_approach_length", p["approach_len"]*100)/100.0
    return p

def turn_radius(V, bank_deg):
    return V*V / (G*math.tan(math.radians(max(5.0, bank_deg))))

def catmull_rom(ctrl, per_seg):
    """Sample a centripetal-ish Catmull-Rom spline through ctrl points [(x,y,z)...]; returns dense samples."""
    if len(ctrl) < 2: return list(ctrl)
    pts = [ctrl[0]] + list(ctrl) + [ctrl[-1]]
    out = []
    for i in range(1, len(pts)-2):
        p0, p1, p2, p3 = pts[i-1], pts[i], pts[i+1], pts[i+2]
        for s in range(per_seg):
            t = s/per_seg; t2 = t*t; t3 = t2*t
            c0 = -0.5*t3 + t2 - 0.5*t; c1 = 1.5*t3 - 2.5*t2 + 1.0
            c2 = -1.5*t3 + 2.0*t2 + 0.5*t; c3 = 0.5*t3 - 0.5*t2
            out.append(tuple(c0*p0[k]+c1*p1[k]+c2*p2[k]+c3*p3[k] for k in range(3)))
    out.append(ctrl[-1])
    return out

def build_track(R, ac, ring_radius=150.0, spacing=None, n_climb=12, n_approach=12):
    """R = resolved mission (test/mission.resolve). Returns an ordered list of directed rings:
    dicts {lat, lon, asl, brg, r, kind}. Kind in {climb, route, approach}. >=100 rings for a real mission."""
    P = inav_params(ac)
    to, ld, wps = R["takeoff"], R["land"], R["waypoints"]
    V = P["cruise"]; Rturn = turn_radius(V, P["bank_angle"])
    if spacing is None: spacing = max(120.0, Rturn*0.9)      # ring pitch ~ turn radius -> corridor resolves turns
    to_elev = to["elev_m"]
    rings = []

    # --- takeoff climb-out: rise at nav_fw_climb_angle along the departure runway heading ---
    tan_c = math.tan(math.radians(P["climb_angle"]))
    wp1_asl = to_elev + wps[0]["alt_rel"]
    climb_len = max(spacing*n_climb, (wp1_asl-to_elev)/max(tan_c, 0.02))
    for i in range(1, n_climb+1):
        d = climb_len*i/n_climb
        la, lo = dest(to["lat"], to["lon"], to["heading_deg"], d)
        rings.append({"lat":la,"lon":lo,"asl":to_elev + d*tan_c,"brg":to["heading_deg"],"r":ring_radius,"kind":"climb"})

    # --- enroute: STRAIGHT legs between route points, joined at each waypoint. iNav's fixed-wing WP nav
    # flies a straight line to each waypoint then turns — NOT a spline — so the rings sit on the straight
    # legs (oriented on the leg bearing) where the aircraft actually is. A spline bulges outside the legs
    # on turns and the rings there miss the real path. Route points: climb-out end -> WPs -> approach start.
    app_alt = ld["elev_m"] + P["approach_len"]*math.tan(math.radians(P["glide_angle"]))
    app_lat, app_lon = dest(ld["lat"], ld["lon"], (ld["heading_deg"]+180)%360, P["approach_len"])
    rpts = [(rings[-1]["lat"], rings[-1]["lon"], rings[-1]["asl"])]
    for w in wps: rpts.append((w["lat"], w["lon"], to_elev + w["alt_rel"]))
    rpts.append((app_lat, app_lon, app_alt))
    # dense enough that the whole track is >=100 rings even for a short circuit (spec: a real test has >=100)
    length = sum(dist_ll(rpts[i], rpts[i+1]) for i in range(len(rpts)-1))
    # +len(rpts) margin compensates the per-leg int() flooring below so the whole track clears >=100 rings
    need_route = max(1, 100 - n_climb - n_approach) + len(rpts)
    rspacing = min(spacing, length/need_route)
    for i in range(len(rpts)-1):
        (la0,lo0,z0),(la1,lo1,z1) = rpts[i], rpts[i+1]
        seg = dist_ll(rpts[i], rpts[i+1]); br = bearing(la0,lo0,la1,lo1)
        k = max(1, int(seg/rspacing))
        for j in range(k):
            f = j/k
            rings.append({"lat":la0+(la1-la0)*f, "lon":lo0+(lo1-lo0)*f, "asl":z0+(z1-z0)*f,
                          "brg":br, "r":ring_radius, "kind":"route"})

    # --- landing approach: glideslope of rings on the landing runway centreline into the threshold ---
    tan_g = math.tan(math.radians(P["glide_angle"]))
    for i in range(n_approach, 0, -1):
        d = P["approach_len"]*i/n_approach
        la, lo = dest(ld["lat"], ld["lon"], (ld["heading_deg"]+180)%360, d)
        rings.append({"lat":la,"lon":lo,"asl":ld["elev_m"] + d*tan_g,"brg":ld["heading_deg"],"r":ring_radius,"kind":"approach"})
    return rings

def ring_passed(tj, ring):
    """Track tj=[(lat,lon,asl)] threaded the directed ring? Forward pierce of the ring plane within radius."""
    clat, clon, casl, brg, rad = ring["lat"], ring["lon"], ring["asl"], ring["brg"], ring["r"]
    br = math.radians(brg); ne, nn = math.sin(br), math.cos(br)
    prev = None
    for (lat, lon, asl) in tj:
        e, n = enu_off(lat, lon, clat, clon); u = asl-casl; s = e*ne + n*nn
        if prev is not None:
            pe, pn, pu, ps = prev
            if ps < 0 and s >= 0:                              # forward pierce only
                t = ps/(ps-s)
                ce = pe+t*(e-pe); cn = pn+t*(n-pn); cu = pu+t*(u-pu)
                ct = ce*(-nn) + cn*ne                          # cross-track in the ring plane
                if math.hypot(ct, cu) < rad: return True
        prev = (e, n, u, s)
    return False

def pierce_index(tj, ring, start=0):
    """Index into tj of the first forward pierce of `ring` within radius, at or after `start`; else None."""
    clat, clon, casl, brg, rad = ring["lat"], ring["lon"], ring["asl"], ring["brg"], ring["r"]
    br = math.radians(brg); ne, nn = math.sin(br), math.cos(br)
    prev = None
    for idx in range(start, len(tj)):
        lat, lon, asl = tj[idx]
        e, n = enu_off(lat, lon, clat, clon); u = asl-casl; s = e*ne + n*nn
        if prev is not None:
            pe, pn, pu, ps = prev
            if ps < 0 and s >= 0:
                t = ps/(ps-s)
                ct = (pe+t*(e-pe))*(-nn) + (pn+t*(n-pn))*ne; cu = pu+t*(u-pu)
                if math.hypot(ct, cu) < rad: return idx
        prev = (e, n, u, s)
    return None

def score(tj, rings):
    """Returns (threaded, inorder): rings the track pierced at all, and the longest run flown IN SEQUENCE
    (each ring pierced strictly after the previous one's pierce — the real 'flew the route' number)."""
    threaded = sum(1 for rg in rings if pierce_index(tj, rg) is not None)
    inorder = 0; cur = 0; i = 0
    for rg in rings:
        j = pierce_index(tj, rg, i)
        if j is not None: cur += 1; i = j; inorder = max(inorder, cur)
        else: cur = 0
    return threaded, inorder
