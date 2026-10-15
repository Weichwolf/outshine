"""Building Place selection and a demand-view adapter over the shared geospatial core."""

from dataclasses import asdict, replace
from pathlib import Path

from demand import View
from outshine_ref.places import PlaceCamera as SharedCamera
from outshine_ref.places import read_camera

ROOT = Path(__file__).resolve().parents[3]
PLACES = ("Husum", "Feldkirch", "ZuerichHauptbahnhof", "Tokyo")
REFERENCE_PLACES = PLACES + ("DarmstadtWest", "Flensburg", "Rosenheim")
FLAT_HEIGHTS_M = {
    "Husum": 20.0,
    "Feldkirch": 80.0,
    "ZuerichHauptbahnhof": 70.0,
    "Tokyo": 60.0,
}


class PlaceCamera(SharedCamera):
    __slots__ = ()

    def view(self, ground_height_m=None, profile=(1920, 1080)):
        eye = self.eye(ground_height_m)
        forward = -self.basis[:, 2]
        self.project(eye + forward, ground_height_m, profile)
        return View(
            eye,
            eye + forward,
            width=profile[0],
            height=profile[1],
            fov_deg=self.vertical_fov_deg,
        )


def read(name):
    if name not in REFERENCE_PLACES:
        raise ValueError("Unknown fixed Buildings Place")
    return PlaceCamera(
        **asdict(read_camera(ROOT / "src/assets/places" / (name + ".scenario")))
    )


def flat(name):
    return replace(
        read(name), declared_height_m=FLAT_HEIGHTS_M[name], samples_height=False
    )
