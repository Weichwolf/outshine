#!/usr/bin/env python3
"""gen_air_decks — ONE recipe, ten JSBSim decks, from the eight published anchors per catalogue row.

This file IS doc/modules/air/flight-model-recipe.md, executable. Every deck under
sim/assets/aircraft/<row>/ is generated from the ANCHORS table below and nothing else; a hand edit of a
generated deck is a defect, because the next run of this tool erases it.

    tools/gen_air_decks.py [--out assets/aircraft] [--check]

--check regenerates into a temporary directory and diffs, so a build gate can assert that the committed
decks are exactly what the recipe produces.

THE RULE THE WHOLE THING HANGS ON (recipe §0): the anchors constrain the PRODUCT T/D, never T and D
separately. The thrust deck is fixed by the analogy plus the published statics and is then FROZEN; all
residual error is absorbed by the drag polar. If that forces an implausible CD0, the THRUST analogy is
what is wrong -- and that is only diagnosable because the rule was written down first.

WHERE THE RECIPE DID NOT CARRY, found while building and recorded here rather than papered over:
A4 (rate of climb) CANNOT invert a subsonic CD0 for a single row in the catalogue. Worked at the [SET]
best-climb condition M 0.9 at sea level in full afterburner, the published maximum climb rates imply a
drag that is negative (f15c, mig23) or an order of magnitude below any real aircraft (f5e: 0.0038
against its own published 0.0200). The published figures are zoom/optimum-condition numbers, not
steady-state Ps. So A4 is demoted to a PROBE beside A3, and the subsonic level is taken exactly the way
`e` is taken in recipe §4.1 -- from the ONE row in the catalogue that publishes it, generalised with
the generalisation declared. Rows whose A2 anchor is itself subsonic (mig17, su7) invert their own.
"""

import argparse
import json
import math
import os
import shutil
import subprocess
import sys
import tempfile

SIM_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# --------------------------------------------------------------------------------------------------
# Units and the standard atmosphere. ISA to 20 km, which covers every ceiling in the catalogue.
# --------------------------------------------------------------------------------------------------
KG_LB = 2.2046226
M_FT = 3.2808399
M2_FT2 = 10.7639104
KGM2_SLUGFT2 = 0.7375621
G0 = 9.80665
KMH_MS = 1.0 / 3.6


def isa(h):
    """(density kg/m^3, speed of sound m/s) at geometric altitude h (m)."""
    if h < 11000.0:
        t = 288.15 - 0.0065 * h
        p = 101325.0 * (t / 288.15) ** 5.255877
    elif h < 20000.0:
        t = 216.65
        p = 22632.06 * math.exp(-G0 * (h - 11000.0) / (287.053 * t))
    else:
        t = 216.65 + 0.001 * (h - 20000.0)
        p = 5474.89 * (t / 216.65) ** (-G0 / (0.001 * 287.053))
    rho = p / (287.053 * t)
    return rho, math.sqrt(1.4 * 287.053 * t)


def sigma(h):
    return isa(h)[0] / 1.225


# --------------------------------------------------------------------------------------------------
# §5 PROPULSION -- two analogies, one per family, both DIMENSIONLESS and normalised to 1.0 at M0/SL.
# --------------------------------------------------------------------------------------------------
# The TURBOFAN analogy: the pinned F-16 model's F100-PW-229 surfaces, exactly as the MiG-29 deck already
# borrows them, with the 60k/70k columns continued by the ISA density ratio instead of the pinned deck's
# zero column ("a wall, not a decay", doc/modules/f16/flight-model.md §12.3).
TF_ALT = (-10000, 0, 10000, 20000, 30000, 40000, 50000, 60000, 70000)
TF_IDLE = (
    (0.0, (0.0430, 0.0488, 0.0528, 0.0694, 0.0899, 0.1183, 0.1467, 0.0908, 0.0508)),
    (0.2, (0.0500, 0.0501, 0.0335, 0.0544, 0.0797, 0.1049, 0.1342, 0.0831, 0.0464)),
    (0.4, (0.0040, 0.0047, 0.0020, 0.0272, 0.0595, 0.0891, 0.1203, 0.0745, 0.0416)),
    (0.6, (-0.0804, -0.0804, -0.0560, -0.0237, 0.0276, 0.0718, 0.1073, 0.0664, 0.0371)),
    (0.8, (-0.2129, -0.2129, -0.1498, -0.1025, 0.0474, 0.0868, 0.0900, 0.0557, 0.0311)),
    (1.0, (-0.2839, -0.2839, -0.1104, -0.0469, -0.0270, 0.0552, 0.0800, 0.0495, 0.0277)),
)
TF_MIL = (
    (0.0, (1.2600, 1.0000, 0.7400, 0.5340, 0.3720, 0.2410, 0.1490, 0.0922, 0.0515)),
    (0.2, (1.1710, 0.9340, 0.6970, 0.5060, 0.3550, 0.2310, 0.1430, 0.0885, 0.0495)),
    (0.4, (1.1500, 0.9210, 0.6920, 0.5060, 0.3570, 0.2330, 0.1450, 0.0898, 0.0502)),
    (0.6, (1.1810, 0.9510, 0.7210, 0.5320, 0.3780, 0.2480, 0.1540, 0.0953, 0.0533)),
    (0.8, (1.2580, 1.0200, 0.7820, 0.5820, 0.4170, 0.2750, 0.1700, 0.1052, 0.0588)),
    (1.0, (1.3690, 1.1200, 0.8710, 0.6510, 0.4750, 0.3150, 0.1950, 0.1207, 0.0675)),
    (1.2, (1.4850, 1.2300, 0.9750, 0.7440, 0.5450, 0.3640, 0.2250, 0.1393, 0.0779)),
    (1.4, (1.5941, 1.3400, 1.0860, 0.8450, 0.6280, 0.4240, 0.2630, 0.1628, 0.0910)),
)
TF_AUG = (
    (0.0, (1.1816, 1.0000, 0.8184, 0.6627, 0.5280, 0.3756, 0.2327, 0.1440, 0.0805)),
    (0.2, (1.1308, 0.9599, 0.7890, 0.6406, 0.5116, 0.3645, 0.2258, 0.1398, 0.0781)),
    (0.4, (1.1150, 0.9474, 0.7798, 0.6340, 0.5070, 0.3615, 0.2240, 0.1386, 0.0775)),
    (0.6, (1.1284, 0.9589, 0.7894, 0.6420, 0.5134, 0.3661, 0.2268, 0.1404, 0.0785)),
    (0.8, (1.1707, 0.9942, 0.8177, 0.6647, 0.5309, 0.3784, 0.2345, 0.1452, 0.0811)),
    (1.0, (1.2411, 1.0529, 0.8648, 0.7017, 0.5596, 0.3983, 0.2467, 0.1527, 0.0854)),
    (1.2, (1.3287, 1.1254, 0.9221, 0.7462, 0.5936, 0.4219, 0.2614, 0.1618, 0.0904)),
    (1.4, (1.4365, 1.2149, 0.9933, 0.8021, 0.6360, 0.4509, 0.2794, 0.1729, 0.0967)),
    (1.6, (1.5711, 1.3260, 1.0809, 0.8700, 0.6874, 0.4860, 0.3011, 0.1864, 0.1042)),
    (1.8, (1.7301, 1.4579, 1.1857, 0.9512, 0.7495, 0.5289, 0.3277, 0.2028, 0.1134)),
    (2.0, (1.8314, 1.5700, 1.3086, 1.0474, 0.8216, 0.5786, 0.3585, 0.2219, 0.1240)),
    (2.2, (1.9700, 1.6900, 1.4100, 1.2400, 0.9100, 0.6359, 0.3940, 0.2439, 0.1363)),
    (2.4, (2.0700, 1.8000, 1.5300, 1.3400, 1.0000, 0.7200, 0.4600, 0.2847, 0.1592)),
    (2.6, (2.2000, 1.9200, 1.6400, 1.4400, 1.1000, 0.8000, 0.5200, 0.3219, 0.1799)),
)

# THE TURBOJET REFERENCE SURFACE -- the recipe's one genuine construction job, built once here and
# reused by eight rows. It is NOT a second borrowed deck: no public turbojet deck exists in the tree, so
# the surface is CONSTRUCTED from two physical statements and one calibration.
#
#   1. ALTITUDE LAPSE [DERIVED]. A turbojet's thrust at fixed corrected speed follows the inlet mass
#      flow, i.e. the density, with a small temperature correction: T/T0 = sigma^n. n = 0.90 dry and
#      n = 0.85 augmented [SET within the textbook 0.7-1.0 band]; the augmented exponent is the smaller
#      one because the afterburner adds heat that does not lapse with the compressor.
#   2. RAM RECOVERY [DERIVED]. Net thrust rises with flight Mach because the inlet compresses:
#      the mass flow grows as (1 + 0.2 M^2)^2.5 while the exhaust-minus-flight velocity difference
#      falls. The product is the classic afterburning-turbojet ram curve, evaluated below and then
#      SCALED BY ONE NUMBER, kRam, which is the surface's single free parameter.
#   3. CALIBRATION [recipe §5]: kRam is set on `mig21`, the densest turbojet anchor set in the
#      catalogue, so that the row's TWO Vmax anchors are simultaneously reachable with a drag polar
#      whose subsonic level equals the catalogue's one published CD0. kRam = 1.00 came out of that and
#      is kept as the reference; the residual is absorbed by the polar per §0.
#
# The zero column of the borrowed turbofan deck is NOT reproduced: this surface decays to 70 000 ft, so
# a ceiling is approached asymptotically rather than hit as a wall.
TJ_MACH_DRY = (0.0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0)
TJ_MACH_AUG = (0.0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 2.2, 2.4, 2.6, 2.8, 3.2)
TJ_RAM_SCALE = 1.00   # calibrated on mig21, see 3. above


def tj_ram(m, augmented, alt_m=0.0):
    """Ram factor, 1.0 at M0. Mass flow (1+0.2M^2)^2.5 against the falling velocity margin.

    Ve is the effective jet velocity: 1300 m/s augmented, 700 m/s dry [SET, the fully-expanded exit
    velocities an afterburning turbojet of this generation runs at]. F = mdot*(Ve - V), so
        F(M)/F(0) = (1+0.2M^2)^2.5 * (Ve - M*a) / Ve
    with a the LOCAL speed of sound, so the same Mach buys less ram in the cold stratosphere than at
    sea level -- which is the physically right statement and the reason the surface is generated on the
    same (Mach, density-altitude) grid JSBSim reads. The 2.5 exponent is the isentropic mass-flow growth
    of a choked inlet at constant corrected speed, not a fit. The augmented Ve also sets where this
    surface DIES, and how little margin `mig25`'s M 3.1 anchor has inside it is recipe R2."""
    ve = 1300.0 if augmented else 700.0
    v = m * isa(max(0.0, alt_m))[1]
    if v >= ve:
        return 0.0
    return TJ_RAM_SCALE * (1.0 + 0.2 * m * m) ** 2.5 * (ve - v) / ve


def tj_surface(machs, augmented, idle=False):
    """[(mach, (factor per altitude column))] on the turbofan deck's own altitude grid, so both
    families are read by the same JSBSim table machinery."""
    n = 0.85 if augmented else 0.90
    rows = []
    for m in machs:
        cols = []
        for aft in TF_ALT:
            s = sigma(max(0.0, aft / M_FT))
            if aft < 0:
                s = sigma(0.0) * (1.0 + (-aft / M_FT) / 8500.0)   # below sea level: exponential
            f = tj_ram(m, augmented, max(0.0, aft / M_FT)) * s ** n
            if idle:
                # Idle is not a scaled reference: an idling engine at speed produces net DRAG, exactly
                # as the borrowed deck's negative entries do. Taken as 4 % of military minus the ram
                # drag of the same mass flow -- the shape, not a second surface.
                f = 0.04 * f - 0.30 * m * m * s ** n
            cols.append(f)
        rows.append((m, tuple(cols)))
    return tuple(rows)


