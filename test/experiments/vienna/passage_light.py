"""Place EEVEE irradiance volumes along generated covered walks."""

from math import atan2
from time import perf_counter

import numpy as np
import shapely

PORTAL_MARGIN_M = 3.5
PROBE_RANGE_M = 48.0
PROBE_SPACING_M = 0.85


def passage_probe_regions(passages, eye_xy):
    eye = shapely.Point(eye_xy)
    nearby = [
        passage
        for passage in passages
        if passage.corridor.distance(eye) <= PROBE_RANGE_M
    ]
    domains = shapely.union_all(
        [
            passage.corridor.buffer(PORTAL_MARGIN_M, join_style="mitre")
            for passage in nearby
        ]
    )
    for domain in shapely.get_parts(domains):
        members = [passage for passage in nearby if domain.intersects(passage.corridor)]
        footprint = domain.minimum_rotated_rectangle
        corners = np.array(footprint.exterior.coords[:4])
        edges = corners[1:3] - corners[:2]
        width, length = np.linalg.norm(edges, axis=1)
        x, y = (corners[0] + corners[2]) * 0.5
        bottom = min(passage.floor_m for passage in members) - 0.8
        top = max(passage.crown_m for passage in members) + 2.3
        radii = (width * 0.5, length * 0.5, (top - bottom) * 0.5)
        center = (x, y, (top + bottom) * 0.5)
        angle = atan2(edges[0, 1], edges[0, 0])
        yield center, radii, angle


def bake_reference_irradiance(scene, passages, eye_xy, *, street_sky=False):
    import bpy

    regions = list(passage_probe_regions(passages, eye_xy))
    if not regions and not street_sky:
        return 0, 0.0
    start = perf_counter()
    if street_sky:
        from diffuse_bounce import place_street_sky

        place_street_sky(scene)
    for center, radii, angle in regions:
        probe = bpy.data.lightprobes.new("Covered walk irradiance", "VOLUME")
        samples = np.clip(np.ceil(2 * np.array(radii) / PROBE_SPACING_M) + 1, 3, 40)
        probe.resolution_x, probe.resolution_y, probe.resolution_z = map(int, samples)
        probe.surfel_density = 4
        probe.bake_samples = 128
        probe.capture_world = True
        probe.capture_indirect = not street_sky
        probe.capture_emission = False
        probe.capture_distance = 12.0
        probe.normal_bias = 0.10
        volume = bpy.data.objects.new(probe.name, probe)
        volume.location = center
        volume.rotation_euler.z = angle
        volume.scale = radii
        scene.collection.objects.link(volume)
    bpy.context.view_layer.update()
    if "FINISHED" not in bpy.ops.object.lightprobe_cache_bake(subset="ALL"):
        raise RuntimeError("Reference irradiance bake did not finish")
    return len(regions), perf_counter() - start
