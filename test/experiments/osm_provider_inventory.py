"""Sample every MVT layer at varied sites without changing runtime sources or assets."""
import argparse
import collections
import concurrent.futures
import hashlib
import json
import math
import pathlib
import struct
import subprocess
import time


SITES = [
    ("Vienna", 48.21, 16.37), ("Rosenheim", 47.860299, 12.131823),
    ("Flensburg", 54.79, 9.43), ("Prague", 50.08, 14.42),
    ("Venice", 45.44, 12.33), ("Paris", 48.85, 2.35),
    ("Berlin", 52.52, 13.40), ("CentralPark", 40.78, -73.965),
    ("Tokyo", 35.66, 139.70), ("HongKong", 22.28, 114.16),
    ("Dubai", 25.20, 55.28), ("Singapore", 1.28, 103.85),
    ("Shanghai", 31.235, 121.50), ("RotterdamHarbour", 51.95, 4.14),
    ("HamburgHarbour", 53.53, 9.96), ("NeurathPower", 51.034, 6.615),
    ("DraxPower", 53.735, -0.992), ("MiddelgrundenWind", 55.690, 12.671),
    ("PalmSpringsWind", 33.933, -116.576), ("BhadlaSolar", 27.53, 71.922),
    ("NoorSolar", 30.929, -6.868), ("FrankfurtAirport", 50.04, 8.57),
    ("SchipholAirport", 52.31, 4.77), ("MunichOlympic", 48.175, 11.55),
    ("Feldkirch", 47.238, 9.599), ("AlpineReservoir", 47.08, 12.69),
    ("Istanbul", 41.008, 28.978), ("Marrakech", 31.63, -7.99),
    ("CapeTown", -33.92, 18.42), ("Rio", -22.90, -43.18),
    ("Sydney", -33.86, 151.21), ("Auckland", -36.85, 174.76),
]
SEMANTIC_KEYS = {
    "class", "subclass", "kind", "building", "building:part", "man_made",
    "power", "generator:source", "plant:source", "material", "building:material",
    "roof:shape", "roof:material", "start_date", "height", "render_height",
    "render_min_height", "hide_3d", "colour", "surface", "service", "brunnel",
}


def varint(data, at):
    value = 0
    for shift in range(0, 70, 7):
        byte = data[at]
        at += 1
        value |= (byte & 127) << shift
        if byte < 128:
            return value, at
    raise ValueError("invalid protobuf integer")


def fields(data):
    at = 0
    while at < len(data):
        key, at = varint(data, at)
        number, wire = key >> 3, key & 7
        if wire == 0:
            value, at = varint(data, at)
        elif wire in (1, 5):
            size = 8 if wire == 1 else 4
            value = data[at:at + size]
            at += size
        elif wire == 2:
            size, at = varint(data, at)
            value = data[at:at + size]
            at += size
            if len(value) != size:
                raise ValueError("truncated protobuf field")
        else:
            raise ValueError("unsupported protobuf wire")
        yield number, wire, value


def packed(data):
    at = 0
    while at < len(data):
        value, at = varint(data, at)
        yield value


def value_of(data):
    for number, _, value in fields(data):
        if number == 1:
            return value.decode("utf-8")
        if number in (2, 3):
            return struct.unpack("<f" if number == 2 else "<d", value)[0]
        if number in (4, 5):
            return value if number == 5 or value < 2**63 else value - 2**64
        if number == 6:
            return (value >> 1) ^ -(value & 1)
        if number == 7:
            return bool(value)
    raise ValueError("empty MVT value")


def layers_of(data):
    for number, _, payload in fields(data):
        if number != 3:
            continue
        layer = list(fields(payload))
        name = next(v.decode() for n, _, v in layer if n == 1)
        keys = [v.decode() for n, _, v in layer if n == 3]
        values = [value_of(v) for n, _, v in layer if n == 4]
        features = []
        for n, _, feature in layer:
            if n != 2:
                continue
            items = list(fields(feature))
            tags = [x for tag, _, v in items if tag == 2 for x in packed(v)]
            if len(tags) % 2:
                raise ValueError("odd MVT tag list")
            props = {keys[tags[i]]: values[tags[i + 1]] for i in range(0, len(tags), 2)}
            features.append((next((v for tag, _, v in items if tag == 3), 0), props))
        yield name, keys, features


