"""WHERE a building stands, as rules rather than as a coordinate.

The architecture generator takes an EPOCH and a GEO-COORDINATE and returns a plausible building.
The coordinate is not decoration: it decides the snow a roof must shed, hence the pitch below
which nobody builds; the seismic zone, hence how tall masonry may go before it is framed; the
material anyone could get to the site before the railway; and which epoch table applies at all,
because `Gruenderzeit` is a word about Central Europe and means nothing in Kyoto.

THE MODEL IS COARSE AND SAYS SO. A snow load is a national annex to EN 1991-1-3 with a map behind
it, and what stands here is the annex's own ALTITUDE FORMULA with the zone taken from latitude --
right in shape, wrong in detail, and stated so that a better source replaces it without touching
anything that reads it. Everything below is a REGISTRY: adding a region is adding one entry, and
nothing else in the tree changes.
"""
import math


class Region:
    """The local rules. Every field carries its origin."""

    def __init__(self, name, table, wall, roofs, level_m, bay_m, min_pitch_deg, eaves_m,
                 storeys_masonry, why):
        self.name = name                      # what a reader calls this place
        self.table = table                    # which epoch table applies
        self.wall = wall                      # the vernacular wall: stone, brick, timber, frame
        self.roofs = roofs                    # the roof shapes built here, most common first
        self.level_m = level_m                # a storey, floor to floor
        self.bay_m = bay_m                    # the facade's repeat
        self.min_pitch_deg = min_pitch_deg    # below this a roof does not shed what falls on it
        self.eaves_m = eaves_m                # how far the roof oversails
        self.storeys_masonry = storeys_masonry  # before a frame is needed
        self.why = why                        # the sentence a reader needs

    def __repr__(self):
        return f"Region({self.name})"


def snow_kn_m2(lat_deg, alt_m):
    """The characteristic ground snow load, EN 1991-1-3's altitude form.

    The German annex gives sk = 0.19 + 0.91 ((A + 140) / 760)^2 for zone 2, and the other zones
    are the same shape with another constant. The ZONE is a map; taken here from latitude, which
    is right in shape and wrong in detail -- a coast at 54 N carries less than an alpine valley
    at 46 N, and only the altitude term sees that. Stated so the map replaces it later."""
    band = max(0.0, (abs(lat_deg) - 35.0) / 25.0)       # 0 at 35 deg, 1 at 60 deg
    base = 0.10 + 0.55 * min(band, 1.6)
    # THE ALTITUDE TERM IS A MID-LATITUDE TERM. Height brings snow because it brings the freezing
    # level within reach, and that level is about 4 to 5 km at the equator and at the ground by
    # 45 to 50 degrees. Applied unconditionally it gave NAIROBI 6.00 kN/m2 and a 45 degree
    # minimum pitch at 1 795 m on the equator (measured). Only the height ABOVE the winter
    # freezing line counts, and 3 500 m falling linearly to zero at 45 degrees is the shape of it.
    freeze_m = 3500.0 * max(0.0, 1.0 - abs(lat_deg) / 45.0)
    above = max(0.0, max(alt_m, 0.0) - freeze_m)
    return base + 0.91 * ((above + 140.0) / 760.0) ** 2


def min_pitch_deg(snow):
    """A roof sheds what falls on it or it carries it. Below about 0.65 kN/m2 a flat roof is
    ordinary; past 2 kN/m2 the vernacular is steep and always has been (the alpine 45 degrees is
    a snow rule before it is a style). Linear between, which is what the tables look like."""
    if snow <= 0.65:
        return 5.0
    if snow >= 2.0:
        return 45.0
    return 5.0 + (snow - 0.65) / (2.0 - 0.65) * 40.0


def seismic(lat_deg, lon_deg):
    """A coarse membership of the world's active belts, as a factor from 0 (stable) to 1.

    The belts are the Circum-Pacific, the Alpide from the Mediterranean through the Himalaya, and
    the mid-Atlantic. A factor, not a design value: what it decides here is how many storeys of
    unreinforced masonry the vernacular allows, which is 5 or 6 where nothing shakes and 2 or 3
    where it does."""
    def near(la, lo, half):
        return max(0.0, 1.0 - math.hypot(lat_deg - la, (lon_deg - lo) * 0.6) / half)

    belts = [(37.0, 140.0, 14.0), (35.0, 25.0, 12.0), (41.0, 15.0, 8.0), (36.0, -119.0, 10.0),
             (-20.0, -70.0, 14.0), (28.0, 85.0, 14.0), (14.0, 121.0, 12.0), (64.0, -20.0, 8.0)]
    return min(1.0, max(near(*b) for b in belts))


# THE REGISTRY. Each entry is a predicate over (lat, lon, alt) and the Region it names; the FIRST that
# answers wins, so a specialisation goes above the general case. Adding a region is one entry.
def _europe_central(lat, lon, alt):
    return 45.0 <= lat <= 56.0 and -2.0 <= lon <= 25.0


