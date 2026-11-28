"""Private bounded messages between one roof planner and its owned child."""

import pickle
import struct
import time

MAX_PACKET_BYTES=2*1024*1024
class HipWireError(RuntimeError):pass


def remaining(deadline):
    value=None if deadline is None else deadline-time.monotonic()
    if value is not None and value<=0:raise TimeoutError('Roof request deadline expired')
    return value


def send(connection,value,deadline=None):
    payload=pickle.dumps(value,protocol=5)
    if len(payload)>MAX_PACKET_BYTES:raise ValueError('Roof message capacity exceeded')
    connection.settimeout(remaining(deadline))
    connection.sendall(struct.pack('!I',len(payload))+payload)


def exact(connection,count,deadline):
    data=bytearray()
    while len(data)<count:
        connection.settimeout(remaining(deadline));chunk=connection.recv(min(65536,count-len(data)))
        if not chunk:raise EOFError('Roof worker connection closed')
        data.extend(chunk)
    return data


def receive(connection,deadline=None):
    count=struct.unpack('!I',exact(connection,4,deadline))[0]
    if not 0<count<=MAX_PACKET_BYTES:raise HipWireError('Invalid roof message length')
    try:return pickle.loads(exact(connection,count,deadline))
    except (pickle.UnpicklingError,ValueError,TypeError,EOFError) as error:raise HipWireError('Invalid roof message payload') from error