def tile_at(latitude, longitude, zoom):
    side = 2**zoom
    return zoom, int((longitude + 180) / 360 * side), int(
        (1 - math.asinh(math.tan(math.radians(latitude))) / math.pi) / 2 * side)


def fetch(url, path):
    if path.is_file():
        return path.read_bytes(), "retained", 0.0
    begin = time.monotonic()
    result = subprocess.run([
        "curl", "--fail", "--silent", "--show-error", "--location",
        "--max-time", "30", "--max-filesize", "4194304", "--retry", "2",
        "--retry-max-time", "45", "--output", str(path) + ".part", url,
    ], capture_output=True, timeout=100)
    if result.returncode:
        pathlib.Path(str(path) + ".part").unlink(missing_ok=True)
        raise RuntimeError(result.stderr.decode().strip())
    pathlib.Path(str(path) + ".part").replace(path)
    return path.read_bytes(), "network", time.monotonic() - begin


def inventory(args):
    args.downloads.mkdir(parents=True, exist_ok=True)
    tasks = {}
    for site, latitude, longitude in SITES:
        for zoom in (10, 12, 14):
            z, x, y = tile_at(latitude, longitude, zoom)
            offsets = [(0, 0)] if zoom != 14 else [
                (dx, dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)]
            for dx, dy in offsets:
                tasks.setdefault((z, x + dx, y + dy), []).append(site)

    def sample(item):
        tile, sites = item
        z, x, y = tile
        url = args.endpoint.format(z=z, x=x, y=y)
        path = args.downloads / (hashlib.sha256(url.encode()).hexdigest() + ".pbf")
        try:
            data, origin, seconds = fetch(url, path)
            return dict(tile=tile, sites=sites, url=url, bytes=len(data),
                        sha256=hashlib.sha256(data).hexdigest(), origin=origin,
                        seconds=seconds, layers=list(layers_of(data)))
        except (OSError, ValueError, RuntimeError, IndexError, StopIteration,
                subprocess.TimeoutExpired) as error:
            return dict(tile=tile, sites=sites, url=url, error=str(error))

    totals = {}
    receipts = []
    errors = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for row in pool.map(sample, tasks.items()):
            if "error" in row:
                errors.append(row)
                continue
            for name, keys, features in row.pop("layers"):
                layer = totals.setdefault(name, dict(tiles=0, features=0, keys=collections.Counter(),
                    values={}, geometry=collections.Counter(), sites=collections.Counter()))
                layer["tiles"] += 1
                layer["features"] += len(features)
                layer["keys"].update(keys)
                for site in row["sites"]:
                    layer["sites"][site] += len(features)
                for geometry, props in features:
                    layer["geometry"][geometry] += 1
                    for key, value in props.items():
                        if key in SEMANTIC_KEYS and not isinstance(value, (int, float, bool)):
                            layer["values"].setdefault(key, collections.Counter()).update([value])
            receipts.append(row)
    report = dict(endpoint=args.endpoint, sites=SITES, requested_tiles=len(tasks),
        successful_tiles=len(receipts), errors=errors, payload_bytes=sum(r["bytes"] for r in receipts),
        scope="Targeted samples, not worldwide completeness; tile-buffer duplicates included.",
        layers=totals, receipts=receipts)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(json.dumps({k: report[k] for k in (
        "requested_tiles", "successful_tiles", "payload_bytes", "scope")}))
    print("layers:", ", ".join(sorted(totals)))
    print("errors:", len(errors))
    return 1 if errors else 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True)
    parser.add_argument("--downloads", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--jobs", default=4, type=int, choices=range(1, 9))
    raise SystemExit(inventory(parser.parse_args()))
