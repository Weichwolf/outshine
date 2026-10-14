import ctypes
import os
import threading
import time
import unittest
from unittest.mock import patch

import shapely

from outshine_ref.buildings.modelling import HipPlanner,HipWorkerCancelled,HipWorkerError,SkeletonBudgetExceeded,SkeletonLimits
from outshine_ref.buildings.modelling.hip_wire import receive,send


def blocked_native(connection):
    send(connection,('ready',os.getpid()));receive(connection);send(connection,('entered',None))
    ctypes.CDLL(None).pause()


def crashed_worker(connection):
    send(connection,('ready',os.getpid()));receive(connection);os._exit(17)


def malformed_worker(connection):
    send(connection,('ready',os.getpid()));receive(connection);send(connection,('ok',None))


def absent(pid):
    try:os.kill(pid,0)
    except ProcessLookupError:return True
    return False


@unittest.skipUnless(os.name=='posix','Desktop POSIX process reference')
class HipWorker(unittest.TestCase):
    def test_native_deadline_reaps_child_and_next_request_recovers(self):
        polygon=shapely.box(0,0,12,5)
        with patch('outshine_ref.buildings.modelling.hip_worker.worker_loop',blocked_native):
            planner=HipPlanner(SkeletonLimits(seconds=.2));planner.__enter__();pid=planner.pid
            started=time.monotonic()
            with self.assertRaises(SkeletonBudgetExceeded):planner.prepare(polygon)
            self.assertLess(time.monotonic()-started,1.)
            self.assertTrue(planner.last_worker_entered);self.assertEqual(planner.last_exit_code,-9);self.assertTrue(absent(pid))
        with planner:
            plan=planner.prepare(polygon);self.assertNotEqual(planner.pid,pid)
            for data in [plan.positions_m,plan.distances_m]:
                with self.assertRaises(ValueError):data.flags.writeable=True
        self.assertIsNone(planner.pid)

    def test_crash_and_invalid_reply_retire_the_connection(self):
        for target in [crashed_worker,malformed_worker]:
            with self.subTest(target=target.__name__),patch('outshine_ref.buildings.modelling.hip_worker.worker_loop',target):
                with HipPlanner() as planner:
                    pid=planner.pid
                    with self.assertRaises(HipWorkerError):planner.prepare(shapely.box(0,0,12,5))
                    self.assertTrue(absent(pid));self.assertIsNone(planner.pid)

    def test_cancel_busy_request_and_close_are_explicit(self):
        with patch('outshine_ref.buildings.modelling.hip_worker.worker_loop',blocked_native),HipPlanner() as planner:
            errors=[]
            def request():
                try:planner.prepare(shapely.box(0,0,12,5))
                except Exception as error:errors.append(error)
            thread=threading.Thread(target=request);thread.start();deadline=time.monotonic()+1.
            while not planner.last_worker_entered and thread.is_alive() and time.monotonic()<deadline:time.sleep(.002)
            self.assertTrue(planner.last_worker_entered)
            with self.assertRaises(HipWorkerError):planner.prepare(shapely.box(0,0,12,5))
            pid=planner.pid;self.assertTrue(planner.cancel());thread.join(1.)
            self.assertFalse(thread.is_alive());self.assertEqual(len(errors),1);self.assertIsInstance(errors[0],HipWorkerCancelled)
            self.assertTrue(absent(pid))
        planner.close()
        with self.assertRaises(HipWorkerError):planner.prepare(shapely.box(0,0,12,5))

    def test_domain_error_does_not_poison_a_complete_exchange(self):
        with HipPlanner() as planner:
            pid=planner.pid
            with self.assertRaises(ValueError):planner.prepare(shapely.Polygon())
            self.assertEqual(len(planner.prepare(shapely.box(0,0,12,5)).facets),4)
            self.assertEqual(planner.pid,pid)

    def test_close_aborts_an_active_native_call(self):
        with patch('outshine_ref.buildings.modelling.hip_worker.worker_loop',blocked_native):
            planner=HipPlanner();planner.__enter__();errors=[]
            def request():
                try:planner.prepare(shapely.box(0,0,12,5))
                except Exception as error:errors.append(error)
            thread=threading.Thread(target=request);thread.start();deadline=time.monotonic()+1.
            while not planner.last_worker_entered and thread.is_alive() and time.monotonic()<deadline:time.sleep(.002)
            self.assertTrue(planner.last_worker_entered);pid=planner.pid;planner.close();thread.join(1.)
            self.assertFalse(thread.is_alive());self.assertTrue(absent(pid));self.assertIsNone(planner.pid)
            self.assertEqual(len(errors),1);self.assertIsInstance(errors[0],HipWorkerCancelled)
