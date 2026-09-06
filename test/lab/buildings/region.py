"""WHERE a building stands, as BUILDING RULES rather than as a coordinate.

The architecture generator takes an EPOCH and a GEO-COORDINATE and returns a plausible building.
The coordinate decides which building tradition applies: what a wall is made of, how far the eaves
oversail, the pitch below which nobody roofs, how many storeys of masonry the ground carries, and
which epoch table has any meaning here -- `Gruenderzeit` is a word about Central Europe.

WEATHER AND CLIMATE ARE NOT THIS FILE'S BUSINESS. A generator that computes a snow load is a
generator doing a provider's work, and this one did until it was caught: it carried EN 1991-1-3's
altitude formula and a model of the world's seismic belts, evaluated per building. Both are gone.
What is left is the RULE each region actually builds by, as a number with its origin written
beside it -- the alpine 45 degrees is there because the snow load is about 5 kN/m2, and that
sentence belongs in a comment, not in an expression a generator evaluates.

Everything below is a REGISTRY: adding a region is adding one predicate and one entry.
"""


class Region:
    """The local building rules. Every field carries its origin."""

    def __init__(self, name, table, wall, roofs, level_m, bay_m, min_pitch_deg, eaves_m,
                 storeys_masonry, why):
        self.name = name                      # what a reader calls this place
        self.table = table                    # which epoch table applies
        self.wall = wall                      # the vernacular wall: stone, brick, timber, frame
        self.roofs = roofs                    # the roof shapes built here, most common first
        self.level_m = level_m                # a storey, floor to floor
        self.bay_m = bay_m                    # the facade's repeat
        self.min_pitch_deg = min_pitch_deg    # below this the tradition does not roof
        self.eaves_m = eaves_m                # how far the roof oversails
        self.storeys_masonry = storeys_masonry  # before a frame is needed
        self.why = why                        # the sentence a reader needs

    def __repr__(self):
        return f"Region({self.name})"


# THE REGISTRY. Each entry is a predicate over (lat, lon, alt) and the Region it names; the FIRST
# that answers wins, so a specialisation goes above the general case.
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
    (_europe_alpine, dict(name="alpine", min_pitch_deg=45.0,  # the snow load up here is about 5 kN/m2 and 45 degrees is what sheds it
                          table="central-europe", wall="stone",
                          roofs=("gabled", "half-hipped", "hipped", "gambrel"),
                          level_m=3.0, bay_m=3.2, eaves_m=0.9, storeys_masonry=4,
                          why="deep snow and a short building season: a steep roof with a wide "
                              "oversail, stone or rendered rubble below it")),
    (_europe_central, dict(name="central-europe", min_pitch_deg=22.0,  # a tiled roof needs about 22 degrees to stay watertight (DIN 4108's Regeldachneigung for interlocking tiles)
                          table="central-europe", wall="brick",
                           roofs=("gabled", "hipped", "mansard", "flat"),
                           level_m=3.0, bay_m=3.2, eaves_m=0.5, storeys_masonry=6,
                           why="the Gruenderzeit block is this region's own type and the table "
                               "is written for it")),
    (_britain, dict(name="britain", min_pitch_deg=20.0,  # slate goes shallower than tile, and the rain is steady rather than heavy
                          table="central-europe", wall="brick",
                    roofs=("gabled", "hipped", "flat"),
                    level_m=2.8, bay_m=3.0, eaves_m=0.35, storeys_masonry=5,
                    why="a shallower roof than the continent's, brick, and a terrace rather "
                        "than a block")),
    (_europe_nordic, dict(name="nordic", min_pitch_deg=27.0,  # snow, but less of it than the Alps carry, and a timber roof sheds it
                          table="central-europe", wall="timber",
                          roofs=("gabled", "half-hipped", "shed"),
                          level_m=2.8, bay_m=3.0, eaves_m=0.7, storeys_masonry=3,
                          why="timber to hand and snow to shed; masonry stays low")),
    (_mediterranean, dict(name="mediterranean", min_pitch_deg=15.0,  # no snow at all, so the pitch is only a rain rule; the Roman tile has always been low
                          table="mediterranean", wall="stone",
                          roofs=("hipped", "flat", "gabled"),
                          level_m=3.2, bay_m=3.4, eaves_m=0.6, storeys_masonry=5,
                          why="no snow, so the pitch is a rain rule; a low tiled hip over "
                              "rubble stone, and a flat roof where it never freezes")),
    (_east_asia, dict(name="east-asia", min_pitch_deg=18.0,  # monsoon rain rather than snow, and a wide eaves oversail does the work
                          table="east-asia", wall="frame",
                      roofs=("hipped", "gabled", "flat"),
                      level_m=3.0, bay_m=3.6, eaves_m=1.1, storeys_masonry=2,
                      why="earthquakes: a timber or steel FRAME, masonry only for outbuildings, "
                          "and an eaves oversail against a monsoon")),
    (_north_america, dict(name="north-america", min_pitch_deg=18.0,  # asphalt shingle's own minimum is about 4:12, which is 18 degrees
                          table="north-america", wall="frame",
                          roofs=("gabled", "hipped", "flat", "gambrel"),
                          level_m=2.9, bay_m=3.6, eaves_m=0.5, storeys_masonry=4,
                          why="balloon and platform framing from the 1830s: a house is a frame, "
                              "and the block is a steel one")),
]

_ANYWHERE = dict(name="anywhere", min_pitch_deg=20.0,  # the pitch a roof of any covering needs to stay watertight, and no more
                          table="central-europe", wall="brick",
                 roofs=("gabled", "hipped", "flat"), level_m=3.0, bay_m=3.2, eaves_m=0.5,
                 storeys_masonry=5,
                 why="no region claims this place, so the rules are the ones that hold "
                     "everywhere: a pitched roof over masonry, sized by the snow that falls here")


def of(lat_deg, lon_deg, alt_m=0.0):
    """The rules where this coordinate is. The first registered region that claims it wins."""
    got = next((r for (claims, r) in REGIONS if claims(lat_deg, lon_deg, alt_m)), _ANYWHERE)
    region = Region(**got)
    region.lat, region.lon, region.alt_m = lat_deg, lon_deg, alt_m
    return region


def describe(region):
    return (f"{region.name}: {region.wall} walls, roofs {'/'.join(region.roofs)}, storey "
            f"{region.level_m:.1f} m, bay {region.bay_m:.1f} m, eaves {region.eaves_m:.2f} m, "
            f"min pitch {region.min_pitch_deg:.0f} deg, masonry to "
            f"{region.storeys_masonry} storeys -- {region.why}")
