"""Read native Place cameras and retain WGS84 east/north/up metres in Python."""

import hashlib
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from pyproj import Transformer

PLACES = ("CentralPark", "Koerbersee", "DarmstadtWest", "Malcesine")
RESOLUTION = (2560, 1440)
_TO_ECEF = Transformer.from_crs(4979, 4978, always_xy=True)
_FROM_ECEF = Transformer.from_crs(4978, 4979, always_xy=True)


def enu_axes(latitude, longitude):
    lat, lon = np.deg2rad((latitude, longitude))
    return np.array(
        (
            (-np.sin(lon), np.cos(lon), 0),
            (-np.sin(lat) * np.cos(lon), -np.sin(lat) * np.sin(lon), np.cos(lat)),
            (np.cos(lat) * np.cos(lon), np.cos(lat) * np.sin(lon), np.sin(lat)),
        )
    )


@dataclass(frozen=True)
class GeographicFrame:
    latitude: float
    longitude: float

    def __post_init__(self):
        if (
            not np.isfinite((self.latitude, self.longitude)).all()
            or abs(self.latitude) > 90
            or abs(self.longitude) > 180
        ):
            raise ValueError("finite WGS84 frame origin required")

    @property
    def axes(self):
        return enu_axes(self.latitude, self.longitude)

    @property
    def ecef_origin(self):
        return np.array(_TO_ECEF.transform(self.longitude, self.latitude, 0))

    def to_local(self, longitude, latitude, height):
        values = np.broadcast_arrays(longitude, latitude, height)
        if not all(np.isfinite(value).all() for value in values):
            raise ValueError("finite geographic coordinates required")
        xyz = np.stack(_TO_ECEF.transform(*values), axis=-1)
        return (xyz - self.ecef_origin) @ self.axes.T

    def to_geographic(self, points):
        points = np.asarray(points, dtype=float)
        if points.shape[-1:] != (3,) or not np.isfinite(points).all():
            raise ValueError("finite ENU triples required")
        xyz = points @ self.axes + self.ecef_origin
        return np.stack(_FROM_ECEF.transform(*np.moveaxis(xyz, -1, 0)), axis=-1)


@dataclass(frozen=True)
class PlaceCamera:
    name: str
    scenario_sha256: str
    frame: GeographicFrame
    latitude: float
    longitude: float
    height_m: float
    samples_height: bool
    bearing_deg: float
    pitch_deg: float
    fov_y_deg: float
    sight_m: float
    clock_utc: str

    def eye(self, ground_asl_m):
        height = self.height_m + (ground_asl_m if self.samples_height else 0)
        return self.frame.to_local(self.longitude, self.latitude, height)

    @property
    def basis(self):
        bearing, pitch = np.deg2rad((self.bearing_deg, self.pitch_deg))
        horizontal = np.array((np.sin(bearing), np.cos(bearing), 0))
        forward = horizontal * np.cos(pitch) + (0, 0, np.sin(pitch))
        up = np.array((0, 0, np.cos(pitch))) - horizontal * np.sin(pitch)
        local = np.column_stack((np.cross(forward, up), up, -forward))
        return self.frame.axes @ enu_axes(self.latitude, self.longitude).T @ local

    def project(self, points, ground_asl_m, resolution=RESOLUTION):
        points = np.asarray(points, dtype=float)
        if points.shape[-1:] != (3,) or not np.isfinite(points).all():
            raise ValueError("finite ENU triples required")
        camera = (points - self.eye(ground_asl_m)) @ self.basis
        depth = -camera[..., 2]
        scale = np.tan(np.deg2rad(self.fov_y_deg) / 2)
        extent = np.array((resolution[0] / resolution[1], 1)) * scale
        ndc = np.full(camera[..., :2].shape, np.nan)
        np.divide(
            camera[..., :2],
            depth[..., None] * extent,
            out=ndc,
            where=depth[..., None] > 0,
        )
        pixels = (ndc * (1, -1) + 1) * np.array(resolution) / 2
        return pixels, depth


def read_camera(path):
    raw = path.read_bytes()
    document = ET.fromstring(raw)
    world, clock = document.find("world"), document.find("clock")
    views = document.findall("views/view")
    if world is None or clock is None or len(views) != 1:
        raise ValueError("one native geographic Place camera and fixed clock required")
    view = views[0]
    at = view.find("at")
    if (
        at is None
        or clock.get("live") != "no"
        or at.get("samplesHeight") not in ("yes", "no")
    ):
        raise ValueError("explicit height reference and fixed clock required")
    values = [
        float(at.get(key))
        for key in ("lat", "lon", "heightM", "bearingDeg", "pitchDeg")
    ]
    fov, sight = float(view.get("fovDeg")), float(world.get("sightM"))
    lat, lon, height, bearing, pitch = values
    if (
        not np.isfinite((*values, fov, sight)).all()
        or abs(lat) > 90
        or abs(lon) > 180
        or abs(pitch) >= 90
        or not 0 < fov < 180
        or sight <= 0
    ):
        raise ValueError("finite geographic pose and perspective lens required")
    return PlaceCamera(
        path.stem,
        hashlib.sha256(raw).hexdigest(),
        GeographicFrame(float(world.get("lat")), float(world.get("lon"))),
        lat,
        lon,
        height,
        at.get("samplesHeight") == "yes",
        bearing,
        pitch,
        fov,
        sight,
        clock.get("start"),
    )


def selected_cameras(root):
    return tuple(
        read_camera(Path(root) / "src/assets/places" / f"{name}.scenario")
        for name in PLACES
    )
