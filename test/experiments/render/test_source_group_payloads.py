"""Same selection with absent fine rows and recorded selective file requests."""

import tempfile
import unittest
from pathlib import Path

import numpy as np
from compare_source_hierarchy import changed_frame, equivalent
from native_source_triangle_materials import DATA
from native_world_inputs import load
from source_group_payloads import Reads, build, query, read_original
from source_raster_candidates import select
from test_source_raster_candidates import source, uploads


class PayloadBoundaries(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="outshine-tests.local-payloads.")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.mesh = source(
            [
                [-0.7, -0.7, 0.5],
                [0.7, -0.7, 0.5],
                [0, 0.7, 0.5],
                [-0.94, -0.94, 0.5],
                [-0.92, -0.94, 0.5],
                [-0.94, -0.92, 0.5],
            ],
            [[0, 1, 2], [3, 4, 5]],
        )
        self.view, self.placement = uploads()

    def layout(self, mesh=None, **options):
        return build(
            self.mesh if mesh is None else mesh,
            self.directory / "data",
            leaf_triangles=1,
            **options,
        )

    def check(self, layout, reads=None, **options):
        expected = select(
            self.mesh, self.view, self.placement, [8, 8], raster_guard_px=0.0, **options
        )
        actual = query(
            layout,
            self.view,
            self.placement,
            [8, 8],
            raster_guard_px=0.0,
            reads=reads,
            **options,
        )
        equivalent(actual, expected)
        return actual

    def test_excluded_bytes_are_not_read_and_source_can_be_released(self):
        layout = self.layout()
        expected = self.mesh["positions"].copy()
        self.mesh["positions"][:] = 0
        original = read_original(layout, Reads())
        np.testing.assert_array_equal(original["positions"], expected)
        self.mesh["positions"][:] = expected
        reads = Reads()
        self.check(layout, reads)
        self.assertEqual(reads.requests, [("corners.bin", 0, 36, 36)])
        self.assertEqual(reads.work()["returned_bytes"], 36)
        with self.assertRaises(ValueError):
            layout.groups.faces.flags.writeable = True

    def test_repeat_reads_nothing_and_previous_bytes_are_immutable(self):
        layout = self.layout()
        first = Reads()
        self.check(layout, first)
        previous = first.retain(layout)
        with self.assertRaises(TypeError):
            previous.blocks[0] = b""
        second = Reads(previous)
        self.check(layout, second)
        self.assertFalse(second.requests)
        self.assertEqual(second.work()["reused_blocks"], 1)
        self.assertEqual(second.work()["retained_block_bytes"], 36)

    def test_another_preparation_never_reuses_ordinal_aliases(self):
        layout = self.layout()
        first = Reads()
        self.check(layout, first)
        other = build(self.mesh, self.directory / "other", leaf_triangles=1)
        reads = Reads(first.retain(layout))
        self.check(other, reads)
        self.assertEqual(reads.work()["reused_blocks"], 0)
        self.assertEqual(reads.work()["returned_bytes"], 36)

    def test_missing_or_corrupt_selected_block_retains_whole_not_partial(self):
        layout = self.layout()
        for payload in (b"", b"x" * 72):
            layout.corners.write_bytes(payload)
            reads = Reads()
            ids, work = query(
                layout,
                self.view,
                self.placement,
                [8, 8],
                raster_guard_px=0.0,
                reads=reads,
            )
            np.testing.assert_array_equal(ids, [0, 1])
            self.assertEqual(work["reason"], "payload_unavailable")
            self.assertFalse(work["complete_model"])
            self.assertFalse(reads.kept)

    def test_unselected_corruption_is_not_read_or_used(self):
        layout = self.layout()
        data = bytearray(layout.corners.read_bytes())
        data[36:] = b"x" * 36
        layout.corners.write_bytes(data)
        self.check(layout)

    def test_unknown_clip_keeps_original_without_payload_read(self):
        layout = self.layout()
        self.view.view("<f4")[0] = 4
        reads = Reads()
        self.check(layout, reads)
        self.assertFalse(reads.requests)

    def test_original_fallback_reads_and_validates_whole_payload(self):
        self.mesh = source(
            [[0.75, 0.75, 0.5], [-0.75, -0.75, 0.5], [0.25, 0.25, 0.6]], [[0, 1, 2]]
        )
        self.view.view("<f4")[4] = -1
        layout = self.layout()
        reads = Reads()
        self.check(layout, reads)
        self.assertEqual(reads.requests[0][0], "original.bin")
        layout.original.write_bytes(b"")
        ids, work = query(
            layout, self.view, self.placement, [8, 8], raster_guard_px=0.0
        )
        np.testing.assert_array_equal(ids, [0])
        self.assertEqual(work["reason"], "payload_unavailable")

    def test_fine_budget_keeps_original_even_when_unread_groups_exist(self):
        layout = self.layout()
        reads = Reads()
        ids, work = self.check(layout, reads, pairs_most=0)
        np.testing.assert_array_equal(ids, [0, 1])
        self.assertEqual(work["reason"], "triangle_pixel_pair_budget")
        self.assertEqual(reads.work()["returned_bytes"], 36)

    def test_invalid_query_never_issues_io(self):
        layout = self.layout()
        for options in (
            {"pairs_most": -1},
            {"chunk_pairs": 0},
            {"vertex_batch_size": 0},
        ):
            reads = Reads()
            with self.assertRaises(ValueError):
                self.check(layout, reads, **options)
            self.assertFalse(reads.requests)

    def test_empty_demand_does_not_require_payload(self):
        self.mesh = source(
            [[-0.94, -0.94, 0.5], [-0.92, -0.94, 0.5], [-0.94, -0.92, 0.5]], [[0, 1, 2]]
        )
        layout = self.layout()
        layout.corners.unlink()
        reads = Reads()
        ids, _ = self.check(layout, reads)
        self.assertEqual(len(ids), 0)
        self.assertFalse(reads.requests)


