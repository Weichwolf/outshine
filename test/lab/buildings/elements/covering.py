"""THE ROOF'S COVERING, from which its PITCH follows -- not the other way round.

A roof does not have a pitch because a table says so; it has the pitch its covering needs to stay
watertight, and the covering is what the region and the epoch had to hand. Every number here is a
trade's own minimum (the German trade calls it the Regeldachneigung) and the colour is the
material's, not a taste:

    covering        min pitch   why that number
    reed thatch        45 deg   water must run off the stems before it soaks in
    plain tile         40 deg   a Biberschwanz laps twice and has no interlock at all
    slate              25 deg   a double-lapped slate; below this the wind drives rain up it
    interlocking tile  22 deg   DIN 4108's Regeldachneigung for a Falzziegel
    shingle            18 deg   asphalt's own 4:12, which is North America's default roof
    corrugated metal   10 deg   a long sheet with a sealed lap
    standing seam       3 deg   the seam IS the seal, so the pitch is only drainage
    bitumen / green      2 deg   a membrane, and the fall is there to move water to the outlet

The registry is keyed by name and a region names the coverings it builds with, most common first;
`for_pitch` picks the one a given pitch admits, which is how an OSM `roof:shape` with no covering
tag still gets the right material.
"""
COVERINGS = {}


def register(name, *, min_pitch_deg, rgb, rough=0.85, note=""):
    COVERINGS[name] = dict(name=name, min_pitch_deg=min_pitch_deg, rgb=rgb,
                           rough=rough, note=note)
    return COVERINGS[name]


register("reet", min_pitch_deg=45.0, rgb=(0.52, 0.42, 0.26), rough=0.98,
         note="thatch: the water must leave the stems before it soaks in")
register("biberschwanz", min_pitch_deg=40.0, rgb=(0.55, 0.28, 0.20),
         note="a plain clay tile, double lapped, no interlock")
register("schiefer", min_pitch_deg=25.0, rgb=(0.22, 0.23, 0.25), rough=0.55,
         note="double-lapped slate, and the alpine and British default")
register("falzziegel", min_pitch_deg=22.0, rgb=(0.58, 0.29, 0.19),
         note="DIN 4108's Regeldachneigung: the interlocking clay tile")
register("betondachstein", min_pitch_deg=22.0, rgb=(0.35, 0.34, 0.33),
         note="the concrete tile, anthracite, which is what post-1960 Europe roofs with")
register("schindel", min_pitch_deg=18.0, rgb=(0.34, 0.30, 0.26),
         note="asphalt shingle at 4:12, North America's own")
register("wellblech", min_pitch_deg=10.0, rgb=(0.46, 0.47, 0.47), rough=0.45,
         note="corrugated sheet with a sealed lap: the hall's and the shed's")
register("stehfalz", min_pitch_deg=3.0, rgb=(0.40, 0.41, 0.42), rough=0.28,
         note="standing seam: the seam is the seal, so the pitch is only drainage")
register("kupfer", min_pitch_deg=3.0, rgb=(0.29, 0.47, 0.40), rough=0.35,
         note="copper, gone green -- a dome, a spire and a civic roof")
register("bitumen", min_pitch_deg=2.0, rgb=(0.24, 0.24, 0.24), rough=0.92,
         note="a membrane; the fall only moves water to the outlet")
register("gruendach", min_pitch_deg=2.0, rgb=(0.31, 0.36, 0.24), rough=0.98,
         note="a green roof, which a contemporary flat roof usually is")

# WHAT A REGION ROOFS WITH, most common first. A covering is a local answer to local weather and
# local material, which is why this belongs beside the region and not beside the epoch.
BY_REGION = {
    "alpine": ("schiefer", "biberschwanz", "stehfalz"),
    "central-europe": ("falzziegel", "biberschwanz", "betondachstein", "schiefer"),
    "britain": ("schiefer", "falzziegel", "bitumen"),
    "nordic": ("falzziegel", "stehfalz", "reet"),
    "mediterranean": ("falzziegel", "biberschwanz", "bitumen"),
    "north-america": ("schindel", "stehfalz", "bitumen"),
    "east-asia": ("falzziegel", "stehfalz", "bitumen"),
    "anywhere": ("falzziegel", "betondachstein", "bitumen"),
}

# AND WHAT A USE ROOFS WITH, where the use overrules the region: a factory is sheeted wherever it
# stands, a dome is copper, a flat roof is a membrane.
BY_EPOCH = {
    "industrial": ("wellblech", "stehfalz", "bitumen"),
    "hall": ("stehfalz", "wellblech", "bitumen"),
    "commercial": ("bitumen", "stehfalz"),
    "tower": ("bitumen", "stehfalz"),
    "contemporary": ("gruendach", "bitumen", "stehfalz"),
    "farm": ("biberschwanz", "falzziegel", "wellblech"),
    "sacral": ("kupfer", "schiefer", "biberschwanz"),
    "gothic": ("schiefer", "kupfer"),
    "baroque": ("kupfer", "biberschwanz", "schiefer"),
}


def for_pitch(pitch_deg, region_name, epoch, seed=0):
    """The covering this roof is laid with: the first one its region or its use builds with that
    the pitch admits. A pitch below every covering's minimum is a FLAT roof and takes a membrane."""
    order = list(BY_EPOCH.get(epoch, ())) + list(BY_REGION.get(region_name, BY_REGION["anywhere"]))
    fits = [COVERINGS[n] for n in order if COVERINGS[n]["min_pitch_deg"] <= pitch_deg + 1e-9]
    if not fits:
        return COVERINGS["bitumen"]
    return fits[seed % min(2, len(fits))] if len(fits) > 1 else fits[0]


def min_pitch_for(region_name, epoch):
    """The shallowest pitch this place and this use can roof at, which is the bound a generator
    owes: a 12 degree roof in a region that only knows plain tile is a roof that leaks."""
    order = list(BY_EPOCH.get(epoch, ())) + list(BY_REGION.get(region_name, BY_REGION["anywhere"]))
    return min(COVERINGS[n]["min_pitch_deg"] for n in order) if order else 22.0