# --------------------------------------------------------------------------------------------------
# §4 THE DRAG POLAR
# --------------------------------------------------------------------------------------------------
# e = 0.66 [DERIVED, recipe §4.1] from the F-5E's published CD0 = 0.0200 and (L/D)max = 10.0, the only
# row in the catalogue publishing the pair. One aircraft's number, generalised to nine others, and the
# first term the attribution band sweeps.
OSWALD_E = 0.66
# The three further attribution scalars, 1.0 = the recipe as written (module.md §Spec 11 instrument 2).
PERTURB_CD0 = 1.0
PERTURB_IXX = 1.0
PERTURB_THRUST = 1.0
# The catalogue's ONE published subsonic zero-lift drag coefficient [T4, F-5E], generalised exactly as
# `e` above is, and for the same stated reason. Rows whose own A2 anchor is subsonic invert their own
# and do not use this.
CD0_SUB_REF = 0.0200
# THE TRANSONIC RISE SHAPE [ANALOGY] -- the MiG-29 deck's own inverted CD0(M) table, normalised to its
# subsonic level. Only the MAGNITUDE is fitted per row (recipe §4.2); three inverted points cannot
# define a rise, and pretending otherwise is the failure the method exists to prevent.
CD0_SHAPE = ((0.00, 1.000), (0.60, 1.000), (0.85, 1.047), (0.95, 1.535), (1.05, 2.116),
             (1.22, 2.121), (1.60, 1.930), (2.00, 1.791), (2.40, 1.721), (3.20, 1.700))


def lerp_table(tbl, x):
    if x <= tbl[0][0]:
        return tbl[0][1]
    if x >= tbl[-1][0]:
        return tbl[-1][1]
    for (x0, y0), (x1, y1) in zip(tbl, tbl[1:]):
        if x <= x1:
            return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
    return tbl[-1][1]


# --------------------------------------------------------------------------------------------------
# THE ANCHORS. Every number is doc/modules/air/catalogue.md's, with that file's tier. Nothing is
# computed here that the catalogue states, and nothing is stated here that the catalogue does not.
# --------------------------------------------------------------------------------------------------
class Row:
    def __init__(self, key, name, family, engine, engines, mil_kn, ab_kn,
                 s_m2, b_m, l_m, empty_kg, gross_kg, mtow_kg, fuel_kg,
                 a1_mach, a1_alt_m, a2_mach, a3_ceil_m, a4_roc_ms, a5_g,
                 clmax_v_ms=0.0, clmax_mass_kg=0.0, alpha_lim_deg=0.0, note=""):
        self.__dict__.update(locals())
        del self.__dict__["self"]
        self.ar = b_m * b_m / s_m2
        self.k = 1.0 / (math.pi * self.ar * OSWALD_E)
        # The mass the envelope anchors are flown at: the published GROSS (clean take-off) weight, which
        # is the loading every Vmax/ceiling/climb figure in these sources is quoted at.
        self.w_n = gross_kg * G0

    def thrust_n(self, mach, alt_m, augmented=True):
        """The FROZEN half of the T/D product, N, at (M, h), from the family analogy + the statics.
        A7 is published PER ENGINE, so the installed thrust is the row's own engine count times it.

        IT READS THE DECK'S OWN TABLE, not the closed form behind it. The turbojet reference is a
        continuous ram curve, but what the aeroplane flies is that curve SAMPLED on the JSBSim grid and
        interpolated linearly between the samples -- and the ram curve is convex there, so the chord
        sits BELOW it: -1.1 % at M 1.66, -1.6 % at M 2.0. An inversion against the closed form gives the
        deck a CD0 for a thrust it does not have, and at the anchor the two curves are tangent by
        construction, so 1 % of thrust is 5 % of Vmax. Measured on su7: M 1.66 against an anchor of
        1.74 with a polar that matched the deck's own drag to 0.5 %."""
        static = (self.ab_kn if augmented else self.mil_kn) * 1000.0 * self.engines
        if self.family == "turbofan":
            tbl = TF_AUG if augmented else TF_MIL
        else:
            tbl = (tj_surface(TJ_MACH_AUG, augmented=True) if augmented
                   else tj_surface(TJ_MACH_DRY, augmented=False))
        return static * bilinear(tbl, TF_ALT, mach, alt_m * M_FT)

    def invert_cd0(self, mach, alt_m):
        """Recipe §4.2: at Vmax, level, T = D and L = W."""
        rho, a = isa(alt_m)
        v = mach * a
        q = 0.5 * rho * v * v
        qs = q * self.s_m2
        cl = self.w_n / qs
        cd = self.thrust_n(mach, alt_m) / qs
        return cd - self.k * cl * cl, cl, cd


def bilinear(rows, cols, x, y):
    """rows = ((rowvar, (values per col)), ...); JSBSim CLAMPS rather than extrapolates, and so does
    this, so the deck and the recipe read the same surface."""
    rv = [r[0] for r in rows]
    x = min(max(x, rv[0]), rv[-1])
    y = min(max(y, cols[0]), cols[-1])
    i = max(0, min(len(rv) - 2, next(k for k in range(len(rv) - 1) if x <= rv[k + 1])))
    j = max(0, min(len(cols) - 2, next(k for k in range(len(cols) - 1) if y <= cols[k + 1])))
    tx = (x - rv[i]) / (rv[i + 1] - rv[i]) if rv[i + 1] != rv[i] else 0.0
    ty = (y - cols[j]) / (cols[j + 1] - cols[j]) if cols[j + 1] != cols[j] else 0.0
    a = rows[i][1][j] + (rows[i][1][j + 1] - rows[i][1][j]) * ty
    b = rows[i + 1][1][j] + (rows[i + 1][1][j + 1] - rows[i + 1][1][j]) * ty
    return a + (b - a) * tx


# fmt: off
ANCHORS = [
    # --------- the turbofan pair: the analogy is the pinned F-16's own F100 deck -----------------
    Row("f15c", "F-15C Eagle", "turbofan", "F100-PW-220", 2, 64.9, 105.7,
        56.48, 13.05, 19.43, 13154.0, 20185.0, 30844.0, 6103.0,
        2.50, 12000.0, 1.20, 19812.0, 340.6, 9.0,
        alpha_lim_deg=22.0,
        note="A7 14 590 lbf dry / 23 770 lbf AB per engine [T4]; A6 empty 29 000 lb, gross 44 500 lb, "
             "MTOW 68 000 lb, internal fuel 13 455 lb [T4]; A8 608 ft2 / 42 ft 10 in / 63 ft 9 in [T4]. "
             "Both free probes close to 0.2 % (recipe Knowledge 1)."),
    Row("su27", "Su-27S", "turbofan", "AL-31F", 2, 75.22, 122.6,
        62.0, 14.7, 21.9, 16380.0, 23430.0, 33000.0, 9400.0,
        2.35, 12000.0, 1.13, 18500.0, 300.0, 9.0,
        alpha_lim_deg=24.0,
        note="The cleanest row in the catalogue: both free probes close exactly. NO CAMPAIGN NAMES THIS "
             "TYPE (catalogue A11) -- it is capability without a question, and the deck says so."),
    # --------- the eight turbojets: the constructed reference surface ----------------------------
    Row("mig21", "MiG-21bis", "turbojet", "R-25-300", 1, 40.18, 69.58,
        23.0, 7.154, 14.7, 0.0, 8725.0, 9800.0, 2750.0,
        2.05, 13000.0, 1.06, 17500.0, 0.0, 8.5,
        clmax_v_ms=69.44, clmax_mass_kg=6500.0, alpha_lim_deg=20.0,
        note="THE TURBOJET REFERENCE'S CALIBRATION ROW (recipe §5). A6 publishes NO EMPTY MASS [TODO] "
             "-- the four-way mass closure that validated the MiG-29 deck to 0.002 % cannot be run "
             "(catalogue A10/D1), and the row stays ALPHA on that account alone. A4 is a TIME "
             "(17 000 m in 8 min 30 s), not a rate, so it is a probe and not an inversion. "
             "[DISPUTED] the published T/W 0.76 inverts to 9 333 kg against the published gross 8 725."),
    Row("mig23", "MiG-23MLD", "turbojet", "R-35-300", 1, 83.6, 127.49,
        37.35, 13.965, 16.7, 0.0, 14840.0, 17800.0, 3410.0,
        2.35, 12000.0, 1.14, 18500.0, 230.0, 8.5,
        alpha_lim_deg=18.0,
        note="THE SPREAD PLANFORM [SET] (37.35 m2 / 13.965 m span): the row is an interceptor in W3/O1 "
             "and the spread wing is its acquisition configuration. The bias is declared -- understates "
             "supersonic Vmax, overstates the swept-wing turn (recipe R8). "
             "[DISPUTED] TWICE, in opposite directions: T/W inverts to 14 281 kg, wing loading to "
             "13 820 kg, published gross 14 840 kg, and no empty mass at all. The least internally "
             "consistent row in the catalogue (catalogue D2)."),
    Row("mig25", "MiG-25PD", "turbojet", "R-15B-300", 2, 73.5, 100.1,
        61.4, 14.01, 23.82, 20000.0, 36720.0, 41000.0, 14570.0,
        3.10, 13000.0, 1.06, 20700.0, 208.0, 4.5,
        alpha_lim_deg=12.0,
        note="RECIPE R2, NAMED IN ADVANCE: the R-15 was designed for sustained M2.8+ with ram "
             "compression an M2 engine never sees, so the reference surface -- calibrated at M2.05 -- "
             "is extrapolating by a full Mach number at exactly this row's most important anchor. IT "
             "WILL BE THE RECIPE'S WORST ROW AND THAT IS PREDICTED, NOT DISCOVERED. "
             "A5 +4.5 g is the ROW: a sourced airframe limit (aileron reversal, 70 cm wingtip flex), "
             "not a modelling weakness. Both free probes close exactly."),
    Row("mig17", "MiG-17F", "turbojet", "VK-1F", 1, 26.5, 33.8,
        22.6, 9.628, 11.264, 3919.0, 5340.0, 6069.0, 1170.0,
        0.93, 3000.0, 0.89, 16600.0, 65.0, 8.0,
        clmax_v_ms=48.0, clmax_mass_kg=4500.0, alpha_lim_deg=20.0,
        note="THE CATALOGUE'S CHEAPEST DECK. Its A2 anchor is SUBSONIC (M0.89 at sea level), so this "
             "row inverts its own subsonic CD0 instead of taking the F-5E generalisation -- one of only "
             "two that can. [DISPUTED] wing loading closes at MTOW, T/W at gross (catalogue D3)."),
    Row("su7", "Su-7BKL", "turbojet", "AL-7F-1", 1, 66.6, 94.1,
        34.0, 9.31, 16.8, 8940.0, 13570.0, 15210.0, 3220.0,
        1.74, 11000.0, 0.94, 17600.0, 160.0, 0.0,
        alpha_lim_deg=18.0,
        note="A5 IS [TODO] AND THAT IS A HARD CONSEQUENCE, not a footnote: with no published g limit "
             "and no FLCS in the deck the alpha limiter is [SET] from the row's own measured CLmax "
             "(recipe §6) and the setting is logged. The row is ALPHA until it is measured. "
             "A2 M0.94 is transonic, so it inverts its own CD0 at the drag peak. "
             "[DISPUTED] wing loading inverts to 14 783 kg, between gross and MTOW (catalogue D4)."),
    Row("su22", "Su-17M4 / Su-22M4", "turbojet", "AL-21F-3", 1, 76.4, 109.8,
        38.5, 13.68, 19.02, 12160.0, 16400.0, 19430.0, 3770.0,
        1.70, 11000.0, 1.13, 14200.0, 230.0, 7.0,
        alpha_lim_deg=18.0,
        note="THE SPREAD PLANFORM [SET]: a strike aircraft in O3 flies its attack run configured for "
             "load, not for dash (recipe R8). [DISPUTED] wing loading inverts to 17 056 kg spread or "
             "15 284 kg swept -- the swing-wing ambiguity showing up in arithmetic before it shows up "
             "in a deck (catalogue D5). A1 1 860 km/h at 'altitude' [T4] read at 11 000 m."),
    Row("mirf1", "Mirage F1C", "turbojet", "Atar 9K-50", 1, 49.03, 70.6,
        25.0, 8.4, 15.3, 7400.0, 10900.0, 16200.0, 4300.0,
        2.20, 11000.0, 0.0, 20000.0, 243.0, 0.0,
        alpha_lim_deg=20.0,
        note="A2 IS MISSING AND IT IS A REAL COST (catalogue): recipe §4.2 inverts CD0 at TWO "
             "supersonic points and this row supplies one. It stays ALPHA on that account until a "
             "sea-level figure is sourced. A5 [TODO] as well, so the limiter is [SET] like su7's. "
             "T/W closes exactly; wing loading is not published."),
    Row("f5e", "F-5E Tiger II", "turbojet", "J85-GE-21", 2, 15.57, 22.24,
        17.28, 8.13, 14.69, 4347.0, 7142.0, 11192.0, 2050.0,
        1.63, 11000.0, 0.0, 15789.0, 175.3, 0.0,
        clmax_v_ms=63.79, clmax_mass_kg=5745.0, alpha_lim_deg=22.0,
        note="THE RECIPE'S VALIDATION ROW. It publishes CD0 = 0.0200 and (L/D)max = 10.0 [T4], the pair "
             "that DERIVES the whole catalogue's e = 0.66 and, as this build found, the whole "
             "catalogue's subsonic CD0 as well. Both free probes close to 0.3 %. A2 and A5 are [TODO]. "
             "NO CAMPAIGN NAMES THIS TYPE (catalogue A11)."),
]
# fmt: on

