# Version 1.2.0 universal compatibility build

One install archive, one MomentumSlideSE.dll, one INI. Built from version 1.1;
its slide-to-sneak fix and physics rules are retained.

Targeted executables: Steam 1.5.97, Steam 1.6.1170, GOG 1.6.1179,
Steam 1.7.99 and Steam 1.7.104. Install the exact matching SKSE and Address
Library, plus a SkyParkour build and required behaviors compatible with that game.
VR and unreviewed future versions are rejected. No claim of universal future 1.7 support.

## Implementation

- Every runtime uses CommonLibSSE-NG 13.1.0, revision
  12d61f3c39f7d4ba1b1ab543df2bca0e613dc747, built with SE/AE switching and no VR.
- Both legacy SKSEPlugin_Query and AE metadata are exported. Query and Load share
  an exact runtime allowlist. Metadata enables dynamic structure layouts and v5
  Address Library support, rather than imposing a post-1.6.629-only layout.
- Actor state and animation graph runtime data use CommonLib's versioned accessors.
  The modern CFilter type preserves the player's collision group for floor probes.
- Retains the four hook indices: animation notification 1, player update 0xAD,
  clip update 5, on-ground update 8. All four targets must be readable vtable entries
  pointing to executable committed memory before any hook is installed. Existing
  hook predecessors are preserved, including those owned by other plugins.
- At DataLoaded, validate the primary and animation-holder player vtables against
  Address Library. Until successful, the extension cannot start a slide.
- Compile-time assertions cover controller velocity/normal/surfaceInfo and clip time.
  These assertions describe the headers, not proof of an actual game executable's ABI.
- Hook checks do not prove that an executable target has the expected signature.
  Real game tests remain required for that and for Havok behavior.

## Validation and logs

The log is Documents/My Games/Skyrim Special Edition/SKSE/MomentumSlideSE.log.
Look for `1.2.0 universal`, the exact runtime, four hook validation successes,
`Four hooks installed`, and `Player primary and animation interfaces validated`.
If SkyParkour is missing or validation fails, the extension stays inactive.

CTest checks physics, runtime allowlists, invalid hook-target rejection and mocked
actor/graph layout selection. `verify_compatibility.py` (Windows Python + pefile)
checks the compiled DLL exports, version, runtime metadata, real Query acceptance
for all five targets, and Query/Load rejection for editor/unsupported runtimes.
It does not call Load for a supported runtime outside Skyrim.

User testing on Steam 1.6.1170 confirmed six normal slide starts/exits, all four
hooks active and player-interface validation, with no extension warnings/errors,
contact timeouts or forced interruptions. Other runtimes remain unverified in game.
On each target test first/third-person flat slides,
uphill/downhill, terrain transitions, second-crouch recovery, normal and Advanced
Sneak Entry, walls/ledges, save loading and restored controls. Compare flat timing
with version 1.1; inspect logs and any crash reports.

## Build from complete source

Use x64 MSVC 2022, a Windows SDK, CMake 3.24+ and Ninja. From the extracted archive:

```powershell
cmake -S . -B build-universal -G Ninja -DCMAKE_BUILD_TYPE=Release -DSLIDE_DEPENDENCIES="$PWD/dependencies/modern" -DSLIDE_LEGACY_DEPS="$PWD/dependencies/legacy"
cmake --build build-universal --parallel 2
ctest --test-dir build-universal --output-on-failure
cmake --install build-universal --component Slide --prefix Data
python verify_compatibility.py
```

The legacy-named dependency directory contains only shared spdlog/rapidcsv sources;
there is no separate legacy DLL or legacy CommonLib build. The source includes all
dependency notices and GPL license text. No SkyParkour/game binaries are included.
