"""Prepare complete geodetic camera turns for the external Places driver."""

import copy
import math
import xml.etree.ElementTree as ET


def vector(element, default=(0.0, 0.0, 0.0)):
    return (
        tuple(
            float(element.get(key, str(value)))
            for key, value in zip(("x", "y", "z"), default)
        )
        if element is not None
        else default
    )


def camera_centre(world, view):
    latitude, longitude = (float(world.get(key, "0")) for key in ("lat", "lon"))
    lat, lon = map(math.radians, (latitude, longitude))
    at = view.find("at")
    camera_lat, camera_lon = (float(at.get(key)) for key in ("lat", "lon"))
    height = float(at.get("heightM", "0"))

    def ecef(lat_deg, lon_deg, height):
        phi, lam = map(math.radians, (lat_deg, lon_deg))
        sine, cosine = math.sin(phi), math.cos(phi)
        flattening = 1 / 298.257223563
        eccentricity = flattening * (2 - flattening)
        normal = 6378137 / math.sqrt(1 - eccentricity * sine * sine)
        return (
            (normal + height) * cosine * math.cos(lam),
            (normal + height) * cosine * math.sin(lam),
            (normal * (1 - eccentricity) + height) * sine,
        )

    displacement = tuple(
        a - b
        for a, b in zip(
            ecef(camera_lat, camera_lon, height), ecef(latitude, longitude, 0)
        )
    )
    east = (-math.sin(lon), math.cos(lon), 0)
    up = (math.cos(lat) * math.cos(lon), math.cos(lat) * math.sin(lon), math.sin(lat))
    south = (
        math.sin(lat) * math.cos(lon),
        math.sin(lat) * math.sin(lon),
        -math.cos(lat),
    )
    return tuple(
        sum(a * b for a, b in zip(displacement, axis)) + float(view.get(key, "0"))
        for axis, key in zip((east, up, south), ("offsetX", "offsetY", "offsetZ"))
    )


def rotate(value, axis, angle):
    cosine, sine = math.cos(angle), math.sin(angle)
    dot = sum(a * b for a, b in zip(axis, value))
    cross = (
        axis[1] * value[2] - axis[2] * value[1],
        axis[2] * value[0] - axis[0] * value[2],
        axis[0] * value[1] - axis[1] * value[0],
    )
    return tuple(
        v * cosine + c * sine + u * dot * (1 - cosine)
        for v, c, u in zip(value, cross, axis)
    )


def target_rotator(initial, world, axis):
    centre = camera_centre(world, initial)
    target = vector(initial.find("lookAt"))
    delta = tuple(t - c for t, c in zip(target, centre))
    initial_up = vector(initial.find("up"), (0.0, 1.0, 0.0))

    def apply(view, degrees):
        turned = rotate(delta, axis, math.radians(degrees))
        up = rotate(initial_up, axis, math.radians(degrees))
        for tag, values in (
            ("lookAt", tuple(c + t for c, t in zip(centre, turned))),
            ("up", up),
        ):
            element = view.find(tag)
            if element is None:
                element = ET.SubElement(view, tag)
            for key, value in zip(("x", "y", "z"), values):
                element.set(key, format(value, ".17g"))

    return apply


def turn_axis(world, camera):
    origin = tuple(float(world.get(key, "0")) for key in ("lat", "lon"))
    at = tuple(float(camera.get(key)) for key in ("lat", "lon"))
    if origin == at:
        return (0.0, -1.0, 0.0)
    latitude, longitude = map(math.radians, at)
    origin_latitude, origin_longitude = map(math.radians, origin)
    delta = longitude - origin_longitude
    east = math.cos(latitude) * math.sin(delta)
    up = math.cos(latitude) * math.cos(origin_latitude) * math.cos(delta) + math.sin(
        latitude
    ) * math.sin(origin_latitude)
    north = -math.cos(latitude) * math.sin(origin_latitude) * math.cos(
        delta
    ) + math.sin(latitude) * math.cos(origin_latitude)
    return (-east, -up, north)


def turn_scenario(source, rate):
    root = ET.parse(source).getroot()
    views = root.find("views")
    geographic = (
        []
        if views is None
        else [
            view
            for view in views
            if view.find("at") is not None
            and "lat" in view.find("at").attrib
            and "lon" in view.find("at").attrib
        ]
    )
    if len(geographic) != 1:
        raise ValueError(f"{source}: exactly one geodetic view is required")
    render = root.find("render")
    if render is None or root.find("world") is None:
        raise ValueError(f"{source}: declared world and render profile are required")
    clock = root.find("clock")
    if clock is None or not clock.get("start") or clock.get("live") != "no":
        raise ValueError(f"{source}: capture requires a fixed declared clock")
    initial = copy.deepcopy(geographic[0])
    bearing = float(initial.find("at").get("bearingDeg", "0"))
    if not math.isfinite(bearing):
        raise ValueError(f"{source}: bearing must be finite")
    identifier = initial.get("id")
    if not identifier:
        raise ValueError(f"{source}: capture view requires an id")
    axis = turn_axis(root.find("world"), initial.find("at"))
    target_mode = initial.get("looksAt")
    looks_at = target_mode in ("yes", "true", "1") or (
        target_mode is None and initial.find("lookAt") is not None
    )
    target = target_rotator(initial, root.find("world"), axis) if looks_at else None
    render.set("fps", str(rate))
    views.clear()
    views.append(initial)
    for frame in range(1, rate):
        view = copy.deepcopy(initial)
        view.set("id", f"{identifier}/turn/{frame}")
        degrees = 360 * frame / rate
        if target is not None:
            target(view, degrees)
        else:
            angle = math.remainder(bearing + degrees, 360)
            view.find("at").set("bearingDeg", format(angle, ".17g"))
        views.append(view)
    return (
        ET.tostring(root, encoding="unicode"),
        identifier,
        ",".join(format(360 * value, ".17g") for value in axis),
    )
