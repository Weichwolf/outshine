"""Two-sided source mattes; coverage precedes shading and normals remain individual."""

import numpy as np
from building_mass_lighting import shade
from building_mass_material_image import hit_materials, samples
from current_visibility import project, raster
from triangle_image_reference import average_samples, image_rays


def quad(data, plane):
    low = data["plane_low"][plane]
    high = low + data["plane_shape"][plane] * data["spacing"]
    uv = np.array([low, [high[0], low[1]], high, [low[0], high[1]]])
    return data["plane_origin"][plane] + uv @ data["plane_axes"][plane]


def cell_profiles(data, cells, outgoing):
    first = data["cell_profile_offsets"][cells]
    last = data["cell_profile_offsets"][cells + 1]
    profiles = first[:, None] + np.arange(4)
    present = profiles < last[:, None]
    safe = np.where(present, profiles, 0)
    weight = data["profile_weight"][safe].astype(float) * present
    total = weight.sum(axis=1)
    if np.any(total <= 0):
        raise ValueError("covered cells must retain positive source response profiles")
    facing = np.einsum("nki,ni->nk", data["profile_normal"][safe], outgoing) > 0
    visible = weight / total[:, None] * facing
    mass = visible.sum(axis=1)
    all_facing = np.all(facing | ~present | (weight == 0), axis=1)
    fraction = np.divide(
        visible, mass[:, None], out=np.zeros_like(visible), where=mass[:, None] > 0
    )
    mass[all_facing] = 1
    return safe, fraction, data["alpha"][cells] * mass


def gather_mattes(data, camera, box, directions, outgoing, rate):
    width, height = map(int, box[1] - box[0])
    rays, depths, alpha, profiles, fractions, ties = [], [], [], [], [], []
    traffic = {}
    cells_read, all_records = 0, 0
    for plane in range(len(data["plane_origin"])):
        vertices = quad(data, plane)
        normal = np.cross(*data["plane_axes"][plane])
        indices = np.array([[0, 1, 2], [0, 2, 3]])
        if normal @ (camera[0] - data["plane_origin"][plane]) < 0:
            indices = indices[:, [0, 2, 1]]
        projected, forward = project(vertices, camera)
        if np.any(forward <= 0.1):
            raise ValueError("whole matte must remain beyond near plane")
        projected[:, :2] = (projected[:, :2] - box[0]) * rate
        work = {}
        depth, owner = raster(
            projected,
            forward,
            indices,
            width * rate,
            height * rate,
            with_faces=True,
            counters=work,
        )
        for k, v in work.items():
            traffic[k] = traffic.get(k, 0) + v
        hit = np.flatnonzero(owner.ravel() >= 0)
        z = depth.ravel()[hit]
        points = camera[0] + (0.1 / z)[:, None] * directions[hit]
        uv = (points - data["plane_origin"][plane]) @ data["plane_axes"][plane].T
        at = np.floor((uv - data["plane_low"][plane]) / data["spacing"]).astype(int)
        at = np.maximum(np.minimum(at, data["plane_shape"][plane].astype(int) - 1), 0)
        cells = (
            data["plane_cell_offsets"][plane]
            + at[:, 1] * data["plane_shape"][plane, 0]
            + at[:, 0]
        )
        cells_read += len(cells)
        covered = data["alpha"][cells] > 0
        hit, z, cells = hit[covered], z[covered], cells[covered]
        if not len(hit):
            continue
        local_profiles, local_fraction, local_alpha = cell_profiles(
            data, cells, outgoing[hit]
        )
        live = local_alpha > 0
        all_records += int(live.sum())
        if all_records > 1048576:
            raise ValueError("bounded matte fragment budget exhausted")
        rays.append(hit[live])
        depths.append(0.1 / z[live])
        alpha.append(np.minimum(local_alpha[live], 1))
        profiles.append(local_profiles[live])
        fractions.append(local_fraction[live])
        ties.append(np.full(int(live.sum()), plane, int))
    empty = (
        np.empty(0, int),
        np.empty(0),
        np.empty(0),
        np.empty((0, 4), int),
        np.empty((0, 4)),
        np.empty(0, int),
    )
    result = [
        np.concatenate(v) if v else empty[i]
        for i, v in enumerate((rays, depths, alpha, profiles, fractions, ties))
    ]
    return result, {
        "matte_layer_records": all_records,
        "coverage_cell_reads": cells_read,
        "raster_work": traffic,
        "known_matte_record_array_bytes": sum(v.nbytes for v in result),
    }