BY_KEY = {r.key: r for r in ANCHORS}


# --------------------------------------------------------------------------------------------------
# §3 MASS AND INERTIA -- the radii-of-gyration method, published and applied once already
# --------------------------------------------------------------------------------------------------
# The pinned F-16's tensor normalised by each axis's natural length (doc/modules/mig29/
# flight-model-spec.md §3.2): kx/b, ky/L, kz/((b+L)/2), and Ixz/Izz.
KX_B, KY_L, KZ_BL, IXZ_IZZ = 0.128, 0.206, 0.263, -0.0156
TWIN_IXX_FACTOR = 1.10   # two podded engines outboard of the centreline, exactly as the MiG-29 took


def inertia(row):
    """(Ixx, Iyy, Izz, Ixz) in slug*ft^2, from the row's own b, L and structural mass."""
    m = row.empty_kg if row.empty_kg > 0.0 else row.gross_kg - row.fuel_kg - 100.0
    ixx = m * (KX_B * row.b_m) ** 2
    if row.engines > 1:
        ixx *= TWIN_IXX_FACTOR
    iyy = m * (KY_L * row.l_m) ** 2
    izz = m * (KZ_BL * 0.5 * (row.b_m + row.l_m)) ** 2
    return (ixx * PERTURB_IXX * KGM2_SLUGFT2, iyy * KGM2_SLUGFT2, izz * KGM2_SLUGFT2,
            IXZ_IZZ * izz * KGM2_SLUGFT2)


# --------------------------------------------------------------------------------------------------
# §4.5 LIFT
# --------------------------------------------------------------------------------------------------
def cl_alpha(row):
    """Helmbold/DATCOM for a finite wing, without a sweep term: LEADING-EDGE SWEEP IS NOT PUBLISHED FOR
    A SINGLE DECK ROW (recipe §4.1 rejects Raymer's relation for exactly that reason), so the
    zero-sweep form is used and the resulting overestimate at high sweep is declared rather than
    patched with sixteen invented angles.  CLa = 2*pi*A / (2 + sqrt(A^2 + 4))"""
    a = row.ar
    return 2.0 * math.pi * a / (2.0 + math.sqrt(a * a + 4.0))


def cl_max(row):
    """INV where a landing or stall speed is published (recipe §4.5), else [SET] from the alpha limit
    on the same lift curve -- and the consequence of the [SET] case is declared in the deck banner:
    the row has no takeoff-run anchor and spawns airborne."""
    if row.clmax_v_ms > 0.0:
        w = row.clmax_mass_kg * G0
        return 2.0 * w / (1.225 * row.clmax_v_ms ** 2 * row.s_m2)
    return cl_alpha(row) * math.sin(math.radians(row.alpha_lim_deg)) * math.cos(
        math.radians(row.alpha_lim_deg)) ** 2 * 1.15


# THE ALPHA KNOTS every alpha-indexed table in a generated deck is written on. THE SPACING IS NOT
# COSMETIC AND IT WAS MEASURED: at a uniform 5 deg the lift-dependent drag table -- a PARABOLA in alpha --
# is read by JSBSim as the straight chord from 0 to 5 deg, and a supersonic dash sits at 1.5-2 deg. On
# f15c at M1.93 that chord returns CDi = 0.00519 against the polar's own k*CL^2 = 0.00179, i.e. 2.9x, and
# the 0.0034 of drag it invents is most of the 0.0045 by which the deck missed its own A1 inversion.
# One degree through the linear range makes the chord error negligible; the coarse tail past 20 deg is
# past the alpha limiter and is the EDGE of the model either way (recipe R7).
ALPHA_KNOTS_DEG = (-25, -20, -15, -10, -7, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 7, 10, 12, 15,
                   17, 20, 25, 30, 35, 40, 45)


# The potential term's own maximum: d/da [sin a * cos^2 a] = 0 at a = atan(1/sqrt(2)) = 35.264 deg.
A_POTENTIAL_MAX = math.atan(1.0 / math.sqrt(2.0))


def stall_alpha(row):
    """Where the potential lift curve REACHES CLmax, solved on the curve that is actually written --
    cla*sin(a)*cos^2(a) -- and not on cla*sin(a).

    It used to be asin(CLmax/cla), which ignores the cos^2 factor and therefore returned an alpha where
    the written curve is still 14 % BELOW CLmax. The table then jumped from that value straight to CLmax
    at the next knot: a discontinuity in the lift curve, right where several rows' alpha limiters sit
    (f15c 21.7 deg against a 22.0 deg limit). Measured, the alpha limiter could not hold its own limit on
    five rows because the aeroplane fell through the step."""
    cla = cl_alpha(row)
    target = cl_max(row) / cla if cla > 0 else 1.0
    if target >= math.sin(A_POTENTIAL_MAX) * math.cos(A_POTENTIAL_MAX) ** 2:
        return A_POTENTIAL_MAX
    lo, hi = 0.0, A_POTENTIAL_MAX
    for _ in range(60):
        mid = 0.5 * (lo + hi)
        if math.sin(mid) * math.cos(mid) ** 2 < target:
            lo = mid
        else:
            hi = mid
    return 0.5 * (lo + hi)


def lift_table(row):
    """CL(alpha) on the potential term with a stall shape. NO VORTEX TERM: none of these planforms is a
    LERX aircraft of the MiG-29's kind, and a vortex increment would be an invented number on every row.
    The curve is therefore linear-range only, and the alpha limiter in systems/FBFlightControl -- not
    this table -- is the edge of the model (module.md A1)."""
    cla, clmax = cl_alpha(row), cl_max(row)
    a_stall = stall_alpha(row)
    rows = []
    for deg in ALPHA_KNOTS_DEG:
        a = math.radians(deg)
        cl = cla * math.sin(a) * math.cos(a) ** 2
        if abs(a) > a_stall:
            # Post-stall is NOT modelled (recipe R7). What is written here is a plain decay so the
            # integration stays finite past the limiter; it is the EDGE of the model, not a model of
            # the edge, and nothing may be quoted from it.
            over = (abs(a) - a_stall) / max(1e-6, math.radians(45) - a_stall)
            cl = math.copysign(clmax * (1.0 - 0.55 * min(1.0, over)), a)
        rows.append((a, cl))
    return rows


def suction_blend_table(row):
    """(1 - f(alpha)): how much of the lift-dependent drag is still the attached-flow parabola. The
    parabola itself is NOT tabulated -- it is written against aero/cl-squared, so the deck's induced drag
    is a function of the lift the deck ACTUALLY produced, which is what a polar means. Tabulating it in
    alpha instead double-books the compressibility factor kCLmach: at M2 that factor takes 30 % off the
    lift, the aeroplane answers with 43 % more alpha, and an alpha-indexed table charges it (1/0.70)^2 of
    the induced drag the inversion assumed."""
    return [(a, 1.0 - min(1.0, (abs(a) / math.radians(30.0)) ** 1.5)) for a, _ in lift_table(row)]


def suction_drag_table(row):
    """f(alpha)*|CL|*|tan alpha|: the leading-edge suction progressively LOST past the linear range --
    the MiG-29 deck's own form, whose f(alpha) schedule is the one piece taken by [ANALOGY]. It is a
    genuine function of attitude and stays in alpha; it vanishes as alpha^2.5 and reaches 1 % of the
    parabola only past 5 deg."""
    out = []
    for a, cl in lift_table(row):
        f = min(1.0, (abs(a) / math.radians(30.0)) ** 1.5)
        out.append((a, f * abs(cl) * abs(math.tan(a))))
    return out


def cd0_table(row):
    """The polar's Mach schedule, and the file's single most consequential table (recipe §4.2).

    Returns (table, provenance-lines)."""
    prov = []
    pts = []
    for label, mach, alt in (("A2", row.a2_mach, 0.0), ("A1", row.a1_mach, row.a1_alt_m)):
        if mach <= 0.0:
            prov.append("%s is [TODO] in the catalogue -- this row inverts ONE point instead of two "
                        "and stays ALPHA on that account (recipe §4.2)" % label)
            continue
        cd0, cl, cd = row.invert_cd0(mach, alt)
        pts.append((mach, cd0))
        prov.append("%s M%.2f at %5.0f m [INV]: T = %6.1f kN (FROZEN analogy, %d x %.1f kN static), "
                    "q*S = %7.1f kN, CL %.4f, CD %.4f, k*CL^2 %.5f  ->  CD0 = %.4f"
                    % (label, mach, alt, row.thrust_n(mach, alt) / 1000.0, row.engines, row.ab_kn,
                       0.5 * isa(alt)[0] * (mach * isa(alt)[1]) ** 2 * row.s_m2 / 1000.0,
                       cl, cd, row.k * cl * cl, cd0))
    pts.sort()

    # THE SUBSONIC LEVEL. A row with an anchor at or below M 0.90 has inverted its own; every other row
    # takes the catalogue's one published value.
    own = [(m, c) for m, c in pts if m <= 0.90]
    if own:
        m0, c0 = own[0]
        sub = c0 / lerp_table(CD0_SHAPE, m0)
        prov.append("subsonic level %.4f [INV]: the M%.2f anchor is itself subsonic, divided by the "
                    "shape's own factor %.3f there" % (sub, m0, lerp_table(CD0_SHAPE, m0)))
    else:
        sub = CD0_SUB_REF
        prov.append("subsonic level 0.0200 [ANALOGY from the F-5E's published CD0, the catalogue's "
                    "ONLY one] -- the same generalisation recipe §4.1 makes for e, and it is here "
                    "because A4 inverts a subsonic CD0 for no row at all (see the tool's header)")

    # ASSEMBLE. Subsonic branch = the level on the shape; then every inverted anchor as its own knot,
    # in Mach order; then the decay continued by the shape past the last anchor. Only the rise's
    # MAGNITUDE is fitted (recipe §4.2) and its SHAPE never is.
    lo = pts[0][0] if pts else 9.9
    tbl = [(m, sub * f) for m, f in CD0_SHAPE if m < min(0.90, lo) - 1e-6]
    if lo > 1.05:
        # The transonic peak: carry the lowest anchor up to M1.05 by the shape's own ratio.
        tbl.append((0.95, sub * lerp_table(CD0_SHAPE, 0.95)))
        tbl.append((1.05, pts[0][1] * lerp_table(CD0_SHAPE, 1.05) / lerp_table(CD0_SHAPE, pts[0][0])))
    for m, c in pts:
        if not tbl or m > tbl[-1][0] + 1e-6:
            tbl.append((m, c))
    last_m, last_c = tbl[-1]
    for m, f in CD0_SHAPE:
        if m > last_m + 0.10:
            tbl.append((m, last_c * f / lerp_table(CD0_SHAPE, last_m)))
    if PERTURB_CD0 != 1.0:
        tbl = [(m, c * PERTURB_CD0) for m, c in tbl]
        prov.append("CD0 SCALED BY %.3f — an ATTRIBUTION PERTURBATION, not a model. This deck is not "
                    "the recipe's output and must not be committed." % PERTURB_CD0)
    return tbl, prov


