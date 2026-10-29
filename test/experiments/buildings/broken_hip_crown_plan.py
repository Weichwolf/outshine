"""Deferred two-slope requests on the existing source-bound functional crown."""

from dataclasses import dataclass, fields, replace

from hip_crown_plan import LIMITS, HipCrown, with_hip
from outshine_ref.buildings.modelling.scope import scalar

RECIPE = "outshine/source-height-broken-hip-crown/1"


def parameters(run, height, method, maximum):
    run, height = scalar(run), scalar(height)
    if (
        not 0 < run < height < 1
        or method not in ("direct", "logical")
        or type(maximum) is not int
        or not 1 <= maximum <= 65536
    ):
        raise ValueError("Bounded two-slope crown request required")
    return run, height


@dataclass(frozen=True)
class BrokenHipCrown(HipCrown):
    base_hip: object = None
    run_fraction: float = 0.15
    height_fraction: float = 0.65
    profile_method: str = "direct"
    maximum_triangles: int = 65536
    kind: str = "broken-hip"

    def __post_init__(self):
        run, height = parameters(
            self.run_fraction,
            self.height_fraction,
            self.profile_method,
            self.maximum_triangles,
        )
        base = self.base_hip
        if (
            not isinstance(base, HipCrown)
            or isinstance(base, BrokenHipCrown)
            or self.identifier != base.identifier + "/broken-profile"
            or self.kind != "broken-hip"
            or self.fallback is not base.fallback
            or any(
                getattr(self, f.name) != getattr(base, f.name)
                for f in fields(HipCrown)
                if f.name not in ("identifier", "kind", "fallback")
            )
        ):
            raise ValueError(
                "Broken crown requires the unchanged bound base hip request"
            )
        object.__setattr__(self, "run_fraction", run)
        object.__setattr__(self, "height_fraction", height)


def with_broken_hip(
    plan,
    *,
    run_fraction=0.15,
    height_fraction=0.65,
    profile_method="direct",
    limits=LIMITS,
    maximum_triangles=65536,
):
    run, height = parameters(
        run_fraction, height_fraction, profile_method, maximum_triangles
    )
    if plan.access is not None:
        return plan
    if isinstance(plan.crown, BrokenHipCrown):
        old = plan.crown
        if (run, height, profile_method, maximum_triangles) == (
            old.run_fraction,
            old.height_fraction,
            old.profile_method,
            old.maximum_triangles,
        ):
            return plan
        plan = replace(plan, crown=old.base_hip)
    current = with_hip(plan, limits=limits)
    hip = current.crown
    if not isinstance(hip, HipCrown):
        return current
    values = {f.name: getattr(hip, f.name) for f in fields(HipCrown)}
    values.update(identifier=hip.identifier + "/broken-profile", kind="broken-hip")
    return replace(
        current,
        crown=BrokenHipCrown(
            **values,
            base_hip=hip,
            run_fraction=run,
            height_fraction=height,
            profile_method=profile_method,
            maximum_triangles=maximum_triangles,
        ),
        reason="deferred procedural two-slope crown within original source-height reserve",
    )
