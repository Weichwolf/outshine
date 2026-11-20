"""Compare spatial graph pages across two cached OSM street networks."""

import argparse
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from hashlib import blake2b
import math
from pathlib import Path
import sqlite3
import struct
import subprocess


STREET_HEADER = struct.Struct("<QQQdQQdQ")
NETWORK_HEADER = struct.Struct("<QddQQQQB")
WAY = struct.Struct("<QQ9diiBBBQ")
WAY_BOUNDS = struct.Struct("<QQ4d")
NODE = struct.Struct("<ddQQ4di")
EDGE = struct.Struct("<QdIi")
COORDINATE = struct.Struct("<dd")
UINT32 = struct.Struct("<I")
UINT64 = struct.Struct("<Q")


class Reader:
    def __init__(self, payload):
        self.payload = memoryview(payload)
        self.offset = 0

    def take(self, count):
        start = self.offset
        self.offset += count
        if self.offset > len(self.payload):
            raise ValueError("truncated street network")
        return self.payload[start:self.offset]

    def read(self, layout):
        return layout.unpack(self.take(layout.size))

    def array(self, stride, count_layout=UINT32):
        count, = self.read(count_layout)
        return self.take(count * stride)


@dataclass
class Page:
    nodes: dict = field(default_factory=dict)
    outgoing: Counter = field(default_factory=Counter)


def asset_bytes(database, prefix):
    if not 12 <= len(prefix) <= 64 or any(c not in "0123456789abcdef" for c in prefix):
        raise ValueError("use a lowercase hexadecimal key or prefix of at least twelve digits")
    addresses = database.execute(
        """SELECT p.codec,a.offset,a.bytes,a.package FROM assets a
           JOIN packages p ON p.key=a.package
           WHERE a.kind='street-network' AND a.key LIKE ? LIMIT 2""",
        (prefix + "%",),
    ).fetchall()
    if len(addresses) != 1:
        raise ValueError(f"{prefix} must identify one cached street network")
    codec, offset, size, package = addresses[0]
    payload, = database.execute(
        "SELECT bytes FROM package_bodies WHERE key=?", (package,)
    ).fetchone()
    if codec == 1:
        payload = subprocess.run(
            ["zstd", "-q", "-dc"], input=payload, capture_output=True, check=True
        ).stdout
    elif codec != 0:
        raise ValueError(f"unknown asset codec {codec}")
    return memoryview(payload)[offset:offset + size]


def read_network(payload, way_identities):
    reader = Reader(payload)
    if reader.read(STREET_HEADER)[0] != 0x000131534E:
        raise ValueError("unsupported street network wrapper")
    header = reader.read(NETWORK_HEADER)
    if header[0] != 0x00023154454E or not header[-1]:
        raise ValueError("expected a woven network with the supported binary format")
    coordinates = reader.array(8)
    for stride in (4, 4, 8, 8, 8):
        reader.array(stride)
    ways = reader.array(WAY.size, UINT64)
    way_keys = []
    for index, record in enumerate(WAY.iter_unpack(ways)):
        first, count = record[:2]
        geometry = coordinates[first * COORDINATE.size:(first + count) * COORDINATE.size]
        if count < 2 or len(geometry) != count * COORDINATE.size:
            raise ValueError("invalid way coordinates")
        start = index * WAY.size + WAY_BOUNDS.size
        end = (index + 1) * WAY.size - UINT64.size
        properties = ways[start:end]
        exact = bytes(geometry) + bytes(properties)
        key = blake2b(exact, digest_size=16).digest()
        if way_identities.setdefault(key, exact) != exact:
            raise ValueError("way identity collision")
        way_keys.append(key)
    rows = reader.array(NODE.size, UINT64)
    edges = reader.array(EDGE.size, UINT64)
    positions = [bytes(rows[at:at + COORDINATE.size]) for at in range(0, len(rows), NODE.size)]
    nodes = {}
    outgoing = Counter()
    for source, (_, _, first, degree, *properties) in zip(positions, NODE.iter_unpack(rows)):
        if source in nodes:
            raise ValueError("node coordinates do not uniquely identify this graph")
        nodes[source] = tuple(properties)
        begin, end = first * EDGE.size, (first + degree) * EDGE.size
        if end > len(edges):
            raise ValueError("invalid network edge range")
        for target, length, way, step in EDGE.iter_unpack(edges[begin:end]):
            outgoing[source, positions[target], way_keys[way], step, length] += 1
    if sum(outgoing.values()) != len(edges) // EDGE.size:
        raise ValueError("invalid network edge ranges")
    return nodes, outgoing


