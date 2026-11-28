"""Bake EEVEE irradiance volumes for covered walks and outdoor sky."""

from math import atan2
from time import perf_counter

import numpy as np
import shapely
from viaducts import SLAB_DEPTH

PORTAL_MARGIN_M = 3.5
PROBE_RANGE_M = 48.0
PROBE_SPACING_M = 0.85


def near_lower_deck(decks, eye, reach):
    point = shapely.Point(eye[:2])
    return any(
        deck.lower_routes.dwithin(point, reach)
        and eye[2] < deck.height(np.asarray(eye)[None, :2])[0] - SLAB_DEPTH
        for deck in decks
    )


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


def place_exterior_sky(scene, *, woodland):
    import bpy
    from mathutils import Vector

    camera = scene.camera.matrix_world
    if woodland:
        probe = bpy.data.lightprobes.new("Woodland sky irradiance", "VOLUME")
        probe.resolution_x = probe.resolution_y = 33
        probe.resolution_z = 14
        probe.capture_distance = 70.0
        probe.normal_bias = 0.07
        probe.view_bias = 0.09
        forward = -camera.col[2].to_3d()
        offset = Vector((40 * forward.x, 40 * forward.y, 16))
        radii = (50, 50, 20)
    else:
        probe = bpy.data.lightprobes.new("Street sky visibility", "VOLUME")
        probe.resolution_x = probe.resolution_y = 19
        probe.resolution_z = 9
        probe.capture_distance = 64.0
        probe.normal_bias = 0.12
        probe.view_bias = 0.10
        offset = Vector((0, 0, 8))
        radii = (36, 36, 12)
    probe.surfel_density = 2
    probe.bake_samples = 128
    probe.capture_world = True
    probe.capture_indirect = woodland
    probe.capture_emission = False
    volume = bpy.data.objects.new(probe.name, probe)
    volume.location = camera.translation + offset
    volume.scale = radii
    scene.collection.objects.link(volume)


def bake_light_probes():
    import bpy

    start = perf_counter()
    bpy.context.view_layer.update()
    if "FINISHED" not in bpy.ops.object.lightprobe_cache_bake(subset="ALL"):
        raise RuntimeError("Reference irradiance bake did not finish")
    return perf_counter() - start


def bake_reference_irradiance(
    scene, passages, eye_xy, sun, *, street_sky=False, woodland=False
):
    import bpy

    regions = list(passage_probe_regions(passages, eye_xy))
    if not (regions or street_sky or woodland):
        return 0, 0.0
    start = perf_counter()
    if street_sky or woodland:
        place_exterior_sky(scene, woodland=woodland)
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
    sun_energy = sun.energy
    try:
        if woodland:
            sun.energy = 0
        bake_light_probes()
    finally:
        if woodland:
            sun.energy = sun_energy
    return len(regions), perf_counter() - start