# --------------------------------------------------------------------------------------------------
# §6 MOMENTS AND CONTROL POWER -- inverted against A5
# --------------------------------------------------------------------------------------------------
STATIC_MARGIN = 0.05   # [SET] 5 % of the mean geometric chord, stable; no row publishes one
ELEV_MAX_DEG = 25.0    # [SET] the stabilator/elevator stop every generated deck carries


def cm_alpha(row):
    return -STATIC_MARGIN * cl_alpha(row)


CLA_TAIL, TAIL_ETA = 2.4, 0.9   # [GEO] the tail's own lift slope and dynamic-pressure ratio


def cm_de(row):
    """[GEO] from the row's own tail volume: Cmde = -CLa_h * (S_h/S) * (l_h/cbar) * eta, the SAME
    surface that produces CLde = {CLa_h*(S_h/S)*eta} = 0.432 one axis up.

    IT USED TO BE INVERTED AGAINST A5 -- Cmde*de_max = -Cmalpha*alpha_limit -- and that inversion is
    RETIRED with its measurement, because it sizes the elevator to BARELY trim the alpha limit at full
    travel, i.e. to an aeroplane with zero manoeuvre margin at its own limit. Two numbers killed it:
      * on f5e it returned 0.167/rad where the recipe's own [SET] tail volume implies 1.25/rad, a
        factor 7.5 -- §2 and §6 were describing two different tails;
      * ROTATION. The moment balance about the main gear at Vr = 1.15*Vs needs
            |Cmde| >= [ d*CLmax/(1.3225*de_max) + 0.432*kCLge*d ] / cbar
        = 0.415/rad on f5e at the [SET] 0.15*cbar main-gear offset. 0.167 could not lift the nose: full
        aft stick from 167 kt held 2.8 deg of pitch until 226 kt, and the take-off run measured 1 403 m
        against a published 610 (+130 %) on EVERY row that publishes one, to within 4 %.
    The [GEO] value satisfies both by construction: it rotates with margin, and trimming the alpha
    limit now costs a fraction of the travel instead of all of it."""
    return -CLA_TAIL * 0.20 * (0.42 * row.l_m / (row.s_m2 / row.b_m)) * TAIL_ETA


def a4_weight_kg(row):
    """The WEIGHT at which the row's published maximum rate of climb is reachable with its own frozen
    thrust and its own inverted polar, at sea level, maximised over Mach. 0 where the row publishes no
    rate.

    THIS IS R11'S MISSING DERIVATION, and it decides per row rather than by decree whether A4 is an
    anchor at all. Every other anchor in the catalogue is quoted at the GROSS weight; the climb rates
    name no weight. Computed:
      * f15c 13 141 kg, su22 9 982 kg, mirf1 6 024 kg -- BELOW the row's own EMPTY weight, i.e. the
        published figure is unreachable at any loading of that aeroplane. It is not a constraint on a
        deck, it is a constraint on nothing.
      * su27, mig23, mig25, su7, f5e -- between empty and gross, i.e. a light-weight figure whose weight
        no source states.
      * mig17 alone reaches its published 65 m/s at gross (76.9 m/s available).
    So A4 is JUDGED where the figure is reachable at the weight the rest of the anchor set is flown at,
    and a PROBE where it is not."""
    if row.a4_roc_ms <= 0.0:
        return 0.0
    tbl, _ = cd0_table(row)
    rho, a = isa(0.0)
    best = 0.0
    for i in range(4, 22):
        m = i * 0.05
        v = m * a
        qs = 0.5 * rho * v * v * row.s_m2
        thrust = row.thrust_n(m, 0.0)
        cd0 = lerp_table(tbl, m)
        lo, hi = 200.0, row.gross_kg * 3.0
        for _ in range(80):
            mid = 0.5 * (lo + hi)
            w = mid * G0
            cl = w / qs
            if (thrust - (cd0 + row.k * cl * cl) * qs) * v / w > row.a4_roc_ms:
                lo = mid
            else:
                hi = mid
        best = max(best, 0.5 * (lo + hi))
    return best


def pitch_stick_max(row):
    """systems/FBFlightControl's pitch authority cap for THIS row [DERIVED], recipe §6.1: the stick
    fraction at which full travel TRIMS 1.5x the row's own alpha limit. The limiter then owns the same
    50 % reserve on every row, and no row can pull itself into a tumble on the cap alone -- the one real
    hazard the MiG-29 round measured on a raw airframe. It is the ONE number a catalogue row's flight
    control preset needs beyond its published alpha limit."""
    a = math.radians(row.alpha_lim_deg)
    de = math.radians(ELEV_MAX_DEG)
    return min(1.0, 1.5 * abs(cm_alpha(row)) * a / (abs(cm_de(row)) * de))


def cm_de_rotation_demand(row):
    """What rotation at Vr needs of |Cmde|, so the deck can print its own margin (recipe §7 step 5).
    0 where the row publishes no stall/landing speed -- such a row spawns airborne and never rotates."""
    if row.clmax_v_ms <= 0.0:
        return 0.0
    chord = row.s_m2 / row.b_m
    d = 0.15 * chord
    de = math.radians(ELEV_MAX_DEG)
    return (d * cl_max(row) / (1.3225 * de) + 0.432 * 1.15 * d) / chord


# --------------------------------------------------------------------------------------------------
# XML emission
# --------------------------------------------------------------------------------------------------
def xml_safe(text):
    """A double hyphen is illegal INSIDE an XML comment, and these decks are three quarters comment.
    The two delimiters are protected and every other run of hyphens becomes an em dash."""
    return (text.replace("<!--", "\x01").replace("-->", "\x02")
                .replace("--", "\u2014").replace("\x01", "<!--").replace("\x02", "-->"))


def tbl_xml(rows, indent, fmt="%8.4f  %9.5f"):
    return "\n".join(" " * indent + fmt % (x, y) for x, y in rows)


def surface_xml(name, rows, indent=2):
    pad = " " * indent
    head = pad + "     " + "".join("%8d" % c for c in TF_ALT)
    body = "\n".join(pad + " %5.2f" % m + "".join("%8.4f" % v for v in cols) for m, cols in rows)
    return ("  <function name=\"%s\">\n   <table>\n"
            "    <independentVar lookup=\"row\">velocities/mach</independentVar>\n"
            "    <independentVar lookup=\"column\">atmosphere/density-altitude</independentVar>\n"
            "    <tableData>\n%s\n%s\n    </tableData>\n   </table>\n  </function>\n"
            % (name, head, body))


ENGINE_TPL = """<?xml version="1.0"?>
<!--
  FlightBox — {engine}, GENERATED by tools/gen_air_decks.py from doc/modules/air/flight-model-recipe.md
  §5. DO NOT EDIT: the next run of the tool overwrites this file.

  {count} instance(s) of this deck are the {name}'s propulsion; placement lives in {key}.xml.

  THE STATICS ARE PUBLISHED AND THE SURFACES ARE NOT. milthrust/maxthrust are the catalogue's own
  A7 figures [T4]; the (Mach, density-altitude) surfaces are {analogy}. They are DIMENSIONLESS
  FACTORS normalised to 1.0 at M0/sea level, so setting the statics IS the scaling step, exactly and
  with nothing in between.

  AND THE RULE THAT GOES WITH IT (recipe §0): this deck is FROZEN. The envelope anchors constrain the
  PRODUCT T/D and never T and D separately, so all residual error is absorbed by the drag polar in
  {key}.xml. If that forces an implausible CD0, THIS FILE is the suspect.
-->

<turbine_engine name="{engine}">
  <!-- A7 [T4]: {mil_kn:.2f} kN dry / {ab_kn:.2f} kN augmented, static, sea level, M0. -->
  <milthrust> {mil_lbf:.1f} </milthrust>
  <maxthrust> {ab_lbf:.1f} </maxthrust>

  <bypassratio> {bpr:.2f} </bypassratio>
  <tsfc> {tsfc:.2f} </tsfc>
  <atsfc> {atsfc:.2f} </atsfc>

  <!-- Idle spool speeds [SET] at the pinned F-16 deck's own values: no source states them for any
       engine in this catalogue, and one borrowed pair is better than ten invented ones. -->
  <idlen1> 30.0 </idlen1>
  <idlen2> 60.0 </idlen2>
  <maxn1> 100.0 </maxn1>
  <maxn2> 100.0 </maxn2>

  <augmented> 1 </augmented>
  <augmethod> 2 </augmethod>
  <injected> 0 </injected>

{idle}
{mil}
{aug}
</turbine_engine>
"""

NOZZLE_TPL = """<?xml version="1.0"?>
<!-- FlightBox — the {engine}'s thruster: thrust is computed entirely by the turbine model, this file
     only says "apply it as a direct force". GENERATED by tools/gen_air_decks.py. -->
<direct name="{engine} nozzle">
</direct>
"""

RESET_TPL = """<?xml version="1.0"?>
<initialize name="reset00">
  <!-- A default IC because a JSBSim model is expected to ship one. FlightBox uses it for exactly as
       much as it uses the pinned F-16's: nothing — every initial condition comes from fdm/FBFdmBoot.
       GENERATED by tools/gen_air_decks.py. -->
  <ubody unit="FT/SEC"> 0.0 </ubody>
  <vbody unit="FT/SEC"> 0.0 </vbody>
  <wbody unit="FT/SEC"> 0.0 </wbody>
  <latitude unit="DEG"> 46.7 </latitude>
  <longitude unit="DEG"> 6.8 </longitude>
  <phi unit="DEG"> 0.0 </phi>
  <theta unit="DEG"> 1.0 </theta>
  <psi unit="DEG"> 0.0 </psi>
  <altitude unit="FT"> {alt_ft:.1f} </altitude>
</initialize>
"""