def tile(position, zoom):
    latitude, longitude = COORDINATE.unpack(position)
    width = 1 << zoom
    column = math.floor((longitude + 180) / 360 * width) % width
    row = math.floor((1 - math.asinh(math.tan(math.radians(latitude))) / math.pi) * width / 2)
    return column, min(width - 1, max(0, row))


def partition(network, zoom):
    nodes, outgoing = network
    pages = defaultdict(Page)
    owner = {position: tile(position, zoom) for position in nodes}
    for position, properties in nodes.items():
        pages[owner[position]].nodes[position] = properties
    portals = 0
    for edge, count in outgoing.items():
        source, target = edge[:2]
        pages[owner[source]].outgoing[edge] = count
        if owner[source] != owner[target]:
            portals += count
    return dict(pages), portals


def check_pages(pages, expected, zoom):
    nodes, outgoing = expected
    owner = {position: tile(position, zoom) for position in nodes}
    node_count = edge_count = waiting = 0
    for address, page in pages.items():
        for position, properties in page.nodes.items():
            assert owner[position] == address and nodes[position] == properties
            node_count += 1
        for edge, count in page.outgoing.items():
            source, target = edge[:2]
            assert source in page.nodes and outgoing[edge] == count
            edge_count += 1
            destination = pages.get(owner[target])
            if destination is None or target not in destination.nodes:
                waiting += count
    return node_count, edge_count, waiting


def check_portal():
    first = COORDINATE.pack(35.25, 139.0)
    second = COORDINATE.pack(35.25, 139.1)
    edge = first, second, b"way", 1, 9093.0
    network = {first: (2,), second: (2,)}, Counter({edge: 1})
    pages, portals = partition(network, 17)
    partial = {tile(first, 17): pages[tile(first, 17)]}
    assert check_pages(partial, network, 17) == (1, 1, portals)
    assert portals == 1 and check_pages(pages, network, 17) == (2, 1, 0)


def compare(before, after, zoom):
    old, _ = partition(before, zoom)
    new, portals = partition(after, zoom)
    assert check_pages(old, before, zoom) == (len(before[0]), len(before[1]), 0)
    retained = {key: page for key, page in old.items() if new.get(key) == page}
    updated = {key: page for key, page in new.items() if key not in retained}
    assert check_pages({**retained, **updated}, after, zoom) == (len(after[0]), len(after[1]), 0)
    retained_edges = sum(sum(page.outgoing.values()) for page in retained.values())
    print(
        f"z{zoom}: {len(retained)}/{len(new)} pages and "
        f"{retained_edges}/{sum(after[1].values())} edges reused. "
        f"{portals} cross-page references. {len(old.keys() - new.keys())} evictions."
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("database", type=Path)
    parser.add_argument("before")
    parser.add_argument("after")
    arguments = parser.parse_args()
    check_portal()
    identities = {}
    uri = arguments.database.resolve().as_uri() + "?mode=ro"
    with sqlite3.connect(uri, uri=True) as database:
        database.execute("BEGIN")
        before = read_network(asset_bytes(database, arguments.before), identities)
        after = read_network(asset_bytes(database, arguments.after), identities)
    for zoom in (14, 15, 16, 17):
        compare(before, after, zoom)


if __name__ == "__main__":
    main()
