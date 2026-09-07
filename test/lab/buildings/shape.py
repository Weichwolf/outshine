"""THE ROOF FOLLOWS THE PLAN, and this is `src/generators/building/BuildingShape.cpp`, copied.

The lab chose a roof from an EPOCH: `classify` inferred a period from the tags and the style
offered a list, of which `roof_for` took the FIRST. The footprint never entered the choice.
Measured 2026-09-07 over sixteen real plans: nine got `flat`, among them a terraced house, a
bungalow, a school and a town hall tower -- because everything without a `start_date` falls into
`late20`, whose list begins with `flat`. That is why a town of these reads as grey boxes.

The C++ decides from the GEOMETRY and knows no epoch at all. Its rule is copied here constant for
constant, so the two cannot drift and a correction made in the lab is a correction that can be
carried across as one. Where the lab later adds detail it ADDS -- it does not replace the choice.

    UseOf     area < 26 m2                                  -> Outbuilding
              height > 21 m and area < 260 m2               -> Spire
              height > 19 m                                 -> Tower
              area > 1300 m2                                -> Hall
              area > 380 m2                                 -> Block
              aspect > 2.2 and area > 90 m2                  -> Terrace
              otherwise                                     -> House

    RoofOf    reads as round                                -> Dome
              Outbuilding  Shed | Flat        Spire  Hip | Flat        Tower  Flat
              Hall         Sawtooth where aspect > 1.8 and area > 2600 m2, else Flat
              Block        Mansard from 4 storeys, else Gable over aspect 1.9, else Hip
              Terrace      Gable | Flat       House   Gable from aspect 1.30, else Hip

`pitchable` is the REGION's word and not the plan's: where the share of pitched roofs around is
known it decides, and where it is not the footprint's own fill stands in -- a plan that fills its
box carries a pitched roof, a ragged one does not.
"""
import math

# `BuildingShape.cpp`, line for line. A number changed here without changing it there is a defect.
OUTBUILDING_UNDER_M2 = 26.0
SPIRE_OVER_M, SPIRE_UNDER_M2 = 21.0, 260.0
TOWER_OVER_M = 19.0
HALL_OVER_M2 = 1300.0
BLOCK_OVER_M2 = 380.0
TERRACE_OVER_ASPECT, TERRACE_OVER_M2 = 2.2, 90.0

ROUND_LEAST_CORNERS = 8
ROUND_OVER_FILL, ROUND_UNDER_FILL = 0.70, 0.84
ROUND_UNDER_ASPECT = 1.30

PITCHED_MAJORITY = 0.5
PITCHABLE_FROM_FILL = 0.74
SAWTOOTH_OVER_ASPECT, SAWTOOTH_OVER_M2 = 1.8, 2600.0
MANSARD_FROM_STOREYS = 4
BLOCK_GABLE_OVER_ASPECT = 1.9
HOUSE_GABLE_FROM_ASPECT = 1.30

PITCH_HOUSE_DEG = 42.0
PITCH_OUTBUILDING_DEG = 22.0
PITCH_HALL_DEG = 6.0
PITCH_SPIRE_DEG = 62.0

# `RoofKind` against the lab's registry names. The registry carries `c_kind` for exactly this,
# so the two tables meet at the C++'s own word rather than at a second mapping.
KIND = {"Flat": "flat", "Gable": "gabled", "Hip": "hipped", "Shed": "skillion",
        "Mansard": "mansard", "Sawtooth": "sawtooth", "Dome": "dome"}

OUTBUILDING, SPIRE, TOWER, HALL, BLOCK, TERRACE, HOUSE = (
    "Outbuilding", "Spire", "Tower", "Hall", "Block", "Terrace", "House")


def measured(poly):
    """`AreaM2`, `HalfUm`, `HalfVm`, `Fill` and `Aspect` as the C++ computes them: the half-widths
    are the minimum rotated rectangle's, the fill is the area over that box, and the aspect is
    the long half over the short one."""
    area = float(poly.area)
    box = poly.minimum_rotated_rectangle
    xs, ys = box.exterior.coords.xy
    side = [math.dist((xs[i], ys[i]), (xs[i + 1], ys[i + 1])) for i in range(4)]
    half_u, half_v = max(side[0], side[1]) / 2.0, min(side[0], side[1]) / 2.0
    fill = area / max(4.0 * half_u * half_v, 1e-9)
    return area, half_u, half_v, fill, half_u / max(half_v, 1e-9)


