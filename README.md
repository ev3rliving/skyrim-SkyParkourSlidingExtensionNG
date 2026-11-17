# SkyParkour Sliding Extension — 1.2.0

One SKSE DLL for Steam Skyrim **1.5.97, 1.6.1170, 1.7.99/1.7.104** and **GOG 1.6.1179**. Based on version **1.1**, preserving its slide physics and normal/Advanced Sneak Entry fixes.

[Download on Nexus Mods](https://www.nexusmods.com/skyrimspecialedition/mods/195488).

## Version 1.2.0

- One universal package instead of separate runtime-specific DLLs.
- Legacy SKSE Query support, modern Address Library support, and runtime-aware actor/animation accessors.
- Startup hook/interface validation and clearer runtime logging.
- All three regression tests and compiled-DLL compatibility checks passed.
- Tested in game on **1.6.1170**: six slides exited normally, all hooks validated, and no extension warnings/errors appeared. Other targeted runtimes await in-game verification. Unknown future versions and VR are rejected.

## Installation

Install `SkyParkourSlidingExtension-1.2.0.zip` and keep only one `MomentumSlideSE.dll` enabled. Existing INI settings can be retained.

Requires matching SKSE and Address Library, a compatible SkyParkour installation (developed against 3.6.5) with its normal requirements/generated behaviors, Crouch Slide enabled, and Microsoft Visual C++ v14 x64 runtime. No additional behavior generation is needed specifically for this extension. Ground SE and Player Physics SE are optional.

See [COMPATIBILITY.md](COMPATIBILITY.md) for runtime handling and [VERIFICATION.md](VERIFICATION.md) for testing evidence and remaining checks.

## Build

Dependency sources are included under `dependencies/`. In an x64 MSVC 2022 developer shell with CMake 3.24+ and Ninja, run `./Build.ps1`. It builds one DLL, runs CTest, and creates `SkyParkourSlidingExtension-1.2.0.zip`.

With Windows Python and `pefile` installed, run `python verify_compatibility.py` to inspect the DLL and exercise its actual Query export. Run `python package_compatibility.py` to create the install and complete-source archives in `release/1.2.0/`. Supported-runtime Load is never invoked outside Skyrim by these tests.

## Credits and license

Tsptds and JellyFishInLoop for SkyParkour; Altimor for Player Physics inspiration; SKSE/CommonLibSSE-NG contributors. No SkyParkour DLL, ESP or animation files are redistributed.

GPL-3.0-or-later with the additional permissions in [EXCEPTIONS.md](EXCEPTIONS.md). See [LICENSE](LICENSE) and retain the dependency notices under `licenses/` and `dependencies/`. Provide corresponding source when distributing DLLs.
