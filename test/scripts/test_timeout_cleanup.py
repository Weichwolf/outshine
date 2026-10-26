import importlib.util
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(os.environ.get('OUTSHINE_RUN_SOURCE', ROOT / 'test/run.sh'))


class GroupSignals(unittest.TestCase):
    def setUp(self):
        path = SOURCE.parent / 'scripts/run_with_timeout.py'
        spec = importlib.util.spec_from_file_location('timeout_supervisor', path)
        self.supervisor = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.supervisor)

    def test_permission_denied_probe_means_group_still_exists(self):
        with mock.patch.object(self.supervisor.os, 'killpg', side_effect=PermissionError):
            self.assertTrue(self.supervisor.signal_group(123, 0))

    def test_permission_denied_for_real_signal_remains_an_error(self):
        with mock.patch.object(self.supervisor.os, 'killpg', side_effect=PermissionError):
            for number in (signal.SIGTERM, signal.SIGKILL):
                with self.subTest(number=number), self.assertRaises(PermissionError):
                    self.supervisor.signal_group(123, number)

    def test_only_missing_group_is_absent(self):
        with mock.patch.object(self.supervisor.os, 'killpg', side_effect=ProcessLookupError):
            for number in (0, signal.SIGTERM, signal.SIGKILL):
                with self.subTest(number=number):
                    self.assertFalse(self.supervisor.signal_group(123, number))


class SupervisorOutput(unittest.TestCase):
    def test_separate_stderr_preserves_payload_and_failure_status(self):
        with tempfile.TemporaryDirectory(prefix='outshine-timeout-streams-') as temporary:
            directory = Path(temporary)
            output, errors, marker = (directory / name for name in ('stdout', 'stderr', 'timeout'))
            Path(str(marker) + '.start').touch()
            command = [sys.executable, str(SOURCE.parent / 'scripts/run_with_timeout.py'),
                       str(os.getpid()), '5', str(output), str(marker), '--stderr-file', str(errors),
                       sys.executable, '-c',
                       'import sys; print("payload"); print("diagnostic", file=sys.stderr); sys.exit(7)']
            run = subprocess.run(command, capture_output=True, text=True, timeout=10)
            self.assertEqual(run.returncode, 7, run.stdout + run.stderr)
            self.assertEqual(output.read_text(), 'payload\n')
            self.assertEqual(errors.read_text(), 'diagnostic\n')
            self.assertFalse(marker.exists())
            self.assertFalse(Path(str(marker) + '.cleanup-failed').exists())


if __name__ == '__main__':
    unittest.main()
