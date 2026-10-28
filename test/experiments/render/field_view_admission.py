"""Bounded empirical field admission; source and depth authority remain explicit."""

import math
from dataclasses import dataclass

import numpy as np
from field_view_sampling import FieldGrid, ImageView


@dataclass(frozen=True)
class ViewDomain:
    grid: FieldGrid
    calibration_view: ImageView
    source_binding: str
    radiance_binding: str
    distance_m: tuple
    angle_deg: float
    measured_errors: tuple

    def __post_init__(self):
        object.__setattr__(self, "distance_m", tuple(self.distance_m))
        object.__setattr__(self, "measured_errors", tuple(self.measured_errors))
        if (
            not self.source_binding
            or not self.radiance_binding
            or len(self.distance_m) != 2
            or not 0 < self.distance_m[0] < self.distance_m[1] < math.inf
            or not 0 < self.angle_deg < 90
            or len(self.measured_errors) != 2
            or not all(math.isfinite(e) and e >= 0 for e in self.measured_errors)
        ):
            raise ValueError(
                "Bounded, source/light-bound empirical calibration required"
            )


def select_field(
    domain,
    view,
    *,
    was_field,
    source_binding,
    radiance_binding,
    complete,
    depth_separated,
):
    if (
        source_binding != domain.source_binding
        or radiance_binding != domain.radiance_binding
    ):
        return "parent", "changed_source_or_radiance"
    if not complete:
        return "parent", "original_remainder"
    if not depth_separated:
        return "parent", "external_depth_interleave_unqualified"
    # A declared research profile, not a global C/T certificate or device gate.
    if domain.measured_errors[0] > 0.05 or domain.measured_errors[1] > 0.03:
        return "parent", "empirical_quality_profile_failed"
    reference = domain.calibration_view
    if (view.width, view.height, view.focal_px) != (
        reference.width,
        reference.height,
        reference.focal_px,
    ) or view.basis != reference.basis:
        return "parent", "camera_profile_not_qualified"
    right, up = domain.grid.axes()
    toward = np.asarray(view.eye) - domain.grid.centre
    forward = toward @ np.asarray(domain.grid.toward_eye)
    if forward <= 0:
        return "parent", "behind_capture"
    angles = np.degrees(np.arctan2([toward @ right, toward @ up], forward))
    distance = np.linalg.norm(toward)
    low, high = domain.distance_m
    angle = domain.angle_deg
    if not was_field:
        # Enter through a smaller region; leave only at the calibrated boundary.
        inset = (high - low) / 8
        low, high, angle = low + inset, high - inset, angle * 0.8
    if not low <= distance <= high or np.any(abs(angles) > angle):
        return "parent", "outside_pose_region"
    return "field", "inside_empirically_tested_region"
