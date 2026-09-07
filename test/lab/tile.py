"""THE TILE THE ENGINE BUILDS IN, IN PYTHON. `src/generators/base/Tile.h`, line for line.

OSM ARRIVES IN TILES AND SO SHOULD THE GEOMETRY. A town twin built in one piece stood at 686 MB
of working set for 43 MB of mesh (measured 2026-09-07 on OldTown), because every generator's
intermediate -- a road solve's sparse system, a CDT's point set, a body's own lists -- was alive
at once. Built tile by tile, the peak is ONE tile's working set and the rest is the geometry it
already handed over, which is exactly why the engine streams: `Generators::Tile`, `TilePool`,
`TerrainTiles`.

A tile carries its own SEED, and that is the other half of why it is the unit: a generator that
seeds from the tile makes the same street whether the camera came from the north or the south,
which is what DETERMINISM means for a world nobody authored.

WHAT A TILE COSTS, which is the arithmetic a build is held to. At Rothenburg's latitude:

    zoom 12   6 417 m across      a quarter of a city
    zoom 13   3 208 m
    zoom 14   1 604 m             the DEM's own zoom in this lab
    zoom 15     802 m
    zoom 16     401 m             a few blocks -- the build unit

A place twin at a 90 m reach is a fifth of one zoom-16 tile and carries 1.75 M triangles; a full
zoom-16 tile of a dense old town is therefore of the order of 40 M triangles and 1 GB of mesh,
which is the number that says a tile is built at a RUNG and not at L3.
"""
import math

# the door's own constants, from `include/math/Units.h`
kDegPerTurn = 360.0
kDegPerHalfTurn = 180.0
kDeg2Rad = math.pi / kDegPerHalfTurn
kRad2Deg = kDegPerHalfTurn / math.pi
kMPerDegLon = 111320.0
kMPerDegLat = 111132.0

_GOLDEN = 0x9E3779B97F4A7C15
_SPLIT_FIRST = 0xBF58476D1CE4E5B9
_SPLIT_SECOND = 0x94D049BB133111EB
_MASK = (1 << 64) - 1
_ZOOM_SHIFT = 58
_COLUMN_SHIFT = 29


def _mix(v):
    """SplitMix64, exactly as `Tile.cpp` does it -- the same seed for the same tile on both sides."""
    v = (v + _GOLDEN) & _MASK
    v = ((v ^ (v >> 30)) * _SPLIT_FIRST) & _MASK
    v = ((v ^ (v >> 27)) * _SPLIT_SECOND) & _MASK
    return v ^ (v >> 31)


def _lat_deg(y, zoom):
    n = math.pi - 2.0 * math.pi * y / float(1 << zoom)
    return kRad2Deg * math.atan(math.sinh(n))


def _lon_deg(x, zoom):
    return x / float(1 << zoom) * kDegPerTurn - kDegPerHalfTurn


