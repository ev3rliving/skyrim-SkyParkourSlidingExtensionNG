"""Read-only verification of the staged SKSE DLL, installation ZIP and supplied HKX timing.

Requires Python and pefile; uses the neighboring project's vendored pefile if present.
"""
import hashlib
import struct
import sys
import zipfile
from pathlib import Path

root = Path(__file__).resolve().parent
sys.path.insert(0, str(root.parent / 'Player Physics SE/tools/python'))
import pefile

dll = root / 'Data/SKSE/Plugins/MomentumSlideSE.dll'
pe = pefile.PE(str(dll))
assert pe.FILE_HEADER.Machine == 0x8664
exports = {s.name: s.address for s in pe.DIRECTORY_ENTRY_EXPORT.symbols}
assert b'SKSEPlugin_Load' in exports and b'SKSEPlugin_Version' in exports
metadata = pe.get_data(exports[b'SKSEPlugin_Version'], 0x350)
assert metadata[8:].split(b'\0')[0] == b'MomentumSlideSE'
assert struct.unpack_from('<I', metadata, 4)[0] == ((3 << 16) | (1 << 4))
assert struct.unpack_from('<16I', metadata, 0x30C) == ((1 << 24) | (6 << 16) | (1170 << 4),) + (0,) * 15
assert metadata[0x308] & 5 == 5
with zipfile.ZipFile(root / 'MomentumSlideSE-0.3.1-test.zip') as archive:
    names = {n.replace('\\', '/'): n for n in archive.namelist()}
    assert set(n for n in names if n.endswith('.dll')) == {'SKSE/Plugins/MomentumSlideSE.dll'}
    assert all(n.startswith(('SKSE/', 'Docs/')) for n in names)
    assert not any(n.endswith(('.hkx', '.esp', '.esl')) for n in names)
    assert archive.read(names['SKSE/Plugins/MomentumSlideSE.dll']) == dll.read_bytes()
    assert archive.read(names['SKSE/Plugins/MomentumSlideSE.ini']) == (root / 'config/MomentumSlideSE.ini').read_bytes()
print('PASS: x64, SKSE exports, runtime 1.6.1170, archive payload and isolation')
print('DLL SHA256:', hashlib.sha256(dll.read_bytes()).hexdigest())
print('Imports:', ', '.join(e.dll.decode() for e in pe.DIRECTORY_ENTRY_IMPORT))

# These hashes were recorded before implementation. Preserve both existing mods.
for rel, expected in {
    'Player Physics Ground SE/Data/SKSE/Plugins/PlayerPhysicsGroundSE.dll': 'f8d8fcd6b0215847499edcf6a5a537caadc4c7ba77dceceb89baae3746a3e5a3',
    'Player Physics SE/Data/SKSE/Plugins/PlayerPhysicsSE.dll': 'd503f83f7f6b0666db35b8691251b91f44b94352d227348df079105c89fef821',
}.items():
    assert hashlib.sha256((root.parent / rel).read_bytes()).hexdigest() == expected
print('PASS: existing Ground SE and jump DLLs unchanged')

asset_root = root.parent / 'SkyParkour v3 - Procedural Parkour and Climbing Framework (SPPF)/meshes/actors/character'
for pov in ('', '_1stperson/'):
    path = asset_root / (pov + 'animations/SkyParkour_Slide/SprintSlide.hkx')
    data = path.read_bytes()
    assert data[:8] == bytes.fromhex('57e0e05710c0c010')
    # Havok 2010 x64 packfile section table: local fixups map pointer slots to strings.
    base, local, global_ = struct.unpack_from('<3I', data, 180)
    times = {}
    for i in range(local, global_, 8):
        source, target = struct.unpack_from('<2I', data, base + i)
        if source == 0xffffffff or target == 0xffffffff:
            continue
        text = data[base+target:].split(b'\0', 1)[0]
        if text in (b'SkyParkour_Recovery', b'SkyParkour_SlideStop.Slide'):
            times[text.decode()] = struct.unpack_from('<f', data, base + source - 8)[0]
    assert len(times) == 2 and all(t > 0.70 for t in times.values())
    print('PASS: slope-adjusted middle precedes recovery/stop:', pov or 'third person', times)
