"""One real existing flat Feldkirch root/contact snapshot, separate from plant form."""

from dataclasses import dataclass, replace
from pathlib import Path

import numpy as np
import shapely
from habitat_stand_experiment import plans
from outshine_ref.contracts import immutable
from outshine_ref.vegetation import StandHabitat
from outshine_ref.vegetation.plan_arrays import immutable_array
from recipe_graph import external
from recipe_graph_data import canonical, digest
from recipe_plant_values import array_records

SITE_TYPE = "vegetation.habitat-site/1"
DATUM = "ellipsoid0-flat-study-no-DEM"


@dataclass(frozen=True)
class RootSite:
    habitat: StandHabitat
    provenance: object
    site_id: tuple
    origin: object
    frame: object

    def descriptor(self):
        return {
            "habitat": self.habitat.recipe(),
            "provenance": self.provenance,
            "site_id": self.site_id,
            "arrays": array_records({"origin": self.origin, "frame": self.frame}),
        }


def site_input(value):
    if not isinstance(value, RootSite):
        raise TypeError("Actual prepared root site required")
    value = replace(
        value,
        provenance=immutable(value.provenance),
        origin=immutable_array(value.origin),
        frame=immutable_array(value.frame),
    )
    data = value.descriptor()
    metadata = {
        "frame_id": value.habitat.frame.identifier,
        "axes": "east,north,up",
        "units": "m",
        "height_datum": DATUM,
        "representation": "Float64 root/rigid frame; retained prepared XY habitat",
        "content_sha256": digest(data),
    }
    return external(
        SITE_TYPE,
        value,
        data,
        value.origin.nbytes + value.frame.nbytes + len(canonical(data)),
        metadata,
    )


def source_site(path=None):
    rows = list(plans()) if path is None else list(plans(Path(path)))
    feature, stand = rows[-1]
    sites = stand.sites((6, 35, 42), max_sites=64)
    selected = int(np.flatnonzero(sites["ids"][:, 3] == 1)[0])
    origin = np.r_[sites["positions_xy_m"][selected], 0.0]
    return RootSite(
        stand.habitat,
        immutable(feature["properties"]),
        tuple(int(v) for v in sites["ids"][selected]),
        immutable_array(origin),
        immutable_array(np.eye(3)),
    )


def read_site(product):
    site = product.value
    if not isinstance(site, RootSite) or not isinstance(site.habitat, StandHabitat):
        raise TypeError("Actual source-bound prepared root site required")
    if (
        site.provenance["frame"] != site.habitat.frame.identifier
        or site.provenance["source_id"] != site.habitat.source_id
        or site.provenance["region_id"] != site.habitat.region_id
        or len(site.site_id) != 5
        or any(type(v) is not int or not 0 <= v < 2**64 for v in site.site_id)
    ):
        raise ValueError("Prepared habitat and retained source/frame binding disagree")
    expected = site_input(site).metadata
    if any(product.metadata.get(k) != v for k, v in expected.items()):
        raise ValueError("Site frame/units/datum/content metadata disagree")
    if (
        site.origin.shape != (3,)
        or site.frame.shape != (3, 3)
        or any(
            a.dtype != np.dtype("float64") or not np.isfinite(a).all()
            for a in (site.origin, site.frame)
        )
    ):
        raise ValueError("Root port requires actual finite Float64 geometry")
    if site.origin[2] != 0 or (
        np.max(abs(site.frame.T @ site.frame - np.eye(3))) > 2e-12
        or abs(np.linalg.det(site.frame) - 1) > 2e-12
    ):
        raise ValueError("Declared flat root plane and proper rigid frame required")
    if not site.habitat.domain.covers(shapely.Point(site.origin[:2])):
        raise ValueError("Actual root has no prepared habitat support")
    return site