def layer_weights(rays, depth, alpha, ties, count):
    if (
        any(len(a) != len(rays) for a in (depth, alpha, ties))
        or np.any(rays < 0)
        or np.any(rays >= count)
        or not all(np.isfinite(v).all() for v in (depth, alpha))
        or np.any((alpha < 0) | (alpha > 1))
    ):
        raise ValueError("finite bounded layer transfers required")
    order = np.lexsort((ties, depth, rays))
    sorted_rays = rays[order]
    unique, first, counts = np.unique(
        sorted_rays, return_index=True, return_counts=True
    )
    transmission = np.ones(count)
    weight = np.zeros(len(rays))
    for rank in range(int(counts.max(initial=0))):
        active = counts > rank
        at = order[first[active] + rank]
        target = unique[active]
        weight[at] = transmission[target] * alpha[at]
        transmission[target] *= 1 - alpha[at]
    return weight, transmission


def render(
    data,
    original,
    fields,
    camera,
    box,
    tables,
    rate=16,
    *,
    light=(0.45, -0.5, 0.7),
    intensity=3,
    ambient=0.1,
):
    width, height = map(int, box[1] - box[0])
    count = width * height * rate**2
    if count > 1048576:
        raise ValueError("bounded diagnostic pixel quadrature required")
    _, directions = image_rays(camera, box, width, height, rate)
    outgoing = -directions / np.linalg.norm(directions, axis=1)[:, None]
    (rays, depths, alpha, profiles, fractions, ties), work = gather_mattes(
        data, camera, box, directions, outgoing, rate
    )
    cloud_count = len(rays)
    original_faces = np.flatnonzero(
        np.isin(original["binding"][:, 1], data["needs_sources"])
    )
    rest = {
        "positions": original["positions"],
        "indices": original["indices"][original_faces],
    }
    native = samples(rest, camera, box, rate)
    native_rays = np.flatnonzero(native["hit"])
    native_depth = (native["points"] - camera[0]) @ camera[1][2]
    all_rays = np.r_[rays, native_rays]
    weight, transmission = layer_weights(
        all_rays,
        np.r_[depths, native_depth],
        np.r_[alpha, np.ones(len(native_rays))],
        np.r_[ties, np.full(len(native_rays), len(data["plane_origin"]), int)],
        count,
    )
    colour = np.zeros((count, 3))
    lobes = 0
    shaded = 0
    for rank in range(4):
        selected = np.flatnonzero((weight[:cloud_count] > 0) & (fractions[:, rank] > 0))
        profile = profiles[selected, rank]
        material = data["profile_material"][profile]
        value, cost = shade(
            data["material_albedo"][material],
            data["material_roughness_weights"][material],
            data["roughness_palette"],
            data["profile_normal"][profile],
            outgoing[rays[selected]],
            light,
            tables,
            intensity=intensity,
            ambient=ambient,
        )
        coefficient = weight[selected] * fractions[selected, rank]
        np.add.at(colour, rays[selected], coefficient[:, None] * value)
        lobes += cost["ggx_lobe_evaluations"]
        shaded += len(selected)
    selected = np.flatnonzero(weight[cloud_count:] > 0)
    face = original_faces[native["faces"][selected]]
    rgb, roughness = hit_materials(original, fields, native["points"][selected], face)
    value, cost = shade(
        rgb,
        roughness,
        data["roughness_palette"],
        fields["normal"][face],
        outgoing[native_rays[selected]],
        light,
        tables,
        intensity=intensity,
        ambient=ambient,
    )
    np.add.at(
        colour, native_rays[selected], weight[cloud_count + selected, None] * value
    )
    work.update(
        {
            "native_remainder_hit_samples": len(native_rays),
            "native_remainder_samples_shaded": len(selected),
            "native_remainder_ggx_lobes": cost["ggx_lobe_evaluations"],
            "ggx_lobe_evaluations": lobes + cost["ggx_lobe_evaluations"],
            "matte_profile_samples_shaded": shaded,
            "zero_transmittance_records_not_shaded": int(np.count_nonzero(weight == 0)),
            "all_pixel_samples": count,
        }
    )
    return {
        "C": average_samples(colour, width, height, rate),
        "T": average_samples(transmission, width, height, rate),
    }, work
