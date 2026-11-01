"""Executable SQLite VFS reference; private flat cache directories only."""

import contextlib
import ctypes as C
import fcntl
import json
import os
from pathlib import Path
import sqlite3
import stat
import _sqlite3
import threading
import time


OK, BUSY, IOERR, FULL, CANTOPEN = 0, 5, 10, 13, 14
SIZE_HINT, CHUNK_SIZE = 5, 6
PTR = C.c_void_p
I64 = C.c_int64


class File(C.Structure):
    _fields_ = [('methods', PTR)]


class Methods(C.Structure):
    _fields_ = [('version', C.c_int)] + [
        (name, PTR) for name in (
            'close', 'read', 'write', 'truncate', 'sync', 'size', 'lock',
            'unlock', 'reserved', 'control', 'sector', 'characteristics',
            'shm_map', 'shm_lock', 'shm_barrier', 'shm_unmap', 'fetch', 'unfetch',
        )
    ]


class Vfs(C.Structure):
    _fields_ = [
        ('version', C.c_int), ('file_bytes', C.c_int), ('pathname_bytes', C.c_int),
        ('next', PTR), ('name', C.c_char_p), ('app', PTR),
    ] + [(name, PTR) for name in (
        'open', 'delete', 'access', 'full_path', 'dl_open', 'dl_error', 'dl_symbol',
        'dl_close', 'random', 'sleep', 'time', 'error', 'time64', 'set_call',
        'get_call', 'next_call',
    )]


OPEN = C.CFUNCTYPE(C.c_int, PTR, PTR, PTR, C.c_int, PTR)
CLOSE = C.CFUNCTYPE(C.c_int, PTR)
WRITE = C.CFUNCTYPE(C.c_int, PTR, PTR, C.c_int, I64)
TRUNCATE = C.CFUNCTYPE(C.c_int, PTR, I64)
SIZE = C.CFUNCTYPE(C.c_int, PTR, C.POINTER(I64))
CONTROL = C.CFUNCTYPE(C.c_int, PTR, C.c_int, PTR)
SHM_MAP = C.CFUNCTYPE(C.c_int, PTR, C.c_int, C.c_int, C.c_int, PTR)


