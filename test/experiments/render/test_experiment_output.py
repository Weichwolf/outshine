import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from experiment_output import create_output


class OutputLocation(unittest.TestCase):
    def test_host_temp_and_existing_output(self):
        with tempfile.TemporaryDirectory(prefix="outshine-output-") as folder:
            output = Path(folder) / "fresh"
            create_output(output)
            self.assertTrue(output.is_dir())
            with self.assertRaises(FileExistsError):
                create_output(output)

    def test_canonical_tmp_alias(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-output-", dir="/tmp"
        ) as folder:
            output = Path("/tmp") / Path(folder).name / "fresh"
            create_output(output)
            self.assertTrue(output.resolve().is_dir())

    def test_temp_checkout_and_symlink_escape(self):
        with tempfile.TemporaryDirectory(prefix="outshine-output-") as folder:
            root = Path(folder)
            checkout = root / "checkout"
            checkout.mkdir()
            alias = root / "alias"
            alias.symlink_to(checkout, target_is_directory=True)
            with patch("experiment_output.ROOT", checkout):
                for output in (checkout / "output", alias / "output", root.parent):
                    with self.assertRaises(ValueError):
                        create_output(output)
                    self.assertFalse((checkout / "output").exists())


if __name__ == "__main__":
    unittest.main()
