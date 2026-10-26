"""Historical four-scene controls, including an explicitly archived camera."""

from pathlib import Path

from outshine_ref.places import PlaceCamera, read_camera

PLACES = ("CentralPark", "Koerbersee", "DarmstadtWest", "Malcesine")
SCENARIOS = (
    "test/outshine/data/places/CentralPark.scenario",
    "test/outshine/data/places/Koerbersee.scenario",
    "test/outshine/data/diagnostic-places/DarmstadtWest.scenario",
    "test/outshine/data/places/Malcesine.scenario",
)
RESOLUTION = (2560, 1440)
__all__ = ["PLACES", "RESOLUTION", "PlaceCamera", "read_camera", "selected_cameras"]


def selected_cameras(root):
    return tuple(read_camera(Path(root) / path) for path in SCENARIOS)
