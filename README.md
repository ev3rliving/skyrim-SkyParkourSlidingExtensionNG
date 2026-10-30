# SkyParkour Sliding Extension — 1.1

Separate SKSE plugin for Steam Skyrim SE **1.6.1170** and the supplied **SkyParkour 3.6.5**. Requires SKSE64 2.2.6, AE Address Library, SkyParkour and its normal requirements/generated first- and third-person behaviors, and the Microsoft Visual C++ x64 runtime.

Credits: Tsptds and JellyFishInLoop for SkyParkour; Altimor for Player Physics inspiration; SKSE/CommonLibSSE-NG contributors. Third-party library licenses are included under `licenses/`.

## Version 1.1

Restores compatibility with SkyParkour's Sneak Entry and Advanced Sneak Entry settings by allowing its slide-to-sneak events through unchanged. Slope-based slide extension remains active. Advanced Sneak Entry follows SkyParkour's recovery window, which occurs later during an extended downhill slide.

This release targets **Steam 1.6.1170 only**. Build, regression tests and package checks passed; the Sneak Entry changes still need in-game validation. See [VERIFICATION.md](VERIFICATION.md).

[Download the mod on Nexus Mods](https://www.nexusmods.com/skyrimspecialedition/mods/195488).

## Build

Use an x64 MSVC developer shell and run `./Build.ps1`. The build expects a CommonLibSSE-NG 3.6.0 package at `../SkyrimSE/package`, or pass `-CommonLibPackage` with its location. It builds the DLL, runs CTest and creates `SkyParkourSlidingExtension-1.1-Steam-1.6.1170.zip`. See the CMake configuration for required spdlog/CommonLib targets.

## License

Copyright (c) 2026 ev3rliving and contributors. Licensed under GPL-3.0-or-later with the additional permissions in [EXCEPTIONS.md](EXCEPTIONS.md). See [LICENSE](LICENSE). Retain the separate third-party notices under `licenses/`.
