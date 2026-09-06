"""WHERE THE EYE IS AND WHERE THE SUN IS -- the two things a picture needs before a renderer.

This file used to carry a flat rasteriser as well: one normal per face, a z-buffer, no smoothing,
built as the geometry's own honest instrument. It is gone. Every picture the lab makes is rendered
by Blender's CYCLES on the GPU now, because a look that is judged has to be judged on the thing a
player would see, and a second renderer is a second answer to the same question -- which is the
defect this tree names first (`blend.py` is the one).

What is left is not rendering: a CAMERA is where a client stands, and the SUN's position is
astronomy. Both belong to the scene and neither belongs to a renderer.
"""
import math

import numpy as np


class Camera:
    """Where the eye is and what it looks at, in the client's own terms."""

    def __init__(self, lat, lon, agl_m=60.0, bearing_deg=0.0, pitch_deg=-6.0, fov_deg=55.0,
                 width=1280, height=720, plan_above_m=None, span_m=0.0):
        self.lat, self.lon = lat, lon
        self.agl_m = agl_m
        self.bearing_deg = bearing_deg
        self.pitch_deg = -90.0 if plan_above_m else pitch_deg
        self.fov_deg = fov_deg
        self.width, self.height = width, height
        self.plan_above_m = plan_above_m
        self.span_m = span_m

    @property
    def orthographic(self):
        """A PLAN IS ORTHOGRAPHIC, and the client says so: `ZurichPlan` sets `Orthographic` with
        `YMagM = span/2`. Drawn in perspective instead, the same declaration gives a different
        picture, and a twin that differs from the client by its projection is not a twin."""
        return self.plan_above_m is not None

    @property
    def y_mag_m(self):
        return 0.5 * self.span_m

    def basis(self):
        """Right, up and forward in the local ENU frame. Bearing is clockwise from north, which
        is the surveyor's convention and the client's."""
        b = math.radians(self.bearing_deg)
        p = math.radians(self.pitch_deg)
        fwd = np.array([math.sin(b) * math.cos(p), math.cos(b) * math.cos(p), math.sin(p)])
        right = np.array([math.cos(b), -math.sin(b), 0.0])
        up = np.cross(right, fwd)
        n = np.linalg.norm(up)
        return right, (up / n if n > 1e-9 else np.array([0.0, 0.0, 1.0])), fwd


    def project(self, point):
        """A world point (in the camera's own translated frame, eye at the origin plus `agl_m`)
        to a PIXEL, or None behind the eye. What a measurement needs when it has to read one
        surface out of a picture rather than the picture as a whole."""
        right, up, fwd = self.basis()
        rel = np.asarray(point, dtype=float) - np.array([0.0, 0.0, self.agl_m])
        ahead = float(np.dot(rel, fwd))
        if ahead <= 1e-6:
            return None
        half = math.tan(math.radians(self.fov_deg) / 2.0)
        x = float(np.dot(rel, right)) / (ahead * half)
        y = float(np.dot(rel, up)) / (ahead * half * self.height / self.width)
        return (0.5 * (1.0 + x) * self.width, 0.5 * (1.0 - y) * self.height)


def sun_direction(lat_deg, lon_deg, when_utc):
    """WHERE THE SUN STANDS, from the place and the hour. Astronomy, not weather: NOAA's own
    low-precision solar position, good to about a tenth of a degree, which is far inside what a
    flat-shaded lab picture can show. `when_utc` is an ISO instant like `2026-06-21T11:19:00Z`."""
    y, mo, d = int(when_utc[0:4]), int(when_utc[5:7]), int(when_utc[8:10])
    hh, mm = int(when_utc[11:13]), int(when_utc[14:16])
    a = (14 - mo) // 12
    jdn = (d + (153 * (mo + 12 * a - 3) + 2) // 5 + 365 * (y + 4800 - a)
           + (y + 4800 - a) // 4 - (y + 4800 - a) // 100 + (y + 4800 - a) // 400 - 32045)
    jd = jdn - 0.5 + (hh + mm / 60.0) / 24.0
    n = jd - 2451545.0
    mean_long = math.radians((280.460 + 0.9856474 * n) % 360.0)
    anomaly = math.radians((357.528 + 0.9856003 * n) % 360.0)
    ecliptic = mean_long + math.radians(1.915) * math.sin(anomaly) \
        + math.radians(0.020) * math.sin(2 * anomaly)
    obliquity = math.radians(23.439 - 0.0000004 * n)
    ra = math.atan2(math.cos(obliquity) * math.sin(ecliptic), math.cos(ecliptic))
    dec = math.asin(math.sin(obliquity) * math.sin(ecliptic))
    gmst = (18.697374558 + 24.06570982441908 * n) % 24.0
    lst = math.radians((gmst * 15.0 + lon_deg) % 360.0)
    ha = lst - ra
    lat = math.radians(lat_deg)
    alt = math.asin(math.sin(lat) * math.sin(dec) + math.cos(lat) * math.cos(dec) * math.cos(ha))
    az = math.atan2(-math.sin(ha) * math.cos(dec),
                    math.cos(lat) * math.sin(dec) - math.sin(lat) * math.cos(dec) * math.cos(ha))
    # ENU: east, north, up
    return np.array([math.cos(alt) * math.sin(az), math.cos(alt) * math.cos(az), math.sin(alt)])