def gear_xml(row):
    """Three BOGEYs sized off the row's own length and span. Spring/damping scale with the structural
    mass against the pinned F-16's (7 893 kg / 20 000 lbs-per-ft), friction is the F-16 deck's own
    (dry concrete is dry concrete) [ANALOGY]."""
    m = row.empty_kg if row.empty_kg > 0.0 else row.gross_kg - row.fuel_kg - 100.0
    k = 20000.0 * m / 7893.0
    # THE MAIN GEAR SITS 0.15 c-bar BEHIND THE CG [SET] -- the normal tricycle layout, and MEASURED
    # rather than chosen: at 0.06 L behind it the nose gear carried 22 % of the weight and the inverted
    # elevator power (sized to trim the alpha limit, not to lever a nose off a runway) could not rotate
    # the aeroplane at all, giving a takeoff run of 4 643 m against a published 610. At 0.15 c-bar the
    # nose carries ~10 %, inside the documented 8-15 % band.
    chord_ft = row.s_m2 * M2_FT2 / (row.b_m * M_FT)
    nose_x = 0.28 * row.l_m * M_FT * 12.0
    main_x = 0.50 * row.l_m * M_FT * 12.0 + 0.15 * chord_ft * 12.0
    drop = -0.13 * row.l_m * M_FT * 12.0
    track = 0.24 * row.b_m * M_FT * 12.0
    c = []
    for name, x, y in (("NOSE_LG", nose_x, 0.0), ("LEFT_MLG", main_x, -track), ("RIGHT_MLG", main_x, track)):
        c.append("""    <contact type="BOGEY" name="%s">
      <location unit="IN"> <x> %.1f </x> <y> %.1f </y> <z> %.1f </z> </location>
      <static_friction> 0.8 </static_friction>
      <dynamic_friction> 0.5 </dynamic_friction>
      <rolling_friction> 0.02 </rolling_friction>
      <spring_coeff unit="LBS/FT"> %.0f </spring_coeff>
      <damping_coeff unit="LBS/FT/SEC"> %.0f </damping_coeff>
      %s
      <brake_group> %s </brake_group>
      <retractable>1</retractable>
    </contact>""" % (name, x, y, drop, k, 0.25 * k,
                     "<max_steer unit=\"DEG\"> 30 </max_steer>" if name == "NOSE_LG"
                     else "<max_steer unit=\"DEG\"> 0 </max_steer>",
                     "NOSE" if name == "NOSE_LG" else ("LEFT" if y < 0 else "RIGHT")))
    return "\n".join(c)