class Tile:
    """One slippy-map tile: its anchor, its span in metres, its own frame and its own seed."""

    __slots__ = ("zoom", "x", "y", "seed", "anchor_lat", "anchor_lon", "span_em", "span_nm")

    def __init__(self, zoom, x, y):
        self.zoom, self.x, self.y = int(zoom), int(x), int(y)
        self.seed = _mix(((self.zoom << _ZOOM_SHIFT) ^ ((self.x & 0xFFFFFFFF) << _COLUMN_SHIFT)
                          ^ (self.y & 0xFFFFFFFF)) & _MASK)
        south, north = _lat_deg(self.y + 1, self.zoom), _lat_deg(self.y, self.zoom)
        west, east = _lon_deg(self.x, self.zoom), _lon_deg(self.x + 1, self.zoom)
        self.anchor_lat, self.anchor_lon = south, west
        self.span_nm = (north - south) * kMPerDegLat
        self.span_em = (east - west) * kMPerDegLon * math.cos(0.5 * (north + south) * kDeg2Rad)

    @staticmethod
    def of(zoom, lat_deg, lon_deg):
        scale = float(1 << int(zoom))
        s = math.sin(lat_deg * kDeg2Rad)
        x = int(math.floor((lon_deg + kDegPerHalfTurn) / kDegPerTurn * scale))
        y = int(math.floor((0.5 - math.log((1.0 + s) / (1.0 - s)) / (4.0 * math.pi)) * scale))
        return Tile(zoom, x, y)

    def seed_of(self, stream):
        """`Tile::Seed(stream)`: one tile, many independent streams, all reproducible."""
        return _mix(self.seed ^ _mix(int(stream) & _MASK))

    def enu(self, lat_deg, lon_deg):
        """A geodetic point in the tile's own East-North frame, anchored at its south-west."""
        east = _wrap180(lon_deg - self.anchor_lon) * kMPerDegLon * math.cos(lat_deg * kDeg2Rad)
        return (east, (lat_deg - self.anchor_lat) * kMPerDegLat)

    def geo(self, east_m, north_m):
        lat = self.anchor_lat + north_m / kMPerDegLat
        return (lat, self.anchor_lon + east_m / (kMPerDegLon * math.cos(lat * kDeg2Rad)))

    def holds(self, east_m, north_m):
        return 0.0 <= east_m < self.span_em and 0.0 <= north_m < self.span_nm

    def neighbours(self, rings=1):
        """The tile and its ring, because a generator that meets its neighbour needs a HALO: the
        road bed measured a seam of 411 mm with none and 4.8 mm at two smoothing lengths."""
        return [Tile(self.zoom, self.x + dx, self.y + dy)
                for dy in range(-rings, rings + 1) for dx in range(-rings, rings + 1)]

    def __repr__(self):
        return f"Tile({self.zoom}/{self.x}/{self.y} {self.span_em:.0f}x{self.span_nm:.0f} m)"


def _wrap180(deg):
    return (deg + kDegPerHalfTurn) % kDegPerTurn - kDegPerHalfTurn


# THE TREE'S OWN ANSWER, frozen. `src/generators/base/Tile.cpp` computes these with the same
# SplitMix64 and the same shifts; the two were compiled side by side on 2026-09-07 and agree bit
# for bit on every zoom and both streams. A lab tile that is not the engine's tile is a lab that
# seeds a different world, so this is a SNAPSHOT check in CLAUDE.md's sense -- agreement with the
# tree, which is exactly what it has to be.
CPP_SEED = {
    (12, 2163, 1400): (0xAFAECDA70F6852D0, 0x4AE400CF3046DA9F),
    (13, 4327, 2800): (0x5AA88D20B37AB1B1, 0xA385A51E7E43D59B),
    (14, 8655, 5600): (0xAAB03BD3571ABEBA, 0x75D7B72BA4EEE8B5),
    (15, 17310, 11200): (0x0BB8B698617A668E, 0x1A5F4949F7C6629C),
    (16, 34621, 22401): (0xF1F6BC92A6894613, 0x4B6FDE7B60850133),
}


def check_seed_matches_cpp():
    """I27: the lab's tile seed IS the engine's. Returns the number that disagree."""
    bad = 0
    for (zoom, x, y), (seed, stream) in CPP_SEED.items():
        t = Tile(zoom, x, y)
        bad += int(t.seed != seed) + int(t.seed_of(7) != stream)
    return bad


def main():
    bad = check_seed_matches_cpp()
    for z in (12, 13, 14, 15, 16):
        t = Tile.of(z, 49.3777, 10.179)
        print(f"  zoom {z:2d}  {t.span_em:7.0f} x {t.span_nm:7.0f} m   seed {t.seed:016x}")
    print(f"tile   {'RED ' + str(bad) + ' seeds differ from the C++' if bad else 'ok'}"
          f"   {len(CPP_SEED)} tiles checked against src/generators/base/Tile.cpp")
    return 1 if bad else 0


if __name__ == "__main__":
    import sys as _sys
    _sys.exit(main())
