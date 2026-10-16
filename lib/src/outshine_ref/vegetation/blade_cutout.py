"""Prepared authored parameter cutouts on the existing analytical blade carrier."""

import hashlib
import json
from dataclasses import asdict, dataclass
from functools import cached_property

import numpy as np
import shapely
from shapely.affinity import scale

from .blade_surface import Blade


@dataclass(frozen=True)
class BladeCutout:
    """One polygon in (s,t), transverse [-1,1], longitudinal [-petiole/length,1].

    This trims a Blade carrier; its analytic width/curvature remain meaningful.
    It is not a new species declaration or a change to existing PlantPlan recipes.
    """

    blade: Blade
    domain: shapely.Polygon

    def __post_init__(self):
        if not isinstance(self.blade, Blade):
            raise TypeError("existing analytical Blade carrier required")
        if not isinstance(self.domain, shapely.Polygon):
            raise TypeError("one authored parameter polygon required")
        if (
            self.domain.is_empty
            or shapely.get_num_coordinates(self.domain) > 1024
            or shapely.get_coordinate_dimension(self.domain) != 2
            or not self.domain.is_valid
            or not np.isfinite(shapely.get_coordinates(self.domain)).all()
        ):
            raise ValueError("finite valid bounded two-dimensional cutout required")
        lower = -self.blade.petiole / self.blade.length
        if not shapely.box(-1, lower, 1, 1).covers(self.domain):
            raise ValueError("cutout outside carrier parameter bounds")
        midrib = shapely.LineString([(0, lower), (0, 1)])
        if not self.domain.covers(midrib):
            raise ValueError("complete midrib and attachment anchors required")
        tip = self.domain.intersection(shapely.LineString([(-1, 1), (1, 1)]))
        if not tip.equals(shapely.Point(0, 1)):
            raise ValueError("one noncollapsed terminal parameter point required")
        if self.blade.petiole > 0:
            if self.blade.base_fill == 0 or not self.domain.covers(
                shapely.box(-1, lower, 1, 0)
            ):
                raise ValueError("complete regular petiole band required")
        elif self.blade.base_fill == 0:
            base = self.domain.intersection(shapely.LineString([(-1, 0), (1, 0)]))
            if not base.equals(shapely.Point(0, 0)):
                raise ValueError("zero-width base requires a single parameter point")
        object.__setattr__(self, "domain", shapely.normalize(self.domain))

    @cached_property
    def parameter_domain_m(self):
        return scale(
            self.domain, self.blade.width / 2, self.blade.length, origin=(0, 0)
        )

    @cached_property
    def identity(self):
        return hashlib.sha256(
            json.dumps(self.recipe(), sort_keys=True).encode()
        ).hexdigest()

    def recipe(self):
        blade = {
            name: int(value) if name in ("lobes", "fold_power") else float(value)
            for name, value in asdict(self.blade).items()
        }
        return {
            "format": "blade-cutout-1",
            "blade": blade,
            "domain_wkb": self.domain.wkb_hex,
        }

    @classmethod
    def from_recipe(cls, value):
        if (
            set(value) != {"format", "blade", "domain_wkb"}
            or value["format"] != "blade-cutout-1"
        ):
            raise ValueError("complete supported blade cutout recipe required")
        return cls(Blade(**value["blade"]), shapely.from_wkb(value["domain_wkb"]))

    def mesh(self, along=32, across=2, *, max_faces=8192, max_vertices=8192):
        from .cutout_mesh import cutout_mesh

        return cutout_mesh(self, along, across, max_faces, max_vertices)
