#!/usr/bin/env python3
"""Compare surface-only and closed ribbons at identical captured road stations.

Input: road_profile_envelopes CSV. Sampling and upper cross-section remain identical.
Counts cover captured grounded chains; fitted extra stations and junctions are excluded.
"""
import argparse
import csv
import json
from pathlib import Path


def compare(rows):
    chains = {}
    for row in rows:
        lane, station, east, north, height, node, grade, bridge, layer = row
        if not int(bridge):
            chains.setdefault(int(lane), set()).add(int(station))
    chains = [len(stations) for stations in chains.values() if len(stations) >= 2]
    closed_vertices = sum(12 * stations + 16 for stations in chains)
    surface_vertices = sum(4 * stations for stations in chains)
    closed_triangles = sum(16 * (stations - 1) + 12 for stations in chains)
    surface_triangles = sum(6 * (stations - 1) for stations in chains)
    vertex_bytes = (3 + 3 + 4) * 4
    return dict(grounded_chains=len(chains), captured_stations=sum(chains),
                closed_vertices=closed_vertices, surface_vertices=surface_vertices,
                closed_triangles=closed_triangles, surface_triangles=surface_triangles,
                closed_bytes=closed_vertices * vertex_bytes + closed_triangles * 12,
                surface_bytes=surface_vertices * vertex_bytes + surface_triangles * 12,
                upper_vertex_displacement_m=0,
                scope='identical upper sampling; closed lower/wall/end faces omitted; '
                      'no fitted-extra-station, junction, GPU or visibility proof')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profiles', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    with args.profiles.open() as source:
        result = compare(csv.reader(source))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