class BudgetVfs:
    def __init__(self, directory, limit, reserve):
        self.directory = Path(directory).resolve()
        self.directory.mkdir(parents=True, exist_ok=True)
        if not 0 < reserve < limit:
            raise ValueError('require 0 < maintenance reserve < file limit')
        self.limit, self.reserve = limit, reserve
        self.block_bytes = os.statvfs(self.directory).f_frsize
        self.lock = threading.RLock()
        self.local = threading.local()
        self.lock_file = (self.directory / 'budget.lock').open('a+b')
        config = {'version': 1, 'limit': limit, 'reserve': reserve}
        with self.exclusive():
            path = self.directory / 'budget.json'
            if path.exists():
                if json.loads(path.read_text()) != config:
                    raise ValueError('processes must use the same persistent budget')
            else:
                path.write_text(json.dumps(config, sort_keys=True))
        self.files = {}
        self.errors = []
        self.stats = dict(writes=0, growth_checks=0, denials=0, hints_ignored=0,
                          peak_file_bytes=0, peak_wal_bytes=0, peak_billable_bytes=0,
                          peak_allocated_bytes=0, lock_wait_ns=0)
        self.lib = C.CDLL(_sqlite3.__file__)
        self.lib.sqlite3_libversion.restype = C.c_char_p
        if self.lib.sqlite3_libversion().decode() != sqlite3.sqlite_version:
            raise RuntimeError('VFS registration must use Python SQLite itself')
        self.lib.sqlite3_vfs_find.argtypes = [C.c_char_p]
        self.lib.sqlite3_vfs_find.restype = C.POINTER(Vfs)
        self.lib.sqlite3_vfs_register.argtypes = [C.POINTER(Vfs), C.c_int]
        self.lib.sqlite3_vfs_unregister.argtypes = [C.POINTER(Vfs)]
        self.parent = self.lib.sqlite3_vfs_find(None)
        if self.parent.contents.version != 3:
            raise RuntimeError('this experiment requires VFS ABI version 3')
        self.vfs = Vfs.from_buffer_copy(self.parent.contents)
        self.name_bytes = f'outshine-budget-{os.getpid()}-{id(self)}'.encode()
        self.name = self.name_bytes.decode()
        self.vfs.name, self.vfs.next = self.name_bytes, None
        self.callbacks = {
            'open': OPEN(self.safe(self.open_file)),
            'close': CLOSE(self.safe(self.close_file)),
            'write': WRITE(self.safe(self.write)),
            'truncate': TRUNCATE(self.safe(self.truncate)),
            'control': CONTROL(self.safe(self.control)),
            'shm_map': SHM_MAP(self.safe(self.shm_map)),
        }
        self.vfs.open = C.cast(self.callbacks['open'], PTR).value
        if self.lib.sqlite3_vfs_register(C.byref(self.vfs), 0) != OK:
            raise RuntimeError('cannot register budget VFS')

    def safe(self, call):
        def invoke(*args):
            try:
                return call(*args)
            except BaseException as error:
                self.errors.append(repr(error))
                return IOERR
        return invoke

    @contextlib.contextmanager
    def exclusive(self):
        with self.lock:
            nested = getattr(self.local, 'depth', 0)
            began = time.perf_counter_ns()
            if not nested:
                fcntl.flock(self.lock_file.fileno(), fcntl.LOCK_EX)
            self.local.depth = nested + 1
            if hasattr(self, 'stats'):
                self.stats['lock_wait_ns'] += time.perf_counter_ns() - began
            try:
                yield
            finally:
                self.local.depth -= 1
                if not nested:
                    fcntl.flock(self.lock_file.fileno(), fcntl.LOCK_UN)

    @contextlib.contextmanager
    def maintenance(self):
        old = getattr(self.local, 'maintenance', False)
        self.local.maintenance = True
        try:
            yield
        finally:
            self.local.maintenance = old

    def snapshot(self):
        sizes = {}
        for path in self.directory.iterdir():
            try:
                if not path.is_file():
                    raise ValueError('experiment supports flat cache directories')
                sizes[path.name] = path.stat().st_size
            except FileNotFoundError:
                pass
        return sizes

    def allocation(self):
        sizes, charged, allocated = {}, {}, 0
        for path in self.directory.iterdir():
            try:
                info = path.stat()
                if not stat.S_ISREG(info.st_mode):
                    raise ValueError('experiment supports flat cache directories')
                blocks = info.st_blocks * 512
                sizes[path.name] = info.st_size
                charged[path.name] = max(info.st_size, blocks)
                allocated += blocks
            except FileNotFoundError:
                pass
        return sizes, charged, allocated

    def grow(self, path, end, call, reserve_checkpoint=False):
        with self.exclusive():
            self.stats['growth_checks'] += 1
            sizes, charged, allocated = self.allocation()
            before = sizes.get(path.name, 0)
            sizes[path.name] = max(before, end)
            rounded = ((end + self.block_bytes - 1) // self.block_bytes) * self.block_bytes
            charged[path.name] = max(charged.get(path.name, 0), rounded)
            total = sum(charged.values())
            wal_bytes = sum(n for name, n in sizes.items() if name.endswith('-wal'))
            normal_wal = reserve_checkpoint and not getattr(self.local, 'maintenance', False)
            if total > self.limit or (normal_wal and total + wal_bytes + self.reserve > self.limit):
                self.stats['denials'] += 1
                return FULL
            result = call()
            actual, charged, allocated = self.allocation()
            total = sum(charged.values())
            self.stats['peak_file_bytes'] = max(self.stats['peak_file_bytes'], sum(actual.values()))
            self.stats['peak_allocated_bytes'] = max(self.stats['peak_allocated_bytes'], allocated)
            self.stats['peak_billable_bytes'] = max(self.stats['peak_billable_bytes'], total)
            self.stats['peak_wal_bytes'] = max(
                self.stats['peak_wal_bytes'],
                sum(n for name, n in actual.items() if name.endswith('-wal')),
            )
            if total > self.limit:
                raise AssertionError(('file limit exceeded', total, self.limit))
            return result

    def open_file(self, vfs, name, file, flags, out_flags):
        if name is None:
            return CANTOPEN
        path = Path(os.fsdecode(C.cast(name, C.c_char_p).value)).resolve()
        if path.parent != self.directory:
            return CANTOPEN
        result = OPEN(self.parent.contents.open)(self.parent, name, file, flags, out_flags)
        if result != OK:
            return result
        parent = Methods.from_address(File.from_address(file).methods)
        if parent.version != 3:
            CLOSE(parent.close)(file)
            return CANTOPEN
        methods = Methods.from_buffer_copy(parent)
        entry = {'path': path, 'parent': Methods.from_buffer_copy(parent), 'methods': methods}
        self.files[file] = entry
        for key in ('close', 'write', 'truncate', 'control', 'shm_map'):
            setattr(methods, key, C.cast(self.callbacks[key], PTR).value)
        File.from_address(file).methods = C.addressof(methods)
        return OK

    def close_file(self, file):
        entry = self.files.pop(file)
        File.from_address(file).methods = C.addressof(entry['parent'])
        return CLOSE(entry['parent'].close)(file)

    def write(self, file, data, size, offset):
        self.stats['writes'] += 1
        entry = self.files[file]
        end = offset + size
        old = I64()
        result = SIZE(entry['parent'].size)(file, C.byref(old))
        if result != OK:
            return result
        call = lambda: WRITE(entry['parent'].write)(file, data, size, offset)
        wal = entry['path'].name.endswith('-wal')
        if end <= old.value and not wal:
            return call()
        return self.grow(entry['path'], end, call, reserve_checkpoint=wal)

    def truncate(self, file, size):
        entry = self.files[file]
        old = I64()
        result = SIZE(entry['parent'].size)(file, C.byref(old))
        if result != OK:
            return result
        call = lambda: TRUNCATE(entry['parent'].truncate)(file, size)
        if size <= old.value:
            return call()
        return self.grow(entry['path'], size, call, reserve_checkpoint=entry['path'].name.endswith('-wal'))

    def control(self, file, operation, argument):
        if operation in (SIZE_HINT, CHUNK_SIZE):
            self.stats['hints_ignored'] += 1
            return OK
        return CONTROL(self.files[file]['parent'].control)(file, operation, argument)

    def shm_map(self, file, region, size, extend, output):
        entry = self.files[file]
        call = lambda: SHM_MAP(entry['parent'].shm_map)(file, region, size, extend, output)
        if not extend:
            return call()
        return self.grow(Path(str(entry['path']) + '-shm'), (region + 1) * size, call, reserve_checkpoint=True)

    def connect(self, filename):
        db = sqlite3.connect(f'{(self.directory / filename).as_uri()}?vfs={self.name}',
                             uri=True, timeout=0)
        db.execute('PRAGMA temp_store=MEMORY')
        return db

    def close(self):
        if self.files:
            raise RuntimeError('connections must close before unregistering the VFS')
        if self.lib.sqlite3_vfs_unregister(C.byref(self.vfs)) != OK:
            raise RuntimeError('cannot unregister VFS')
        self.lock_file.close()
        if self.errors:
            raise AssertionError(self.errors)
