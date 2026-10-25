"""Read atomic source footprints and height ranges before any building mesh."""

from dataclasses import dataclass

import numpy as np
import shapely
from column_roof_patches import bottom_m, project_patch, roof_patches, tangent_basis
from shapely.affinity import translate


@dataclass(frozen=True)
class MassSource:
    source: int
    footprint: object
    bottom: tuple
    roof: tuple
    raised: bool = False


def describe(identity, surface, basis, anchor):
    if surface is None or not surface["shapes"]:
        raise ValueError("complete original source forms required")
    footprints, bottoms, tops = [], [], []
    shift = 0.0
    for shape in surface["shapes"]:
        polygon = shapely.Polygon(shape["ring"], shape["holes"])
        if not polygon.is_valid or polygon.area <= 0:
            raise ValueError("valid original footprint required")
        bottom = bottom_m(surface, shape)
        footprint, plane, _, _ = project_patch(
            surface, polygon, np.array([0.0, 0.0, bottom]), basis, bottom
        )
        xy = shapely.get_coordinates(footprint)
        bottoms.extend(xy @ plane[:2] + plane[2] - anchor[2])
        footprints.append(footprint)
        for domain, plane in roof_patches(shape):
            roof, height, _, lateral = project_patch(
                surface, domain, plane, basis, bottom
            )
            xy = shapely.get_coordinates(roof)
            tops.extend(xy @ height[:2] + height[2] - anchor[2])
            shift = max(shift, float(lateral))
    footprint = translate(shapely.union_all(footprints), -anchor[0], -anchor[1])
    heights = np.array([min(bottoms), max(bottoms), min(tops), max(tops)])
    if (
        not footprint.is_valid
        or not np.isfinite(heights).all()
        or heights[3] <= heights[0]
    ):
        raise ValueError("valid finite source mass ranges required")
    return MassSource(
        identity,
        footprint,
        tuple(heights[:2]),
        tuple(heights[2:]),
        surface["minimum_height_m"] != 0,
    ), shift


def read_masses(parent, cells):
    basis = tangent_basis(parent.plans)
    records = [
        (int(i), s)
        for _, ids, forms in parent.iter_forms(cells)
        for i, s in zip(ids, forms, strict=True)
    ]
    records.sort(key=lambda row: row[0])
    frames = [s["origin"] for _, s in records if s is not None]
    if not frames:
        raise ValueError("at least one original source frame required")
    anchor = np.mean(frames, axis=0) @ basis.T
    sources, needs, shift = [], [], 0.0
    for identity, surface in records:
        try:
            source, lateral = describe(identity, surface, basis, anchor)
        except (
            ValueError,
            np.linalg.LinAlgError,
            shapely.errors.GEOSException,
        ) as error:
            needs.append({"source": identity, "reason": str(error)})
            continue
        sources.append(source)
        shift = max(shift, lateral)
    return (
        sources,
        {
            "basis": basis,
            "anchor": anchor,
            "selected_sources": np.array([i for i, _ in records], np.uint32),
        },
        {
            "needs": needs,
            "maximum_common_up_wall_shift_m": shift,
            "form_reads": parent.form_reads,
            "form_bytes": parent.form_bytes,
            "source_archive_sha256": parent.archive_sha256,
        },
    )