def use_of(area_m2, height_m, aspect):
    """`UseOf`: what a mass IS, from its area, its height and its slenderness."""
    if area_m2 < OUTBUILDING_UNDER_M2:
        return OUTBUILDING
    if height_m > SPIRE_OVER_M and area_m2 < SPIRE_UNDER_M2:
        return SPIRE
    if height_m > TOWER_OVER_M:
        return TOWER
    if area_m2 > HALL_OVER_M2:
        return HALL
    if area_m2 > BLOCK_OVER_M2:
        return BLOCK
    if aspect > TERRACE_OVER_ASPECT and area_m2 > TERRACE_OVER_M2:
        return TERRACE
    return HOUSE


def reads_as_round(corners, fill, half_u, half_v):
    """`ReadsAsRound`: many corners, a fill BETWEEN two bounds -- a circle in its box is pi/4 =
    0.785 -- and no long axis."""
    return (corners >= ROUND_LEAST_CORNERS and ROUND_OVER_FILL < fill < ROUND_UNDER_FILL
            and half_u < ROUND_UNDER_ASPECT * half_v)


def roof_of(use, aspect, area_m2, fill, corners, half_u, half_v, storeys, pitched_share=-1.0):
    """`RoofOf`, in the C++'s own order. Returns a name from the lab's registry."""
    if reads_as_round(corners, fill, half_u, half_v):
        return KIND["Dome"]
    pitchable = (pitched_share >= PITCHED_MAJORITY if pitched_share >= 0.0
                 else fill >= PITCHABLE_FROM_FILL)
    if use == OUTBUILDING:
        return KIND["Shed"] if pitchable else KIND["Flat"]
    if use == SPIRE:
        return KIND["Hip"] if pitchable else KIND["Flat"]
    if use == TOWER:
        return KIND["Flat"]
    if use == HALL:
        if not pitchable:
            return KIND["Flat"]
        return (KIND["Sawtooth"] if aspect > SAWTOOTH_OVER_ASPECT and area_m2 > SAWTOOTH_OVER_M2
                else KIND["Flat"])
    if use == BLOCK:
        if not pitchable:
            return KIND["Flat"]
        if storeys >= MANSARD_FROM_STOREYS:
            return KIND["Mansard"]
        return KIND["Gable"] if aspect > BLOCK_GABLE_OVER_ASPECT else KIND["Hip"]
    if use == TERRACE:
        return KIND["Gable"] if pitchable else KIND["Flat"]
    if not pitchable:
        return KIND["Flat"]
    return KIND["Gable"] if aspect >= HOUSE_GABLE_FROM_ASPECT else KIND["Hip"]


def pitch_deg_of(use):
    """`PitchDegOf` without its jitter -- the lab is deterministic and the seed is the caller's."""
    if use == OUTBUILDING:
        return PITCH_OUTBUILDING_DEG
    if use == HALL:
        return PITCH_HALL_DEG
    if use == SPIRE:
        return PITCH_SPIRE_DEG
    return PITCH_HOUSE_DEG


def of(poly, height_m, storeys, pitched_share=-1.0):
    """The whole decision for one footprint: (use, roof, pitch in degrees)."""
    area, half_u, half_v, fill, aspect = measured(poly)
    use = use_of(area, height_m, aspect)
    corners = len(poly.exterior.coords) - 1
    return (use, roof_of(use, aspect, area, fill, corners, half_u, half_v, storeys, pitched_share),
            pitch_deg_of(use))


# the C++'s own switch, stated as cases that go red if a constant moves
assert use_of(20.0, 4.0, 1.0) == OUTBUILDING
assert use_of(100.0, 25.0, 1.0) == SPIRE, "tall and small is a spire before it is a tower"
assert use_of(600.0, 25.0, 1.0) == TOWER
assert use_of(2000.0, 8.0, 1.0) == HALL
assert use_of(500.0, 8.0, 1.0) == BLOCK
assert use_of(200.0, 8.0, 3.0) == TERRACE
assert use_of(200.0, 8.0, 1.0) == HOUSE
assert roof_of(HOUSE, 1.5, 200.0, 0.95, 4, 10.0, 6.0, 2) == "gabled"
assert roof_of(HOUSE, 1.1, 200.0, 0.95, 4, 8.0, 7.0, 2) == "hipped"
assert roof_of(HOUSE, 1.5, 200.0, 0.50, 12, 10.0, 6.0, 2) == "flat", "a ragged plan is not pitched"
assert roof_of(BLOCK, 1.5, 500.0, 0.95, 4, 12.0, 8.0, 4) == "mansard"
assert roof_of(HALL, 2.0, 3000.0, 0.95, 4, 40.0, 20.0, 1) == "sawtooth"
assert roof_of(TOWER, 1.0, 300.0, 0.95, 4, 9.0, 9.0, 8) == "flat"
assert roof_of(HOUSE, 1.0, 150.0, 0.78, 16, 7.0, 6.5, 2) == "dome", "round reads before anything"
