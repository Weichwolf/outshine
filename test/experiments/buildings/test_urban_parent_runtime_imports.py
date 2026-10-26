"""Source-free import contract for the later independent parent consumers."""

import unittest

from test_urban_parent_volume import assert_source_free_imports


class ParentRuntimeImportsTest(unittest.TestCase):
    def test_later_consumers_import_without_source_geometry(self):
        for module in [
            "urban_parent_phase",
            "urban_parent_transfer",
            "urban_parent_adaptive",
        ]:
            with self.subTest(module=module):
                assert_source_free_imports([module])


if __name__ == "__main__":
    unittest.main()