def _europe_alpine(lat, lon, alt):
    # ALPINE IS A HEIGHT, NOT A BOX. A latitude-longitude rectangle from Lyon to Vienna made
    # BERN alpine at 540 m and therefore a FRAME building, because a four-storey masonry cap is
    # a mountain village's rule and not a capital's (measured 2026-09-06 on the Bundeshaus). The
    # criterion is the one that produces the vernacular: enough height for real snow.
    return 44.5 <= lat <= 48.5 and 5.0 <= lon <= 16.0 and alt >= 800.0


def _europe_nordic(lat, lon, alt):
    return lat > 56.0 and -25.0 <= lon <= 33.0


def _mediterranean(lat, lon, alt):
    return 30.0 <= lat < 45.0 and -10.0 <= lon <= 40.0


def _britain(lat, lon, alt):
    return 49.5 <= lat <= 61.0 and -11.0 <= lon <= 2.0


def _north_america(lat, lon, alt):
    return 24.0 <= lat <= 60.0 and -170.0 <= lon <= -52.0


def _east_asia(lat, lon, alt):
    return 20.0 <= lat <= 46.0 and 100.0 <= lon <= 146.0


REGIONS = [
    (_europe_alpine, dict(name="alpine", table="central-europe", wall="stone",
                          roofs=("gabled", "half-hipped", "hipped", "gambrel"),
                          level_m=3.0, bay_m=3.2, eaves_m=0.9, storeys_masonry=4,
                          why="deep snow and a short building season: a steep roof with a wide "
                              "oversail, stone or rendered rubble below it")),
    (_europe_central, dict(name="central-europe", table="central-europe", wall="brick",
                           roofs=("gabled", "hipped", "mansard", "flat"),
                           level_m=3.0, bay_m=3.2, eaves_m=0.5, storeys_masonry=6,
                           why="the Gruenderzeit block is this region's own type and the table "
                               "is written for it")),
    (_britain, dict(name="britain", table="central-europe", wall="brick",
                    roofs=("gabled", "hipped", "flat"),
                    level_m=2.8, bay_m=3.0, eaves_m=0.35, storeys_masonry=5,
                    why="a shallower roof than the continent's, brick, and a terrace rather "
                        "than a block")),
    (_europe_nordic, dict(name="nordic", table="central-europe", wall="timber",
                          roofs=("gabled", "half-hipped", "shed"),
                          level_m=2.8, bay_m=3.0, eaves_m=0.7, storeys_masonry=3,
                          why="timber to hand and snow to shed; masonry stays low")),
    (_mediterranean, dict(name="mediterranean", table="mediterranean", wall="stone",
                          roofs=("hipped", "flat", "gabled"),
                          level_m=3.2, bay_m=3.4, eaves_m=0.6, storeys_masonry=5,
                          why="no snow, so the pitch is a rain rule; a low tiled hip over "
                              "rubble stone, and a flat roof where it never freezes")),
    (_east_asia, dict(name="east-asia", table="east-asia", wall="frame",
                      roofs=("hipped", "gabled", "flat"),
                      level_m=3.0, bay_m=3.6, eaves_m=1.1, storeys_masonry=2,
                      why="earthquakes: a timber or steel FRAME, masonry only for outbuildings, "
                          "and an eaves oversail against a monsoon")),
    (_north_america, dict(name="north-america", table="north-america", wall="frame",
                          roofs=("gabled", "hipped", "flat", "gambrel"),
                          level_m=2.9, bay_m=3.6, eaves_m=0.5, storeys_masonry=4,
                          why="balloon and platform framing from the 1830s: a house is a frame, "
                              "and the block is a steel one")),
]

_ANYWHERE = dict(name="anywhere", table="central-europe", wall="brick",
                 roofs=("gabled", "hipped", "flat"), level_m=3.0, bay_m=3.2, eaves_m=0.5,
                 storeys_masonry=5,
                 why="no region claims this place, so the rules are the ones that hold "
                     "everywhere: a pitched roof over masonry, sized by the snow that falls here")


def of(lat_deg, lon_deg, alt_m=0.0):
    """The rules where this coordinate is. The first registered region that claims it wins."""
    got = next((r for (claims, r) in REGIONS if claims(lat_deg, lon_deg, alt_m)), _ANYWHERE)
    snow = snow_kn_m2(lat_deg, alt_m)
    quake = seismic(lat_deg, lon_deg)
    region = Region(min_pitch_deg=max(got.get("min_pitch_deg", 0.0), min_pitch_deg(snow)),
                    **{k: v for k, v in got.items() if k != "min_pitch_deg"})
    region.snow_kn_m2 = snow
    region.seismic = quake
    # a shaking ground takes storeys off masonry before anything else does
    region.storeys_masonry = max(2, int(round(region.storeys_masonry * (1.0 - 0.55 * quake))))
    region.lat, region.lon, region.alt_m = lat_deg, lon_deg, alt_m
    return region


def describe(region):
    return (f"{region.name}: {region.wall} walls, roofs {'/'.join(region.roofs)}, storey "
            f"{region.level_m:.1f} m, bay {region.bay_m:.1f} m, eaves {region.eaves_m:.2f} m, "
            f"snow {region.snow_kn_m2:.2f} kN/m2 -> min pitch {region.min_pitch_deg:.0f} deg, "
            f"seismic {region.seismic:.2f} -> masonry to {region.storeys_masonry} storeys")