class WholeNativeInput(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest, _, _, cls.mesh = load(DATA, attributes=True)

    def test_static_layouts_guards_motion_and_repeat_preserve_ids_and_fallbacks(self):
        with (
            tempfile.TemporaryDirectory(prefix="outshine-tests.native-payloads.") as d,
            np.load(DATA / "native-visible-triangles.npz", allow_pickle=False) as a,
        ):
            layouts = [
                build(self.mesh, Path(d) / name, order=name)
                for name in ("source", "morton", "size")
            ]
            p = a["placements/words"]
            raster = self.manifest["raster"]
            frames = [
                a[pose + "/view_words"] for pose in ("near-direction", "revealing-side")
            ]
            frames += [
                changed_frame(frames[0], angle, np.zeros(3))
                for angle in range(0, 360, 30)
            ]
            frames += [
                changed_frame(frames[0], 0, index * np.array([100, 25, -200]))
                for index in range(12)
            ]
            previous = [None] * 3
            for guard in (0.0, 1 / 256):
                for view in frames:
                    expected = select(self.mesh, view, p, raster, raster_guard_px=guard)
                    for at, layout in enumerate(layouts):
                        reads = Reads(previous[at])
                        actual = query(
                            layout, view, p, raster, raster_guard_px=guard, reads=reads
                        )
                        equivalent(actual, expected)
                        previous[at] = reads.retain(layout)
                        repeat = Reads(previous[at])
                        equivalent(
                            query(
                                layout,
                                view,
                                p,
                                raster,
                                raster_guard_px=guard,
                                reads=repeat,
                            ),
                            expected,
                        )
                        if reads.requests and reads.requests[0][0] == "corners.bin":
                            self.assertEqual(repeat.work()["returned_bytes"], 0)
                        self.assertLessEqual(
                            sum(len(v) for v in previous[at].blocks.values()),
                            layout.triangles * 36,
                        )


if __name__ == "__main__":
    unittest.main()
