"""Optional attached dormer roofs, preserving the existing facade/function owners."""

from dataclasses import asdict

import shapely
from dormer_assets import prepare
from dormer_present import bound
from dormer_windows import DEPTH_M
from facade_parts import append
from outshine_ref.buildings.modelling.skeleton_plan import SkeletonBudgetExceeded
from tile_elevations import ElevationFacades
from tile_facade_recess import demand
from tile_facades import recipe_for
from tile_roof_envelopes import choose


class DormerRoofs:
    def __init__(
        self, previous, rows, view, entrances, *, frontages=None, bay_method="bisect"
    ):
        self.previous, self.view, self.entrances = previous, view, entrances
        self.sources = {r["id"]: r for r in rows if r["layer"] == "building"}
        self.dormer_assets, self.dormer_rejected, self.dormer_completed = {}, [], []
        self.failed = set()
        self.current_envelope = None
        self.frontages, self.bay_method = frontages, bay_method

    def __getattr__(self, name):
        return getattr(self.previous, name)

    def prepare_asset(self, row, parameters, choice):
        def clear(geometry, low, high):
            return self.entrances.clear(row, geometry, high, low_m=low)

        if self.frontages is not None:
            try:
                frontages = self.frontages(row, parameters | {"eave_m": choice.eave_m})
                return prepare(
                    row,
                    parameters,
                    choice,
                    clear=clear,
                    frontages=frontages,
                    bay_method=self.bay_method,
                )
            except (
                ValueError,
                NotImplementedError,
                SkeletonBudgetExceeded,
                shapely.errors.GEOSException,
            ) as error:
                self.dormer_rejected.append(
                    {"id": row["id"], "reason": "aligned fallback: " + str(error)}
                )
        return prepare(row, parameters, choice, clear=clear)

    def asset(self, row, parameters, choice):
        if row["id"] in self.dormer_assets:
            return self.dormer_assets[row["id"]]
        if row["id"] in self.failed:
            return None
        if (
            choice.kind != "hip"
            or choice.rise_m < 2.8
            or row["clipped"]
            or parameters["hide3d"]
            or parameters["minHeight"] != 0
            or not 150 <= row["polygon"].area <= 2500
            or recipe_for(row, parameters)["family"] not in ("residential", "mixed-use")
        ):
            return None
        if row is not self.sources[row["id"]]:
            raise ValueError("Dormer planning requires its immutable source snapshot")
        try:
            asset = self.prepare_asset(row, parameters, choice)
            self.dormer_assets[row["id"]] = asset
            return asset
        except (
            ValueError,
            NotImplementedError,
            SkeletonBudgetExceeded,
            shapely.errors.GEOSException,
        ) as error:
            self.failed.add(row["id"])
            self.dormer_rejected.append({"id": row["id"], "reason": str(error)})
            return None

    def __call__(self, row, parameters, material):
        choice = choose(row, parameters)
        asset = self.asset(row, parameters, choice)
        if asset is None:
            result = self.previous(row, parameters, material)
            self.current_envelope = self.previous.current_envelope
            return result
        if row is not self.sources[row["id"]]:
            raise ValueError(
                "Dormer draw requires its immutable prepared source snapshot"
            )
        near = any(
            demand(p.frame(), p.base_m, self.view, depth=DEPTH_M) is not None
            for p in asset.dormers
        )
        mode = "near" if near else "full"
        try:
            product = asset.near_products() if near else asset.full
        except (ValueError, NotImplementedError, shapely.errors.GEOSException) as error:
            self.dormer_rejected.append(
                {"id": row["id"], "reason": "near pockets: " + str(error)}
            )
            product, mode = asset.full, "full"
        self.current_envelope = product.physical
        info = {
            "roof_kind": choice.kind,
            "roof_rise_m": choice.rise_m,
            "eave_m": choice.eave_m,
            "ridge_degrees": choice.ridge_degrees,
            "selected_eave_edge": choice.edge_index,
            "roof_origin": choice.origin,
            "roof_reason": choice.reason,
            "roof_dormer_plan": [asdict(p) for p in asset.dormers],
            "roof_dormer_product": mode,
            "roof_dormer_origin": "procedural attached boxes on actual source roof, not supplied dormers",
            "roof_dormer_status": "Closed roof with simple window pockets; attic rooms/access/night programme unplanned",
            "form": "Closed original roof and attached dormer union",
        }
        if asset.profile is not None:
            info["roof_profile"] = asset.profile
        self.dormer_completed.append(
            {"id": row["id"], "dormers": len(asset.dormers), "product": mode}
        )
        return bound(product.body, material, 0), info


class DormerFacades(ElevationFacades):
    def __init__(
        self, rows, view, *, dormer_layout="edge", bay_method="bisect", **kwargs
    ):
        if dormer_layout not in ("edge", "facade") or bay_method not in (
            "bisect",
            "linear",
        ):
            raise ValueError("Known dormer layout and bay selection required")
        super().__init__(rows, view, **kwargs)
        self.base = DormerRoofs(
            self.base,
            self.rows,
            view,
            self.entrances,
            frontages=self.stage_roof_facades if dormer_layout == "facade" else None,
            bay_method=bay_method,
        )

    def __call__(self, row, parameters, material):
        try:
            mesh, info = super().__call__(row, parameters, material)
        finally:
            self.roof_staged_elevations.pop(row["id"], None)
        if "roof_dormer_product" not in info:
            return mesh, info
        asset = self.base.dormer_assets[row["id"]]
        if asset.frontages:
            actual = {f["id"]: f for f in info.get("facade_plan", ())}
            keys = ("a", "b", "width_m", "columns")
            if any(
                f["id"] not in actual or any(f[k] != actual[f["id"]][k] for k in keys)
                for f in asset.frontages
            ):
                raise ValueError("Dormer axes differ from the final bound facade plan")
            info = info | {"roof_dormer_frontages": list(asset.frontages)}
        product = getattr(asset, info["roof_dormer_product"])
        front = self.binding(material, (0.72, 0.64))
        pieces = [bound(product.skin, material, front)]
        if product.details is not None:
            pieces.append(
                bound(
                    product.details, material, front, self.binding(material, (1.0, 1.0))
                )
            )
        return append(mesh, pieces), info

    def report(self):
        return super().report() | {
            "dormers": self.base.dormer_completed,
            "dormer_rejected": self.base.dormer_rejected,
        }