def deck_xml(row):
    s_ft2 = row.s_m2 * M2_FT2
    b_ft = row.b_m * M_FT
    l_ft = row.l_m * M_FT
    chord_ft = s_ft2 / b_ft
    ixx, iyy, izz, ixz = inertia(row)
    empty_kg = row.empty_kg if row.empty_kg > 0.0 else row.gross_kg - row.fuel_kg - 100.0
    cd0, prov = cd0_table(row)
    cla, clmax = cl_alpha(row), cl_max(row)
    cg_in = 0.5 * l_ft * 12.0
    analogy = ("the pinned F-16 model's own F100-PW-229 deck (recipe §5: the same engine one dash "
               "number away)" if row.family == "turbofan" else
               "the recipe's CONSTRUCTED turbojet reference surface, calibrated on mig21")
    fam_note = ("" if row.empty_kg > 0.0 else
                "\n  NO PUBLISHED EMPTY MASS. empty = gross - internal fuel - pilot = %.0f kg [DERIVED],\n"
                "  and the four-way mass closure that validated the MiG-29 deck to 0.002 %% therefore\n"
                "  CANNOT BE RUN for this row. It stays ALPHA on that account alone (catalogue A10).\n"
                % empty_kg)

    header = """<?xml version="1.0"?>
<?xml-stylesheet type="text/xsl" href="http://jsbsim.sourceforge.net/JSBSim.xsl"?>
<!--
  FlightBox — {name}, a FlightBox-OWN JSBSim model, GENERATED by tools/gen_air_decks.py.
  DO NOT EDIT BY HAND: the next run of the tool overwrites this file, and a hand edit would break the
  one property that makes ten decks affordable — that they all came out of ONE procedure.

  ============================================================================================
  READ THIS BEFORE QUOTING ANY NUMBER BELOW AS A {name} PROPERTY.

  This is a CATALOGUE CELL, not a module: an airframe FlightBox is not judged on
  (doc/modules/air/module.md §Spec 1). There is no wind-tunnel dataset behind it at any confidence
  tier. Every aerodynamic number here is one of exactly four things:

    [INV]      INVERTED from a published envelope anchor by the equations of motion. The anchor is
               named at the number.
    [GEO]      COMPUTED FROM GEOMETRY by a standard method (DATCOM / lifting-line / tail volume).
    [ANALOGY]  TAKEN FROM A DIFFERENT AIRCRAFT'S PUBLIC DATA, declared as such.
    [SET]      A DECLARED FlightBox SETTING; the reasoning is stated and the consequence measurable.

  WHAT THIS DECK DELIBERATELY CANNOT DO (recipe R7 / module.md A1): there is NO post-stall and NO
  departure aerodynamics. The linear range is modelled and stops; systems/FBFlightControl's alpha
  limiter is not a MODELLED boundary but the EDGE OF THE MODEL. A catalogue fighter is at its most
  faithful in the BVR arena and at its least faithful in a slow-speed knife fight.

  ROW NOTE: {note}{fam_note}
  ============================================================================================

  THE POLAR'S PROVENANCE, number by number (recipe §4.2, and every inverted CD0 is published beside
  the frozen thrust that produced it, so a later reader can re-solve without re-deriving):
{prov}

  STRUCTURAL FRAME: origin at the NOSE TIP, x positive AFT, y positive to starboard, z positive up,
  z = 0 on the fuselage reference line. Overall length {l_m:.2f} m = {l_in:.0f} in [T4].
-->
<fdm_config name="{name}" version="2.0" release="ALPHA"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xsi:noNamespaceSchemaLocation="http://jsbsim.sourceforge.net/JSBSim.xsd">

  <fileheader>
    <author> FlightBox (tools/gen_air_decks.py) </author>
    <license licenseName="GPL-2.0-or-later" licenseURL="https://www.gnu.org/licenses/gpl-2.0.html"/>
    <filecreationdate> 2026-07-28 </filecreationdate>
    <version> 1.0 </version>
    <description> {name} — a GENERATED FlightBox catalogue deck, doc/modules/air/flight-model-recipe.md </description>
    <note>
      GENERATED FROM EIGHT PUBLISHED ENVELOPE ANCHORS BY CLOSED-FORM INVERSION, NOT MEASURED. release=
      ALPHA until `make -C sim test-air {key}` puts every anchor inside its band (recipe §7.1) AND the
      row passes module.md §Spec 11's attribution test. There is no PRODUCTION for a catalogue row.
    </note>
  </fileheader>
""".format(name=row.name, key=row.key, note=row.note, fam_note=fam_note, l_m=row.l_m,
           l_in=l_ft * 12.0,
           prov="\n".join("    " + p for p in prov))

    metrics = """
  <!-- ============================== GEOMETRY (recipe §2) ============================== -->
  <metrics>
    <!-- A8 [T4]. Every coefficient below is non-dimensionalised against exactly this number. -->
    <wingarea unit="FT2"> {s:.1f} </wingarea>
    <wingspan unit="FT"> {b:.2f} </wingspan>
    <!-- [DERIVED] mean GEOMETRIC chord = S/b. True MAC differs with taper; the whole polar is written
         against this reference and never mixed with another, so the +-10 % cancels. -->
    <chord unit="FT"> {c:.2f} </chord>
    <!-- [SET] 0.20*S / 0.15*S and 0.42*L / 0.40*L: tail areas and arms are almost never published, and
         they set only damping and control power — control power is INVERTED against A5 below, so an
         error here is absorbed one step later. vtailarm is explicitly NOT left at 0: the pinned F-16
         deck's unfilled field is a defect this recipe does not repeat. -->
    <htailarea unit="FT2"> {ht:.1f} </htailarea>
    <htailarm unit="FT"> {hl:.2f} </htailarm>
    <vtailarea unit="FT2"> {vt:.1f} </vtailarea>
    <vtailarm unit="FT"> {vl:.2f} </vtailarm>
    <!-- AERORP sits ON the design CG station, so every moment coefficient is a moment about that point
         and the static margin is exactly -dCm/dalpha over CLalpha. [SET] -->
    <location name="AERORP" unit="IN"> <x> {cg:.0f} </x> <y> 0 </y> <z> 0 </z> </location>
    <location name="EYEPOINT" unit="IN"> <x> {eye:.0f} </x> <y> 0 </y> <z> 20 </z> </location>
    <location name="VRP" unit="IN"> <x> 0 </x> <y> 0 </y> <z> 0 </z> </location>
  </metrics>
""".format(s=s_ft2, b=b_ft, c=chord_ft, ht=0.20 * s_ft2, hl=0.42 * l_ft, vt=0.15 * s_ft2,
           vl=0.40 * l_ft, cg=cg_in, eye=0.26 * l_ft * 12.0)

    mass = """
  <!-- ============================== MASS AND INERTIA (recipe §3) ==============================
       THE INERTIA TENSOR IS PUBLISHED FOR NO ROW IN THIS CATALOGUE. It is [DERIVED] by the normalised
       radii of gyration of doc/modules/mig29/flight-model-spec.md §3.2, already published and applied
       once: take the pinned F-16's tensor, normalise each axis by its natural length
       (kx/b = {kxb}, ky/L = {kyl}, kz/((b+L)/2) = {kzbl}, Ixz/Izz = {ixzr}), re-dimension on this row's
       own b = {b_m:.2f} m, L = {l_m:.2f} m and structural mass {m:.0f} kg.{twin}
       Ixx and the roll control power are the only two knobs that set roll acceleration; both are
       derived rather than measured, and they are the FIRST thing to re-derive if the measured roll
       response misses. -->
  <mass_balance negated_crossproduct_inertia="true">
    <ixx unit="SLUG*FT2"> {ixx:.0f} </ixx>
    <iyy unit="SLUG*FT2"> {iyy:.0f} </iyy>
    <izz unit="SLUG*FT2"> {izz:.0f} </izz>
    <ixy unit="SLUG*FT2"> 0 </ixy>
    <ixz unit="SLUG*FT2"> {ixz:.0f} </ixz>
    <iyz unit="SLUG*FT2"> 0 </iyz>

    <emptywt unit="LBS"> {ew:.0f} </emptywt>
    <location name="CG" unit="IN"> <x> {cg:.0f} </x> <y> 0 </y> <z> 0 </z> </location>

    <!-- [SET] 100 kg pilot + kit as ONE point mass, mirroring the F-16 and MiG-29 decks. -->
    <pointmass name="Pilot">
      <weight unit="LBS"> 220 </weight>
      <location unit="IN"> <x> {eye:.0f} </x> <y> 0 </y> <z> 0 </z> </location>
    </pointmass>
  </mass_balance>
""".format(kxb=KX_B, kyl=KY_L, kzbl=KZ_BL, ixzr=IXZ_IZZ, b_m=row.b_m, l_m=row.l_m, m=empty_kg,
           twin=("\n       TWIN-ENGINE CORRECTION, applied and declared: Ixx is raised by 10 %% for two "
                 "podded\n       engines outboard of the centreline, exactly as the MiG-29 deck took it."
                 if row.engines > 1 else ""),
           ixx=ixx, iyy=iyy, izz=izz, ixz=ixz, ew=empty_kg * KG_LB, cg=cg_in,
           eye=0.26 * l_ft * 12.0)

    ground = """
  <ground_reactions>
    <!-- [ANALOGY] stations from the row's own length and span; spring/damping scaled from the pinned
         F-16's by the structural-mass ratio; friction is the F-16 deck's own. -->
{gear}
    <!-- The tail bumper sits ON the reference line and 0.90 L aft, so it clears the gear at any
         rotation attitude below ~16 deg. Measured, and it cost this recipe a takeoff run of 4 643 m
         against a published 610: with the bumper at 0.95 L and 0.07 L BELOW the line, the aeroplane
         struck its own tail before it reached its rotation attitude and never flew. -->
    <contact type="STRUCTURE" name="TAIL">
      <location unit="IN"> <x> {tail:.0f} </x> <y> 0 </y> <z> {z:.0f} </z> </location>
      <static_friction> 1.0 </static_friction>
      <dynamic_friction> 1.0 </dynamic_friction>
      <spring_coeff unit="LBS/FT"> 40000 </spring_coeff>
      <damping_coeff unit="LBS/FT/SEC"> 8000 </damping_coeff>
    </contact>
  </ground_reactions>
""".format(gear=gear_xml(row), tail=0.90 * l_ft * 12.0, z=0.0)

    tank_lbs = row.fuel_kg * KG_LB
    engines = "\n".join("""    <engine file="{e}">
      <feed>0</feed>
      <thruster file="{e}-nozzle">
        <location unit="IN"> <x> {x:.0f} </x> <y> {y:.1f} </y> <z> -6 </z> </location>
        <orient unit="DEG"> <roll> 0.0 </roll> <pitch> 0.0 </pitch> <yaw> 0.0 </yaw> </orient>
      </thruster>
    </engine>""".format(e=row.engine, x=0.94 * l_ft * 12.0,
                        y=0.0 if row.engines == 1 else (-1) ** i * 0.055 * b_ft * 12.0)
                        for i in range(row.engines))
    prop = """
  <!-- ============================== PROPULSION (recipe §5) ============================== -->
  <propulsion>
{engines}
    <!-- ONE tank at the published internal capacity (A6 {fuel:.0f} kg = {lbs:.0f} lb [T4]) on JSBSim's
         own starvation model. Feed order is [SET] and the CG travel it implies is declared, not
         modelled — no source gives a sequence for any row in this catalogue. -->
    <tank type="FUEL">
      <location unit="IN"> <x> {cg:.0f} </x> <y> 0 </y> <z> 0 </z> </location>
      <capacity unit="LBS"> {lbs:.0f} </capacity> <contents unit="LBS"> {lbs:.0f} </contents>
    </tank>
  </propulsion>
""".format(engines=engines, fuel=row.fuel_kg, lbs=tank_lbs, cg=cg_in)

    fcs = """
  <!-- ============================== FLIGHT CONTROL (recipe §6) ==============================
       THIS IS A RAW AIRFRAME. There is no FLCS inside this deck and no fcs/fbw-override to bypass:
       systems/FBFlightControl wraps it from the OUTSIDE on the Manual/raw path that already exists and
       was already hardened for the MiG-29. The g limit (A5 = {g}) and the alpha limiter live THERE and
       not here, for the three reasons doc/modules/mig29/flight-model-spec.md §7.3 gives — the limiter
       is a force effect, it must be overridable, and burying it here would hide it from
       core/FBFlightMonitor, whose whole point is that the limiter is not the judge. -->
  <flight_control name="{name} conventional FCS">
    <channel name="Pitch">
      <summer name="fcs/pitch-stick-plus-trim">
        <input>fcs/elevator-cmd-norm</input>
        <input>fcs/pitch-trim-cmd-norm</input>
        <clipto> <min>-1.0</min> <max>1.0</max> </clipto>
      </summer>
      <aerosurface_scale name="fcs/elevator-control">
        <input>fcs/pitch-stick-plus-trim</input>
        <range> <min>-{de}</min> <max>{de}</max> </range>
        <output>fcs/elevator-pos-rad</output>
      </aerosurface_scale>
    </channel>
    <channel name="Roll">
      <summer name="fcs/roll-stick-plus-trim">
        <input>fcs/aileron-cmd-norm</input>
        <input>fcs/roll-trim-cmd-norm</input>
        <clipto> <min>-1.0</min> <max>1.0</max> </clipto>
      </summer>
      <aerosurface_scale name="fcs/aileron-control">
        <input>fcs/roll-stick-plus-trim</input>
        <range> <min>-0.3491</min> <max>0.3491</max> </range>
        <output>fcs/left-aileron-pos-rad</output>
      </aerosurface_scale>
      <aerosurface_scale name="fcs/aileron-control-right">
        <input>fcs/roll-stick-plus-trim</input>
        <range> <min>-0.3491</min> <max>0.3491</max> </range>
        <output>fcs/right-aileron-pos-rad</output>
      </aerosurface_scale>
    </channel>
    <channel name="Yaw">
      <summer name="fcs/rudder-stick-plus-trim">
        <input>fcs/rudder-cmd-norm</input>
        <input>fcs/yaw-trim-cmd-norm</input>
        <clipto> <min>-1.0</min> <max>1.0</max> </clipto>
      </summer>
      <aerosurface_scale name="fcs/rudder-control">
        <input>fcs/rudder-stick-plus-trim</input>
        <range> <min>-0.5236</min> <max>0.5236</max> </range>
        <output>fcs/rudder-pos-rad</output>
      </aerosurface_scale>
    </channel>
    <!-- SAME CONVENTION AS THE PINNED F-16 AND THE MiG-29 DECKS, deliberately: throttle-pos =
         2 x throttle-cmd, so 0.5 is military and 1.0 is maximum augmentation. FlightBox commands ONE
         lever (fdm/FBFdm::SetControls writes fcs/throttle-cmd-norm in [0,1]), so a deck without this
         channel can never light its afterburner at all — measured, and it cost this recipe its first
         set of anchor runs. The channel FANS THE ONE LEVER OUT to every engine position; engine-
         differential throttle is not a FlightBox control and no channel produces it. -->
    <channel name="Throttle">
{throttles}    </channel>

    <channel name="Flaps">
      <kinematic name="fcs/flap-control">
        <input>fcs/flap-cmd-norm</input>
        <traverse> <setting> <position>0</position> <time>0</time> </setting>
                   <setting> <position>0.4363</position> <time>4</time> </setting> </traverse>
        <output>fcs/flap-pos-rad</output>
      </kinematic>
    </channel>
    <channel name="Speedbrake">
      <kinematic name="fcs/speedbrake-control">
        <input>fcs/speedbrake-cmd-norm</input>
        <traverse> <setting> <position>0</position> <time>0</time> </setting>
                   <setting> <position>1</position> <time>1.5</time> </setting> </traverse>
        <output>fcs/speedbrake-pos-norm</output>
      </kinematic>
    </channel>
    <channel name="Landing Gear">
      <kinematic name="fcs/gear-control">
        <input>gear/gear-cmd-norm</input>
        <traverse> <setting> <position>0</position> <time>0</time> </setting>
                   <setting> <position>1</position> <time>6</time> </setting> </traverse>
        <output>gear/gear-pos-norm</output>
      </kinematic>
    </channel>
  </flight_control>
""".format(name=row.name, g=row.a5_g if row.a5_g > 0 else "[TODO]", de=math.radians(ELEV_MAX_DEG),
           throttles="".join(
               """      <pure_gain name="fcs/throttle-%d">
        <input>fcs/throttle-cmd-norm</input>
        <gain>2.0</gain>
        <output>fcs/throttle-pos-norm[%d]</output>
      </pure_gain>\n""" % (i, i) for i in range(row.engines)))

    cmde, cma = cm_de(row), cm_alpha(row)
    aero = """
  <!-- ============================== AERODYNAMICS (recipe §4) ============================== -->
  <aerodynamics>
    <!-- GROUND EFFECT [ANALOGY], the pinned F-16 deck's kCLge table (NASA H-1999/H-2177 — a real
         measured shape, of another aircraft). LIFT ONLY, as that deck does. -->
    <function name="aero/function/kCLge">
      <table>
        <independentVar>aero/h_b-mac-ft</independentVar>
        <tableData>
          0.0000  1.2290
          0.1500  1.1160
          0.3000  1.1050
          0.5000  1.0340
          0.7000  1.0080
          1.0000  1.0000
          1.1000  1.0000
        </tableData>
      </table>
    </function>
    <!-- COMPRESSIBILITY ON LIFT [GEO]: Prandtl-Glauert growth to the transonic peak, then the
         supersonic 4/sqrt(M^2-1) decay limited by the finite aspect ratio. -->
    <function name="aero/function/kCLmach">
      <table>
        <independentVar>velocities/mach</independentVar>
        <tableData>
          0.00  1.00
          0.85  1.10
          0.95  1.20
          1.05  1.25
          1.20  1.15
          1.60  0.85
          2.00  0.70
          3.20  0.55
        </tableData>
      </table>
    </function>

    <axis name="LIFT">
      <!-- CLalpha = 2*pi*A/(2+sqrt(A^2+4)) = {cla:.3f} /rad at AR {ar:.3f} [GEO, Helmbold]. NO SWEEP
           TERM: leading-edge sweep is not published for a single deck row (recipe §4.1 rejects
           Raymer's relation for exactly that reason), so the zero-sweep form is used and the resulting
           overestimate at high sweep is DECLARED rather than patched with ten invented angles.
           CLmax = {clmax:.3f} {clsrc}. -->
      <function name="aero/coefficient/CLalpha">
        <description>Lift due to alpha</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>aero/function/kCLge</property>
          <property>aero/function/kCLmach</property>
          <table>
            <independentVar>aero/alpha-rad</independentVar>
            <tableData>
{lift}
            </tableData>
          </table>
        </product>
      </function>
      <!-- [GEO] CLde = (S_h/S) * CLa_h * eta = 0.20 * 2.4 * 0.9 = 0.432 /rad. -->
      <function name="aero/coefficient/CLde">
        <description>Lift due to elevator</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>aero/function/kCLge</property>
          <property>fcs/elevator-pos-rad</property>
          <value> 0.432 </value>
        </product>
      </function>
      <!-- [SET] 0.60 /rad of trailing-edge flap, the single-slotted increment of a fighter flap of
           ~8 % of the reference area. Where a landing speed is published the SUM is INV against it. -->
      <function name="aero/coefficient/CLflaps">
        <description>Lift due to flaps</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>aero/function/kCLge</property>
          <property>fcs/flap-pos-rad</property>
          <value> 0.600 </value>
        </product>
      </function>
      <!-- [GEO] CLq ~ 2*eta*CLa_h*V_H + wing = 3.2 /rad at the recipe's [SET] tail volume. -->
      <function name="aero/coefficient/CLq">
        <description>Lift due to pitch rate</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>aero/ci2vel</property>
          <property>velocities/q-aero-rad_sec</property>
          <value> 3.2 </value>
        </product>
      </function>
    </axis>

    <axis name="DRAG">
      <!-- ZERO-LIFT DRAG vs MACH — where recipe §0 puts ALL the residual error of the frozen thrust
           analogy. The provenance of every knot is in this file's header. -->
      <function name="aero/coefficient/CD0">
        <description>Zero-lift drag vs Mach</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <table>
            <independentVar>velocities/mach</independentVar>
            <tableData>
{cd0}
            </tableData>
          </table>
        </product>
      </function>
      <!-- LIFT-DEPENDENT DRAG, and it is written AGAINST THE LIFT COEFFICIENT THE DECK ACTUALLY
           PRODUCES (aero/cl-squared) and not against alpha. k = 1/(pi*AR*e) = {k:.4f} with e = 0.66
           [DERIVED, recipe §4.1 — the F-5E's published CD0/(L/D)max pair, generalised, and the FIRST
           term the attribution band sweeps]. Past the linear range leading-edge suction is
           progressively lost, and only THAT part is a function of attitude:
             CDi = k*CL^2*(1-f(a)) + f(a)*CL*tan(a),   f = min(1, (a/30deg)^1.5)   [ANALOGY]
           MEASURED, and the reason the split exists: with the whole term tabulated in alpha the deck
           charged 2.9x the polar's own induced drag at a supersonic dash attitude, because a parabola
           sampled every 5 deg is read as a chord at 1.8 deg AND because kCLmach's lift loss reappears
           as extra alpha. Neither is a property of the aeroplane; both moved A1. -->
      <function name="aero/coefficient/CDi">
        <description>Induced drag on the lift actually produced</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>aero/cl-squared</property>
          <value> {k:.6f} </value>
          <table>
            <independentVar>aero/alpha-rad</independentVar>
            <tableData>
{blend}
            </tableData>
          </table>
        </product>
      </function>
      <function name="aero/coefficient/CDsuction">
        <description>Leading-edge suction lost past the linear range</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <table>
            <independentVar>aero/alpha-rad</independentVar>
            <tableData>
{cdi}
            </tableData>
          </table>
        </product>
      </function>
      <!-- [SET] trim drag 0.15*de^2; gear 0.020; flaps 0.08/rad; speedbrake 0.035 — the pinned F-16
           deck's own magnitudes, which are the only measured ones in the tree. -->
      <function name="aero/coefficient/CDde">
        <description>Drag due to elevator deflection</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <value> 0.15 </value>
          <property>fcs/elevator-pos-rad</property>
          <property>fcs/elevator-pos-rad</property>
        </product>
      </function>
      <function name="aero/coefficient/CDgear">
        <description>Drag due to gear</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>gear/gear-pos-norm</property>
          <value> 0.020 </value>
        </product>
      </function>
      <function name="aero/coefficient/CDflaps">
        <description>Drag due to flaps</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>fcs/flap-pos-rad</property>
          <value> 0.080 </value>
        </product>
      </function>
      <function name="aero/coefficient/CDsb">
        <description>Drag due to speedbrake</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>fcs/speedbrake-pos-norm</property>
          <value> 0.035 </value>
        </product>
      </function>
    </axis>

    <axis name="SIDE">
      <!-- [GEO] CYb = -CLa_v * (S_v/S) * eta = -2.0 * 0.15 * 0.9 = -0.27 /rad; CYdr = +0.14 /rad. -->
      <function name="aero/coefficient/CYb">
        <description>Side force due to beta</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>aero/beta-rad</property>
          <value> -0.270 </value>
        </product>
      </function>
      <function name="aero/coefficient/CYdr">
        <description>Side force due to rudder</description>
        <product>
          <property>aero/qbar-psf</property>
          <property>metrics/Sw-sqft</property>
          <property>fcs/rudder-pos-rad</property>
          <value> 0.140 </value>
        </product>
      </function>
    </axis>

    <axis name="ROLL">
      <!-- [SET/GEO] dihedral effect Clb -0.08 /rad, roll damping Clp -0.32, Clr +0.12, aileron power
           Clda {clda:.3f} /rad (a strip integration of a 25 %-semispan aileron pair at the recipe's
           [SET] surface travel), rudder cross-coupling Cldr +0.010. Clda and Ixx are the only two
           numbers that set roll acceleration — doc/pilot.md's close-combat law INVERTS the resulting
           roll plant, which is why step 7 of the recipe MEASURES it and the tier gate reads the
           measurement rather than this line. -->
      <function name="aero/coefficient/Clb">
        <description>Roll moment due to beta</description>
        <product>
          <property>aero/qbar-psf</property><property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property><property>aero/beta-rad</property>
          <value> -0.080 </value>
        </product>
      </function>
      <function name="aero/coefficient/Clp">
        <description>Roll damping</description>
        <product>
          <property>aero/qbar-psf</property><property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property><property>aero/bi2vel</property>
          <property>velocities/p-aero-rad_sec</property>
          <value> -0.320 </value>
        </product>
      </function>
      <function name="aero/coefficient/Clr">
        <description>Roll moment due to yaw rate</description>
        <product>
          <property>aero/qbar-psf</property><property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property><property>aero/bi2vel</property>
          <property>velocities/r-aero-rad_sec</property>
          <value> 0.120 </value>
        </product>
      </function>
      <function name="aero/coefficient/Clda">
        <description>Roll moment due to aileron</description>
        <product>
          <property>aero/qbar-psf</property><property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property><property>fcs/left-aileron-pos-rad</property>
          <value> {clda:.4f} </value>
        </product>
      </function>
      <function name="aero/coefficient/Cldr">
        <description>Roll moment due to rudder</description>
        <product>
          <property>aero/qbar-psf</property><property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property><property>fcs/rudder-pos-rad</property>
          <value> 0.010 </value>
        </product>
      </function>
    </axis>

    <axis name="PITCH">
      <!-- Cmalpha = -{sm} * CLalpha = {cma:.4f} /rad [SET static margin, {sm100:.0f} % of the mean
           geometric chord — no row publishes one, and a stable margin is what a conventional airframe
           of this era has]. Cmde = {cmde:.4f} /rad is [GEO] from THIS row's own tail volume,
           -CLa_h*(S_h/S)*(l_h/cbar)*eta, the same surface that produces CLde = 0.432 one axis up.
           IT IS NO LONGER INVERTED AGAINST A5: that inversion sized the elevator to barely TRIM the
           alpha limit at full travel and could not lift a nose off a runway — measured, a take-off run
           of 1 403 m against a published 610 with full aft stick held from 167 to 226 kt. Rotation at
           Vr = 1.15*Vs needs |Cmde| >= {cmderot:.3f}/rad here; this deck carries {cmdeabs:.3f}. -->
      <function name="aero/coefficient/Cm">
        <description>Pitch moment due to alpha</description>
        <product>
          <property>aero/qbar-psf</property><property>metrics/Sw-sqft</property>
          <property>metrics/cbarw-ft</property><property>aero/alpha-rad</property>
          <value> {cma:.4f} </value>
        </product>
      </function>
      <function name="aero/coefficient/Cmde">
        <description>Pitch moment due to elevator</description>
        <product>
          <property>aero/qbar-psf</property><property>metrics/Sw-sqft</property>
          <property>metrics/cbarw-ft</property><property>fcs/elevator-pos-rad</property>
          <value> {cmde:.4f} </value>
        </product>
      </function>
      <function name="aero/coefficient/Cmq">
        <description>Pitch damping</description>
        <product>
          <property>aero/qbar-psf</property><property>metrics/Sw-sqft</property>
          <property>metrics/cbarw-ft</property><property>aero/ci2vel</property>
          <property>velocities/q-aero-rad_sec</property>
          <value> -12.0 </value>
        </product>
      </function>
      <function name="aero/coefficient/Cmadot">
        <description>Pitch moment due to alpha rate</description>
        <product>
          <property>aero/qbar-psf</property><property>metrics/Sw-sqft</property>
          <property>metrics/cbarw-ft</property><property>aero/ci2vel</property>
          <property>aero/alphadot-rad_sec</property>
          <value> -4.0 </value>
        </product>
      </function>
    </axis>

    <axis name="YAW">
      <!-- [GEO] Cnb = +CLa_v*(S_v/S)*(l_v/b)*eta = 2.0*0.15*{lvb:.3f}*0.9 = {cnb:.4f} /rad — weathercock
           stability from the row's own fin arm over its own span. Cnr -0.35, Cnp -0.06,
           Cndr {cndr:.4f} /rad, Cnda +0.004 (adverse yaw). -->
      <function name="aero/coefficient/Cnb">
        <description>Yaw moment due to beta</description>
        <product>
          <property>aero/qbar-psf</property><property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property><property>aero/beta-rad</property>
          <value> {cnb:.4f} </value>
        </product>
      </function>
      <function name="aero/coefficient/Cnr">
        <description>Yaw damping</description>
        <product>
          <property>aero/qbar-psf</property><property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property><property>aero/bi2vel</property>
          <property>velocities/r-aero-rad_sec</property>
          <value> -0.350 </value>
        </product>
      </function>
      <function name="aero/coefficient/Cnp">
        <description>Yaw moment due to roll rate</description>
        <product>
          <property>aero/qbar-psf</property><property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property><property>aero/bi2vel</property>
          <property>velocities/p-aero-rad_sec</property>
          <value> -0.060 </value>
        </product>
      </function>
      <function name="aero/coefficient/Cndr">
        <description>Yaw moment due to rudder</description>
        <product>
          <property>aero/qbar-psf</property><property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property><property>fcs/rudder-pos-rad</property>
          <value> {cndr:.4f} </value>
        </product>
      </function>
      <function name="aero/coefficient/Cnda">
        <description>Adverse yaw due to aileron</description>
        <product>
          <property>aero/qbar-psf</property><property>metrics/Sw-sqft</property>
          <property>metrics/bw-ft</property><property>fcs/left-aileron-pos-rad</property>
          <value> 0.0040 </value>
        </product>
      </function>
    </axis>
  </aerodynamics>

</fdm_config>
""".format(cla=cla, ar=row.ar, clmax=clmax,
           clsrc=("[INV from the published %.0f km/h landing/stall speed]" % (row.clmax_v_ms * 3.6)
                  if row.clmax_v_ms > 0 else
                  "[SET from the alpha limit — no landing or stall speed is published, so THIS ROW HAS "
                  "NO TAKEOFF-RUN ANCHOR AND SPAWNS AIRBORNE (recipe §4.5)]"),
           lift=tbl_xml(lift_table(row), 14, "%9.4f  %9.4f"),
           cd0=tbl_xml(cd0, 14, "%9.2f  %9.5f"),
           cdi=tbl_xml(suction_drag_table(row), 14, "%9.4f  %9.5f"),
           blend=tbl_xml(suction_blend_table(row), 14, "%9.4f  %9.5f"),
           k=row.k, clda=0.055 + 0.02 * min(1.0, row.ar / 4.0),
           sm=STATIC_MARGIN, sm100=STATIC_MARGIN * 100.0, cma=cma, cmde=cmde,
           cmderot=cm_de_rotation_demand(row), cmdeabs=abs(cmde),
           lvb=0.40 * row.l_m / row.b_m, cnb=2.0 * 0.15 * (0.40 * row.l_m / row.b_m) * 0.9,
           cndr=-0.6 * 2.0 * 0.15 * (0.40 * row.l_m / row.b_m) * 0.9)

    return xml_safe(header + metrics + mass + ground + prop + fcs + aero)


