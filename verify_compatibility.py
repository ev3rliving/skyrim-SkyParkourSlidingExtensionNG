"""Inspect a built DLL and exercise its real SKSE Query/runtime rejection exports.

Requires Windows Python and pefile. Does not run or simulate Skyrim.
"""
import argparse
import ctypes as ct
import hashlib
import struct
import sys
from pathlib import Path

root = Path(__file__).resolve().parent
import pefile

versions_supported = [(1,5,97),(1,6,1170),(1,6,1179),(1,7,99),(1,7,104)]

def pack(v):
    return (v[0] << 24) | (v[1] << 16) | (v[2] << 4)

parser = argparse.ArgumentParser()
parser.add_argument('--build-directory', default='build-universal')
args = parser.parse_args()
dll = next((root / args.build_directory).rglob('MomentumSlideSE.dll'))
pe = pefile.PE(str(dll))
assert pe.FILE_HEADER.Machine == 0x8664
exports = {s.name: s.address for s in pe.DIRECTORY_ENTRY_EXPORT.symbols}
assert {b'SKSEPlugin_Query', b'SKSEPlugin_Load', b'SKSEPlugin_Version'} <= exports.keys()
metadata = pe.get_data(exports[b'SKSEPlugin_Version'], 0x350)
assert struct.unpack_from('<I', metadata, 4)[0] == pack((1, 2, 0))
expected = [pack(v) for v in versions_supported]
assert list(struct.unpack_from('<16I', metadata, 0x30C)) == expected + [0] * (16-len(expected))
flags = struct.unpack_from('<I', metadata, 0x308)[0]
assert flags & 1
assert not (flags & 4) # No single post-629 structure layout imposed
assert struct.unpack_from('<I', metadata, 0x304)[0] & 3 == 3 # dynamic layouts and v5

class Interface(ct.Structure):
    _fields_ = [('skse', ct.c_uint32), ('runtime', ct.c_uint32),
                ('editorVersion', ct.c_uint32), ('isEditor', ct.c_uint32),
                ('query', ct.c_void_p), ('handle', ct.c_void_p),
                ('release', ct.c_void_p), ('pluginInfo', ct.c_void_p)]
class Info(ct.Structure):
    _fields_ = [('infoVersion', ct.c_uint32), ('name', ct.c_char_p), ('version', ct.c_uint32)]

loaded = ct.CDLL(str(dll))
query = loaded.SKSEPlugin_Query
query.argtypes = [ct.POINTER(Interface), ct.POINTER(Info)]
query.restype = ct.c_bool
load = loaded.SKSEPlugin_Load
load.argtypes = [ct.POINTER(Interface)]
load.restype = ct.c_bool
versions = [(1,5,97), (1,6,1170), (1,6,1179), (1,7,99), (1,7,104), (1,7,105), (1,4,15), (1,6,640)]
for v in versions:
    interface, info = Interface(0, pack(v), 0, 0), Info()
    supported = pack(v) in expected
    assert query(ct.byref(interface), ct.byref(info)) == supported, v
    assert info.infoVersion == 1 and info.name == b'MomentumSlideSE'
    assert info.version == pack((1,2,0))
    if not supported:
        assert not load(ct.byref(interface)), v
    interface.isEditor = 1
    assert not query(ct.byref(interface), ct.byref(info))
    assert not load(ct.byref(interface))
assert not query(None, ct.byref(Info()))
assert not query(ct.byref(Interface()), None)
print('PASS:', 'universal DLL', 'x64, metadata, real Query accepts intended versions, rejects others/editor; Load rejects unsupported runtimes.')
print('SHA256:', hashlib.sha256(dll.read_bytes()).hexdigest())
print('Imports:', ', '.join(e.dll.decode() for e in pe.DIRECTORY_ENTRY_IMPORT))

# Protect the 1.1 fix: neither slide-to-sneak event may be rejected by the add-on.
source=(root/'src/main.cpp').read_text()
assert 'if(active && (event=="SkyParkour_SlideToSneak"' not in source
assert "Forward SkyParkour's normal and advanced slide-to-sneak events unchanged." in source
