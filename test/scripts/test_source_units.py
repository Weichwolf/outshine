"""Production source coverage must agree across platform builds and analysis."""

from pathlib import Path
import tempfile
import unittest

from source_units import source_units


class NativeSourceSelection(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='outshine-source-units-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.names = ['src/client/Main.cpp', 'src/render/Scene.cpp',
                      'src/render/device/native/Shared.cpp',
                      'src/render/device/native/vulkan/Device.cpp',
                      'src/render/device/native/metal/Device.mm',
                      'src/render/device/native/metal/Reflection.cpp',
                      'src/render/device/native/metal/Device.h']
        for name in self.names:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('')

    def names_for(self, system):
        return {path.relative_to(self.root).as_posix()
                for path in source_units(self.root, system)}

    def test_linux_selects_vulkan_and_shared_cpp(self):
        self.assertEqual(self.names_for('Linux'), set(self.names[:4]))

    def test_darwin_selects_metal_and_shared_cpp(self):
        self.assertEqual(self.names_for('Darwin'), set(self.names[:3] + self.names[4:6]))

    def test_unselected_backend_does_not_hide_shared_units(self):
        self.assertEqual(self.names_for('Other'), set(self.names[:3]))

    def test_new_nested_backend_unit_is_included_without_a_second_manifest(self):
        path = self.root / 'src/render/device/native/vulkan/transfers/Copy.cpp'
        path.parent.mkdir()
        path.write_text('')
        self.assertIn(path.resolve(), source_units(self.root, 'Linux'))
        self.assertNotIn(path.resolve(), source_units(self.root, 'Darwin'))


if __name__ == '__main__':
    unittest.main()
