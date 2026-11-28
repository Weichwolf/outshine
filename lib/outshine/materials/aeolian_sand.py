"""Regional reflectance for mapped aeolian sand and the tread through it."""

from dataclasses import dataclass

import shapely

SAND_DETAIL = "aeolian sand"
TREAD_DETAIL = "aeolian sand tread"
SAND_TRAIL_SHOULDER_M = 0.70


@dataclass(frozen=True)
class SandProfile:
    sand: str
    tread: str
    dry: tuple[float, float, float]
    wet: tuple[float, float, float] | None = None

    @property
    def surfaces(self):
        return {
            self.sand: (self.dry, 0.91),
            self.tread: (tuple(channel * 0.93 for channel in self.dry), 0.88),
        }


SAND_PROFILES = {
    103: SandProfile("namib dune sand", "namib sand tread", (0.52, 0.220, 0.093)),
    485: SandProfile(
        "restinga dune sand",
        "restinga sand tread",
        (0.67, 0.602, 0.485),
        (0.35, 0.285, 0.216),
    ),
}


def mapped_sand(features):
    areas = []
    for layer, geometry, tags in features:
        if layer == "landcover" and tags.get("class") == "sand":
            valid = shapely.make_valid(geometry)
            areas.extend(shapely.get_parts(valid))
    return shapely.union_all([area for area in areas if area.area > 0])
