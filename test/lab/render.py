"""THE LAB'S OWN CAMERA: a place, a bearing, a picture -- the geometry's digital twin.

The outshine client stands a camera at a coordinate and draws what is there. This does the same
for what the LAB builds, so the two pictures can be put side by side and the difference read off.
It is deliberately the simplest thing that shows geometry honestly:

    FLAT SHADING      one normal per triangle, Lambert against the sun plus a sky term. No
                      interpolation, because a flat face is exactly what a mesh check sees: a
                      crack, a fold, an inverted normal all SHOW rather than being smoothed away
    A Z-BUFFER        per pixel, so what is behind is behind. A painter's sort is not enough at a
                      junction where a deck passes over a road
    A SIMPLE SKY      a two-colour vertical gradient with the sun's own disc. Not an atmosphere:
                      the atmosphere is the engine's and belongs there, and a lab picture that
                      pretended otherwise would invite a comparison it cannot win

No Blender, no external process, no scene format. The instrument runs in the same interpreter as
the generator that made the mesh, which is what makes it a twin rather than an export.
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


class Scene:
    """Triangles with a colour each, in the local ENU frame: x east, y north, z up."""

    def __init__(self):
        self.vertices = []
        self.tris = []
        self.colours = []

    def add(self, vertices, tris, rgb):
        base = len(self.vertices)
        self.vertices.extend([tuple(map(float, v)) for v in vertices])
        for t in tris:
            self.tris.append((t[0] + base, t[1] + base, t[2] + base))
            self.colours.append(rgb)
        return self

    def counts(self):
        return len(self.vertices), len(self.tris)


SKY_LOW = np.array([0.72, 0.80, 0.90])
SKY_HIGH = np.array([0.28, 0.46, 0.76])


def _sky(width, height, camera):
    """A two-colour vertical gradient in SCREEN space, which is what a fixed pitch makes of a
    vertical gradient in the world. Simple by intent: the engine owns the atmosphere."""
    t = np.linspace(0.0, 1.0, height)[:, None]
    band = SKY_LOW[None, None, :] * (1.0 - t[:, :, None]) + SKY_HIGH[None, None, :] * t[:, :, None]
    return np.repeat(band, width, axis=1)


def render(scene, camera, sun, ambient=0.34, near_m=0.5):
    """The picture. A z-buffer over flat-shaded triangles, on a sky."""
    w, h = camera.width, camera.height
    img = _sky(w, h, camera).astype(np.float64)
    if not scene.tris:
        return (np.clip(img, 0.0, 1.0) * 255).astype(np.uint8)
    zbuf = np.full((h, w), np.inf)

    right, up, fwd = camera.basis()
    eye = np.array([0.0, 0.0, camera.agl_m])
    verts = np.asarray(scene.vertices, dtype=float) - eye
    cam = np.stack([verts @ right, verts @ up, verts @ fwd], axis=1)

    ortho = camera.orthographic
    f = (0.5 * h / max(camera.y_mag_m, 1e-9)) if ortho \
        else (0.5 * h / math.tan(math.radians(camera.fov_deg) * 0.5))
    tris = np.asarray(scene.tris, dtype=np.int64)
    cols = np.asarray(scene.colours, dtype=float)

    sun = np.asarray(sun, dtype=float)
    sun = sun / max(np.linalg.norm(sun), 1e-9)
    world = np.asarray(scene.vertices, dtype=float)

    for k in range(len(tris)):
        ia, ib, ic = tris[k]
        # FLAT SHADING, and the normal is the FACE's own: a fold or an inverted triangle shows.
        # Taken from the WHOLE face before any clipping, because a clipped piece has the same
        # plane and therefore the same normal.
        na = world[ia]
        nvec = np.cross(world[ib] - na, world[ic] - na)
        ln = np.linalg.norm(nvec)
        if ln < 1e-12:
            continue
        nvec = nvec / ln
        lam = max(0.0, float(np.dot(nvec, sun)))
        skyward = 0.5 + 0.5 * float(nvec[2])          # a face turned up sees more sky
        shade = ambient * skyward + (1.0 - ambient) * lam
        rgb = np.clip(cols[k] * shade, 0.0, 1.0)
        for (pa, pb, pc) in _clipped(cam[ia], cam[ib], cam[ic], near_m):
            _raster(img, zbuf, pa, pb, pc, rgb, f, w, h, ortho)
    return (np.clip(img, 0.0, 1.0) * 255).astype(np.uint8)


def _clipped(pa, pb, pc, near_m):
    """THE TRIANGLE, CUT AT THE NEAR PLANE rather than thrown away.

    Dropping a whole face because one vertex is behind the eye loses exactly the faces the eye is
    standing ON. Every roof in the gallery was drawn floating in an empty sky: the ground under the
    camera was two 120 m triangles, each with a corner behind it, so both went (measured
    2026-09-06). Sutherland-Hodgman against the single plane z = near, which leaves a triangle or a
    quad -- and the quad is two triangles."""
    pts = [pa, pb, pc]
    inside = [p[2] > near_m for p in pts]
    if all(inside):
        return ((pa, pb, pc),)
    if not any(inside):
        return ()
    out = []
    for at in range(3):
        here, there = pts[at], pts[(at + 1) % 3]
        if inside[at]:
            out.append(here)
        if inside[at] != inside[(at + 1) % 3]:
            u = (near_m - here[2]) / (there[2] - here[2])
            out.append(here + u * (there - here))
    if len(out) == 3:
        return (tuple(out),)
    if len(out) == 4:
        return ((out[0], out[1], out[2]), (out[0], out[2], out[3]))
    return ()


def _raster(img, zbuf, pa, pb, pc, rgb, f, w, h, ortho):
    """One camera-space triangle onto the buffer, z-tested per pixel."""
    if ortho:
        sa = (w * 0.5 + f * pa[0], h * 0.5 - f * pa[1])
        sb = (w * 0.5 + f * pb[0], h * 0.5 - f * pb[1])
        sc = (w * 0.5 + f * pc[0], h * 0.5 - f * pc[1])
    else:
        sa = (w * 0.5 + f * pa[0] / pa[2], h * 0.5 - f * pa[1] / pa[2])
        sb = (w * 0.5 + f * pb[0] / pb[2], h * 0.5 - f * pb[1] / pb[2])
        sc = (w * 0.5 + f * pc[0] / pc[2], h * 0.5 - f * pc[1] / pc[2])
    x0 = max(0, int(math.floor(min(sa[0], sb[0], sc[0]))))
    x1 = min(w - 1, int(math.ceil(max(sa[0], sb[0], sc[0]))))
    y0 = max(0, int(math.floor(min(sa[1], sb[1], sc[1]))))
    y1 = min(h - 1, int(math.ceil(max(sa[1], sb[1], sc[1]))))
    if x1 < x0 or y1 < y0:
        return
    ax, ay = sa
    bx, by = sb
    cx, cy = sc
    area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay)
    if abs(area) < 1e-9:
        return
    xs = np.arange(x0, x1 + 1) + 0.5
    ys = np.arange(y0, y1 + 1) + 0.5
    gx, gy = np.meshgrid(xs, ys)
    w0 = ((bx - ax) * (gy - ay) - (gx - ax) * (by - ay)) / area
    w1 = ((gx - ax) * (cy - ay) - (cx - ax) * (gy - ay)) / area
    inside = (w0 >= 0) & (w1 >= 0) & (w0 + w1 <= 1)
    if not inside.any():
        return
    u = w1
    v = w0
    t = 1.0 - u - v
    depth = t * pa[2] + u * pb[2] + v * pc[2]
    sub = zbuf[y0:y1 + 1, x0:x1 + 1]
    take = inside & (depth < sub)
    if not take.any():
        return
    sub[take] = depth[take]
    patch = img[y0:y1 + 1, x0:x1 + 1]
    patch[take] = rgb


def save(img, path):
    from PIL import Image
    Image.fromarray(img).save(path)
    return path
