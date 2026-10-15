"""Building Place selection and a demand-view adapter over the shared geospatial core."""

from dataclasses import asdict
from pathlib import Path

from demand import View
from outshine_ref.places import PlaceCamera as SharedCamera
from outshine_ref.places import read_camera

ROOT = Path(__file__).resolve().parents[3]
PLACES = ("Husum", "Feldkirch", "ZuerichHauptbahnhof", "Tokyo")
REFERENCE_PLACES = PLACES + ("DarmstadtWest", "Flensburg", "Rosenheim")


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