def engine_xml(row):
    if row.family == "turbofan":
        idle, mil, aug = TF_IDLE, TF_MIL, TF_AUG
        analogy = ("the pinned F-16 model's own F100-PW-229 deck, with its 60k/70k columns continued by "
                   "the ISA density ratio instead of its zero column")
        bpr, tsfc, atsfc = 0.36, 0.74, 2.05
    else:
        idle = tj_surface(TJ_MACH_DRY, augmented=False, idle=True)
        mil = tj_surface(TJ_MACH_DRY, augmented=False)
        aug = tj_surface(TJ_MACH_AUG, augmented=True)
        analogy = ("the recipe's CONSTRUCTED turbojet reference: sigma^0.90 (dry) / sigma^0.85 (aug) "
                   "altitude lapse times the ram curve (1+0.2M^2)^2.5*(Ve-V)/Ve, calibrated on mig21")
        bpr, tsfc, atsfc = 0.0, 0.90, 2.20
    return xml_safe(ENGINE_TPL.format(
        engine=row.engine, name=row.name, key=row.key, count=row.engines, analogy=analogy,
        mil_kn=row.mil_kn, ab_kn=row.ab_kn,
        mil_lbf=row.mil_kn * 1000.0 * 0.2248089 * PERTURB_THRUST,
        ab_lbf=row.ab_kn * 1000.0 * 0.2248089 * PERTURB_THRUST, bpr=bpr, tsfc=tsfc, atsfc=atsfc,
        idle=surface_xml("IdleThrust", idle), mil=surface_xml("MilThrust", mil),
        aug=surface_xml("AugThrust", aug)))


def write_row(root, row):
    d = os.path.join(root, row.key)
    os.makedirs(os.path.join(d, "engine"), exist_ok=True)
    with open(os.path.join(d, "%s.xml" % row.key), "w") as f:
        f.write(deck_xml(row))
    with open(os.path.join(d, "engine", "%s.xml" % row.engine), "w") as f:
        f.write(engine_xml(row))
    with open(os.path.join(d, "engine", "%s-nozzle.xml" % row.engine), "w") as f:
        f.write(NOZZLE_TPL.format(engine=row.engine))
    with open(os.path.join(d, "reset00.xml"), "w") as f:
        f.write(RESET_TPL.format(alt_ft=0.13 * row.l_m * M_FT))


