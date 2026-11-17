"""Package verified target DLLs and full dependency source snapshots.

No game files, private logs, Git history or toolchain binaries are included.
"""
import argparse
import hashlib
import json
import zipfile
from pathlib import Path

root = Path(__file__).resolve().parent
parser=argparse.ArgumentParser()
parser.add_argument('--build-directory',default='build-universal')
args=parser.parse_args()
out = root / 'release/1.2.0'
out.mkdir(parents=True,exist_ok=True)
docs = ['README.md', 'COMPATIBILITY.md', 'VERIFICATION.md', 'LICENSE', 'EXCEPTIONS.md']
targets = {'universal':'universal'}
hashes = {}
for target, label in targets.items():
    dll = next((root / args.build_directory).rglob('MomentumSlideSE.dll'))
    archive = out / 'SkyParkourSlidingExtension-1.2.0.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        z.write(dll, 'SKSE/Plugins/MomentumSlideSE.dll')
        z.write(root/'config/MomentumSlideSE.ini', 'SKSE/Plugins/MomentumSlideSE.ini')
        for name in docs:
            z.write(root/name, 'Docs/MomentumSlideSE/'+name)
        for p in sorted((root/'licenses').iterdir()):
            z.write(p, 'Docs/MomentumSlideSE/licenses/'+p.name)
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        assert z.read('SKSE/Plugins/MomentumSlideSE.dll') == dll.read_bytes()
        assert z.read('SKSE/Plugins/MomentumSlideSE.ini') == (root/'config/MomentumSlideSE.ini').read_bytes()
        assert sum(n.endswith('.dll') for n in z.namelist()) == 1
        assert not any(n.endswith(('.esp','.hkx','.esl')) for n in z.namelist())
    hashes[archive.name] = hashlib.sha256(archive.read_bytes()).hexdigest()
    print('PASS package:', archive.name)

source = out/'SkyParkourSlidingExtension-1.2.0-Source.zip'
dependencies = {
    'legacy/spdlog':root/'dependencies/legacy/spdlog',
    'legacy/rapidcsv':root/'dependencies/legacy/rapidcsv',
    'modern/CommonLibSSE-modern':root/'dependencies/modern/CommonLibSSE-modern',
    'modern/DirectXTK':root/'dependencies/modern/DirectXTK',
}
manifest = {}
with zipfile.ZipFile(source, 'w', zipfile.ZIP_DEFLATED) as z:
    def add(p, name):
        data = p.read_bytes()
        z.writestr(name, data)
        manifest[name] = hashlib.sha256(data).hexdigest()
    for name in docs + ['CMakeLists.txt','Build.ps1','BuildCompatibility.ps1',
                         'verify_compatibility.py','package_compatibility.py','.gitignore']:
        add(root/name, name)
    for folder in ['src','tests','config','cmake','licenses']:
        for p in sorted((root/folder).rglob('*')):
            if p.is_file(): add(p, p.relative_to(root).as_posix())
    for name, folder in dependencies.items():
        for p in sorted(folder.rglob('*')):
            rel = p.relative_to(folder)
            if not p.is_file() or any(x in {'.git','__pycache__'} for x in rel.parts) or ('build' in rel.parts and name != 'modern/DirectXTK'): continue
            if p.suffix.lower() in {'.dll','.exe','.pdb','.obj','.lib','.zip','.pyc'}: continue
            add(p, 'dependencies/'+name+'/'+rel.as_posix())
    z.writestr('SOURCE-SHA256.json', json.dumps(manifest, indent=2)+'\n')
with zipfile.ZipFile(source) as z:
    assert z.testzip() is None
    for name, sha in manifest.items():
        assert hashlib.sha256(z.read(name)).hexdigest() == sha, name
hashes[source.name] = hashlib.sha256(source.read_bytes()).hexdigest()
(out/'SHA256SUMS.txt').write_text(''.join(f'{sha}  {name}\n' for name,sha in hashes.items()))
print('PASS complete source:', len(manifest), 'files;', round(source.stat().st_size/1048576,2), 'MiB')
