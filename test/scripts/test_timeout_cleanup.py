import ctypes
import importlib.util
import os
from pathlib import Path
import re
import shlex
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(os.environ.get('OUTSHINE_RUN_SOURCE', ROOT / 'test/run.sh'))


def alive(pid):
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False


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


class TimeoutCleanup(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='outshine-timeout-cleanup-')
        self.directory = Path(self.temp.name)
        self.runners = []
        self.children = []
        self.child = self.directory / 'ignores-term.py'
        self.child.write_text('import os,signal,sys,time\n'
                              'from pathlib import Path\n'
                              'signal.signal(signal.SIGTERM,signal.SIG_IGN)\n'
                              'if "TEST_STARTED" in os.environ:\n'
                              ' Path(os.environ["TEST_STARTED"]).write_text(str(os.getpid()))\n'
                              'time.sleep(float(os.environ.get("TEST_START_DELAY","0")))\n'
                              'Path(sys.argv[1]).write_text(str(os.getpid()))\n'
                              'while True: time.sleep(1)\n')
        self.foreign_pid = self.directory / 'foreign.pid'
        self.foreign = subprocess.Popen([sys.executable, str(self.child), str(self.foreign_pid)],
                                        start_new_session=True)
        self.runners.append(self.foreign)
        self.read_pid(self.foreign_pid)
        fixture = self.directory / 'fixture.sh'
        fixture.write_text('#!/bin/sh\ntrap "exit 0" TERM\n'
                           '"$TEST_PYTHON" "$TEST_CHILD" "$TEST_PIDFILE" &\n'
                           'while [ ! -s "$TEST_PIDFILE" ]; do sleep 0.01; done\n'
                           '[ "$1" = normal ] && exit 0\nwait\n')
        fixture.chmod(0o700)
        functions = SOURCE.read_text()
        selected = []
        for name in ('Die', 'KillRunning', 'RunWithTimeout'):
            match = re.search(r'^' + name + r'\(\) \{\n.*?^\}\n', functions, re.M | re.S)
            self.assertIsNotNone(match, name)
            selected.append(match.group())
        helper = self.directory / 'runner.sh'
        helper.write_text('set -u\nRUNNING_OWNER=""\nRUNNING_ERROR=""\n'
                          'ROOT=' + shlex.quote(str(SOURCE.resolve().parents[1])) + '\n'
                          'TIMEOUT_S="$TEST_TIMEOUT_S"\n' +
                          '\n'.join(selected) +
                          '\ntrap \'KillRunning; exit 143\' TERM\n'
                          'RunWithTimeout "$TEST_FIXTURE" "$TEST_LOG" "$TEST_MARKER" "$1"\n'
                          'status=$?\nprintf "%s\\n" "$status"\nexit "$status"\n')
        self.helper = helper
        self.pidfile = self.directory / 'child.pid'
        self.marker = self.directory / 'timeout.marker'
        self.started = self.directory / 'started.pid'
        self.environment = dict(os.environ, TEST_PYTHON=sys.executable,
                                TEST_CHILD=str(self.child), TEST_PIDFILE=str(self.pidfile),
                                TEST_FIXTURE=str(fixture), TEST_LOG=str(self.directory / 'case.log'),
                                TEST_MARKER=str(self.marker), TEST_STARTED=str(self.started))

    def read_pid(self, path):
        until = time.monotonic() + 5
        while time.monotonic() < until:
            if path.exists() and path.read_text().strip():
                pid = int(path.read_text())
                self.children.append(pid)
                return pid
            time.sleep(0.01)
        detail = ''
        owner = self.runners[-1]
        if owner.poll() is not None:
            stdout, stderr = owner.communicate()
            detail = f'; owner status={owner.returncode}, stdout={stdout!r}, stderr={stderr!r}'
        case_log = self.directory / 'case.log'
        if case_log.exists():
            detail += '; case=' + case_log.read_text(errors='replace')[-1000:]
        self.fail('fixture did not publish its PID' + detail)

    def start(self, mode, timeout=1, delay=0, wait_for_publication=True):
        environment = dict(self.environment, TEST_TIMEOUT_S=str(timeout), TEST_START_DELAY=str(delay))
        runner = subprocess.Popen(['sh', str(self.helper), mode], env=environment,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  start_new_session=True, text=True)
        self.runners.append(runner)
        child = self.read_pid(self.pidfile if wait_for_publication else self.started)
        return runner, child

    def assert_clean(self, runner, child):
        stdout, stderr = runner.communicate(timeout=10)
        self.assertFalse(alive(child), 'TERM-ignoring descendant survived: ' + stderr)
        self.assertIsNone(self.foreign.poll(), 'foreign process was killed')
        return stdout, stderr

    def test_timeout_cleans_descendants_before_return(self):
        runner, child = self.start('timeout')
        stdout, stderr = self.assert_clean(runner, child)
        self.assertEqual(runner.returncode, 0, stderr)
        self.assertTrue(self.marker.exists(), stdout + stderr)

    def test_normal_owner_exit_cleans_descendants(self):
        self.assert_normal_exit()

    def assert_normal_exit(self, delay=0):
        runner, child = self.start('normal', timeout=30, delay=delay)
        stdout, stderr = self.assert_clean(runner, child)
        self.assertEqual(runner.returncode, 0, stdout + stderr)
        self.assertFalse(self.marker.exists())

    def test_normal_owner_exit_cleans_descendants_after_slow_start(self):
        self.assert_normal_exit(delay=1.25)

    def assert_interrupted(self, delay=0):
        runner, child = self.start('timeout', timeout=30, delay=delay)
        runner.terminate()
        self.assert_clean(runner, child)
        self.assertEqual(runner.returncode, 143)
        self.assertFalse(self.marker.exists(), 'the interrupt case must not be killed by its watchdog')

    def test_interrupt_cleans_descendants(self):
        self.assert_interrupted()

    def test_interrupt_cleans_descendants_after_slow_start(self):
        self.assert_interrupted(delay=1.25)

    def test_interrupt_before_descendant_pid_publication(self):
        runner, child = self.start('timeout', timeout=30, delay=2, wait_for_publication=False)
        self.assertFalse(self.pidfile.exists(), 'interrupt must precede normal PID publication')
        runner.terminate()
        self.assert_clean(runner, child)
        self.assertEqual(runner.returncode, 143)
        self.assertFalse(self.marker.exists())

    def test_interrupt_before_permission_to_launch(self):
        registered = self.directory / 'registered.pid'
        self.environment['TEST_REGISTERED'] = str(registered)
        source = self.helper.read_text()
        self.assertIn(': >"$marker.start"', source)
        self.helper.write_text(source.replace(': >"$marker.start"',
                                             'printf "%s" "$RUNNING_OWNER" >"$TEST_REGISTERED"\n'
                                             '  while :; do sleep 0.02; done'))
        runner = subprocess.Popen(['sh', str(self.helper), 'timeout'],
                                  env=dict(self.environment, TEST_TIMEOUT_S='30'),
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  start_new_session=True, text=True)
        self.runners.append(runner)
        supervisor = self.read_pid(registered)
        runner.terminate()
        self.assert_clean(runner, supervisor)
        self.assertEqual(runner.returncode, 143)
        self.assertFalse(self.started.exists(), 'target launched before ownership registration')
        self.assertFalse(self.marker.exists())

    def test_timeout_kills_owner_that_ignores_term(self):
        fixture = Path(self.environment['TEST_FIXTURE'])
        fixture.write_text(fixture.read_text().replace('trap "exit 0" TERM', 'trap "" TERM'))
        runner, child = self.start('timeout')
        stdout, stderr = self.assert_clean(runner, child)
        self.assertEqual(runner.returncode, 137, stdout + stderr)
        self.assertTrue(self.marker.exists())

    def test_exit_status_without_argument_is_preserved(self):
        fixture = Path(self.environment['TEST_FIXTURE'])
        fixture.write_text('#!/bin/sh\nexit 7\n')
        runner = subprocess.Popen(['sh', str(self.helper), ''],
                                  env=dict(self.environment, TEST_TIMEOUT_S='30'),
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  start_new_session=True, text=True)
        self.runners.append(runner)
        stdout, stderr = runner.communicate(timeout=10)
        self.assertEqual(runner.returncode, 7, stdout + stderr)
        self.assertFalse(self.marker.exists())
        self.assertIsNone(self.foreign.poll(), 'foreign process was killed')

    def test_owner_killed_cleans_descendants(self):
        runner, child = self.start('timeout', timeout=30)
        runner.kill()
        self.assert_clean(runner, child)
        self.assertEqual(runner.returncode, -signal.SIGKILL)
        self.assertFalse(self.marker.exists())

    @unittest.skipUnless(sys.platform.startswith('linux'), 'Linux orphan adoption contract')
    def test_supervisor_reaps_before_outer_subreaper(self):
        libc = ctypes.CDLL(None, use_errno=True)
        previous = ctypes.c_int()
        self.assertEqual(libc.prctl(37, ctypes.byref(previous), 0, 0, 0), 0)
        self.assertEqual(libc.prctl(36, 1, 0, 0, 0), 0)
        try:
            self.assert_normal_exit()
        finally:
            self.assertEqual(libc.prctl(36, previous.value, 0, 0, 0), 0)

    def tearDown(self):
        for runner in self.runners:
            if runner.poll() is None:
                try:
                    os.killpg(runner.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            runner.wait(timeout=5)
            for stream in (runner.stdout, runner.stderr):
                if stream:
                    stream.close()
        for pid in self.children:
            if alive(pid):
                try:
                    os.kill(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            try:
                os.waitpid(pid, os.WNOHANG)
            except ChildProcessError:
                pass
        self.temp.cleanup()


if __name__ == '__main__':
    unittest.main()
