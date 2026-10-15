"""Vegetation scene selection over the shared native Place camera contract."""

from pathlib import Path

from outshine_ref.places import PlaceCamera, read_camera

PLACES = ("CentralPark", "Koerbersee", "DarmstadtWest", "Malcesine")
RESOLUTION = (2560, 1440)
__all__ = ["PLACES", "RESOLUTION", "PlaceCamera", "read_camera", "selected_cameras"]


def selected_cameras(root):
    return tuple(
        read_camera(Path(root) / "src/assets/places" / f"{name}.scenario")
        for name in PLACES
    )