ANCHORS_TPL = """/* GENERATED by tools/gen_air_decks.py --anchors. DO NOT EDIT.
 *
 * WHAT THE HARNESS NEEDS TO FLY A ROW, and nothing else: the limiter pair it is told to hold, the mass
 * the envelope figures are quoted at, the wing the stall speeds come off, and the two schedule bounds.
 * The EXPECTATIONS — every published figure, its band, its source and its tier — are declarations in
 * test/modules/air/envelope.json, which this same generator writes from this same table, so the number
 * the harness flies to and the number it is judged against cannot drift apart.
 *
 * A1Mach and the two limiter settings appear in both files by NECESSITY, not by copy: they bound the
 * climb schedule and set the limiter, and they are also what the row is judged on. */
#ifndef FBAIRANCHORS_H
#define FBAIRANCHORS_H

namespace FlightBox::Test {{

struct FBAirAnchorRow {{
  const char *Key;
  const char *Name;
  double A1Mach, A1AltM;      /* the climb schedule's own cap, and the altitude Vmax is flown at */
  double A5G;                 /* the g LIMITER's setting; 0 = [TODO], the harness then [SET]s 7.0 */
  double AlphaLimitDeg;       /* the alpha LIMITER's setting */
  double EmptyKg, GrossKg, FuelKg;
  double WingAreaM2, ClMax;          /* the wing the 1 g stall speed and the rotation speed come off */
  double ClAtAlphaLimit;             /* the lift the ALPHA LIMITER allows, which is what a pull reaches */
  double PitchStickMax;              /* systems/FBFlightControl's cap [DERIVED], recipe §6.1 */
}};

inline constexpr FBAirAnchorRow kAirAnchors[] = {{
{rows}}};

}} // namespace FlightBox::Test
#endif
"""

# Take-off ground runs published in the catalogue [T4]; 0 = not published for that row.
TAKEOFF_M = {"mig21": 830.0, "mig23": 450.0, "f5e": 610.0}


def anchors_header():
    rows = []
    for r in ANCHORS:
        rows.append('    {"%s", "%s", %.3f, %.1f, %.1f, %.1f, %.1f, %.1f, %.1f, %.2f, %.3f, %.3f, '
                    "%.3f},\n"
                    % (r.key, r.name, r.a1_mach, r.a1_alt_m, r.a5_g, r.alpha_lim_deg, empty_kg(r),
                       r.gross_kg, r.fuel_kg, r.s_m2, cl_max(r), cl_at_alpha_limit(r),
                       pitch_stick_max(r)))
    return ANCHORS_TPL.format(rows="".join(rows))


def empty_kg(r):
    """Two rows publish no empty mass (catalogue A10/D1); the deck's own is then what is left."""
    return r.empty_kg if r.empty_kg > 0.0 else r.gross_kg - r.fuel_kg - 100.0


def cl_at_alpha_limit(r):
    return (cl_alpha(r) * math.sin(math.radians(r.alpha_lim_deg))
            * math.cos(math.radians(r.alpha_lim_deg)) ** 2)


# THE DEVIATION BANDS (recipe §7.1), DERIVED from the one existing generated-from-anchors deck's own
# measured misses and rounded outward. A band is not a target; widening one to admit a deck is the
# falsification the recipe exists to prevent.
BANDS = {"A1": 0.05, "A2": 0.05, "A3": 0.10, "A4": 0.25, "A5": 0.10, "ALPHA": 0.05,
         "TAKEOFF": 0.30, "MASS": 0.01}

CATALOGUE = "[DOC modules/air/catalogue.md §%s]"
RECIPE_BAND = "band [DOC modules/air/flight-model-recipe.md §7.1]"
# A measurement the catalogue publishes NO figure for. It is recorded and reported, never gating —
# recipe step 7 turns these into the row's own FBPilot hooks, and they are DECLARED ACCEPTED MODEL
# PROPERTIES exactly as the MiG-29's 241 deg/s roll rate is.
NO_FIGURE = "[MESS] no published figure; recorded as an accepted model property (recipe §7)"


def declarations_json(decls):
    """One declaration per block, in the shape doc/testing.md §2 shows it — valid JSON that a human
    reads as a table. `json.dump(indent=…)` puts every leaf on its own line and buries the row."""
    def j(v):
        return json.dumps(v, ensure_ascii=False)

    out = ["[\n"]
    for i, d in enumerate(decls):
        out.append("{ %-9s %s,\n  %-9s %s,\n  %-9s %s,\n  %-9s %s,\n  %-9s %s,\n  %-9s %s }%s\n"
                   % ('"subject":', j(d["subject"]), '"claim":', j(d["claim"]),
                      '"measure":', j(d["measure"]), '"expect":', j(d["expect"]),
                      '"source":', j(d["source"]), '"tier":', j(d["tier"]),
                      "," if i + 1 < len(decls) else ""))
    out.append("]\n")
    return "".join(out)


def air_tests():
    """The declarations doc/testing.md §2 asks for, one per anchor per row, from the SAME table the
    decks and the harness header come from. A tier-A row gates; a tier-B row with no `expect.value`
    is a recorded measurement and a visible hole where the catalogue publishes nothing."""
    out = []

    def add(r, anchor, claim, unit, value):
        cat = CATALOGUE % r.key
        published = value is not None
        out.append({
            "subject": "modules/air",
            "claim": "%s (%s): %s" % (r.name, r.key, claim),
            "measure": {"harness": "air-envelope", "args": {"row": r.key, "anchor": anchor}},
            "expect": ({"value": round(value, 4), "unit": unit, "band": BANDS[anchor]} if published
                       else {"value": None, "unit": unit, "band": None}),
            "source": "%s; %s" % (cat, RECIPE_BAND) if published else NO_FIGURE,
            "tier": "A" if published else "B",
        })

    for r in ANCHORS:
        add(r, "A1", "Vmax at %.0f m" % r.a1_alt_m, "M", r.a1_mach)
        add(r, "A2", "Vmax at sea level", "M", r.a2_mach if r.a2_mach > 0.0 else None)
        add(r, "A3", "service ceiling", "m", r.a3_ceil_m)
        # A4 IS JUDGED ONLY WHERE THE PUBLISHED FIGURE IS REACHABLE AT THE WEIGHT THE REST OF THE
        # ANCHOR SET IS FLOWN AT (recipe R11). Every other anchor is quoted at the GROSS weight; the
        # climb rates name no weight, and inverted against the row's own frozen thrust and polar three
        # of them come out BELOW its EMPTY weight — a figure no loading of that aeroplane can reach.
        roc = r.a4_roc_ms if r.a4_roc_ms > 0.0 and a4_weight_kg(r) >= r.gross_kg else None
        add(r, "A4", "rate of climb at sea level", "m/s", roc)
        add(r, "A5", "g held under the limiter (80 deg bank, full aft)", "g",
            r.a5_g if r.a5_g > 0.0 else None)
        add(r, "ALPHA", "alpha held under the limiter (idle decel, full aft)", "deg",
            r.alpha_lim_deg)
        add(r, "TURN", "instantaneous turn rate at the limit", "deg/s", None)
        add(r, "CORNER", "corner speed (the turn-rate maximum)", "kt", None)
        add(r, "ROLL-A", "roll plant pole a at 10 Hz", "-", None)
        add(r, "ROLL-K", "roll plant gain K (peak rate at full stick)", "deg/s", None)
        add(r, "TAKEOFF", "take-off ground run", "m", TAKEOFF_M.get(r.key) or None)
        add(r, "MASS", "deck mass empty + internal fuel + pilot", "kg",
            empty_kg(r) + r.fuel_kg + 100.0)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", default=os.path.join(SIM_DIR, "assets", "aircraft"))
    ap.add_argument("--check", action="store_true",
                    help="regenerate into a temporary tree and diff against --out")
    ap.add_argument("--report", action="store_true", help="print the inverted polar of every row")
    ap.add_argument("--anchors", help="write the harness's flight-parameter header to this path")
    ap.add_argument("--tests", help="write the anchor DECLARATIONS (doc/testing.md §2) to this path")
    ap.add_argument("--only", help="regenerate ONE row only")
    ap.add_argument("--as", dest="as_key", help="write --only's deck under ANOTHER row's key: the "
                    "attribution CONTROL CELL, which swaps the aeroplane and leaves the catalogue "
                    "entry (radar, rounds, tier) exactly where it was")
    ap.add_argument("--cd0", type=float, default=1.0, help="scale the whole CD0 schedule (attribution)")
    ap.add_argument("--e", type=float, default=1.0, help="scale the Oswald factor (attribution)")
    ap.add_argument("--ixx", type=float, default=1.0, help="scale Ixx (attribution)")
    ap.add_argument("--thrust", type=float, default=1.0, help="scale the engine statics (attribution)")
    a = ap.parse_args()

    if a.anchors:
        with open(a.anchors, "w") as f:
            f.write(anchors_header())
        print("gen_air_decks: anchors -> %s" % a.anchors)
        return 0

    if a.tests:
        decls = air_tests()
        with open(a.tests, "w") as f:
            f.write(declarations_json(decls))
        gating = sum(1 for d in decls if d["tier"] == "A")
        print("gen_air_decks: %d declaration(s), %d gating -> %s" % (len(decls), gating, a.tests))
        return 0

    if a.report:
        print("%-7s %6s %7s %8s %9s %9s  %s" %
              ("row", "AR", "k", "CD0sub", "CD0(A2)", "CD0(A1)", "note"))
        for r in ANCHORS:
            tbl, prov = cd0_table(r)
            sub = tbl[0][1]
            a2 = r.invert_cd0(r.a2_mach, 0.0)[0] if r.a2_mach > 0 else float("nan")
            a1 = r.invert_cd0(r.a1_mach, r.a1_alt_m)[0]
            print("%-7s %6.3f %7.4f %8.4f %9.4f %9.4f  %s"
                  % (r.key, r.ar, r.k, sub, a2, a1, r.family))
        return 0

    root = a.out
    if a.check:
        root = tempfile.mkdtemp(prefix="fbdecks")
    # THE ATTRIBUTION PERTURBATIONS of doc/modules/air/module.md §Spec 11 instrument 2. They move
    # exactly the four quantities the recipe DECLARES it does not know -- CD0, e, Ixx and the thrust
    # statics -- and nothing else: the band measures declared ignorance, not arbitrary noise.
    global OSWALD_E, PERTURB_CD0, PERTURB_IXX, PERTURB_THRUST
    OSWALD_E *= a.e
    PERTURB_CD0, PERTURB_IXX, PERTURB_THRUST = a.cd0, a.ixx, a.thrust
    for r in ANCHORS:
        if a.only and r.key != a.only:
            continue
        r.k = 1.0 / (math.pi * r.ar * OSWALD_E)
        if a.as_key:
            r.key = a.as_key
            r.note = ("THE ATTRIBUTION CONTROL CELL: this file is ANOTHER row's aeroplane written under "
                      "this row's key, so the campaign geometry flies the same sensors, the same "
                      "rounds and the same pilot tier over a different deck. It is not the recipe's "
                      "output for this row and must never be committed.")
        write_row(root, r)
    if a.check:
        bad = 0
        for r in ANCHORS:
            p = subprocess.run(["diff", "-r", os.path.join(a.out, r.key), os.path.join(root, r.key)],
                               capture_output=True, text=True)
            if p.returncode != 0:
                print("gen_air_decks: %s differs from the recipe:\n%s" % (r.key, p.stdout[:2000]))
                bad += 1
        shutil.rmtree(root)
        if bad:
            print("gen_air_decks: FAILED — %d deck(s) are not what the recipe produces" % bad,
                  file=sys.stderr)
            return 1
        print("gen_air_decks: %d deck(s) match the recipe byte for byte" % len(ANCHORS))
        return 0
    print("gen_air_decks: %d deck(s) -> %s" % (len(ANCHORS), root))
    return 0


if __name__ == "__main__":
    sys.exit(main())
