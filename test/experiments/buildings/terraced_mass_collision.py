"""Exact interval union for three closed components, without a remeshing Boolean."""

from structural_shell_geometry import NativeColumns, native


class ComponentColumns:
    def __init__(self, asset, low, high):
        self.components = tuple(
            NativeColumns(native(p.mesh, p.vertex_roots), low, high)
            for p in (asset.shell, asset.stairs, asset.landing)
        )

    def intervals(self, point):
        spans = sorted(
            span for component in self.components for span in component.intervals(point)
        )
        union = []
        for lo, hi in spans:
            if union and lo <= union[-1][1]:
                union[-1] = union[-1][0], max(hi, union[-1][1])
            else:
                union.append((lo, hi))
        return union
