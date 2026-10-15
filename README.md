# Momentum Slide SE — 0.3.1 test build

Separate SKSE plugin for Steam Skyrim SE **1.6.1170** and the supplied **SkyParkour 3.6.5**. Requires SKSE64 2.2.6, AE Address Library, SkyParkour and its normal requirements/generated first- and third-person behaviors, and the Microsoft Visual C++ x64 runtime.

## Updating from earlier versions

Replace the old Momentum Slide SE mod with `MomentumSlideSE-0.3.1-test.zip`, including its INI. Keep only one MomentumSlideSE.dll enabled. Keep SkyParkour's Crouch Slide enabled and retain its generated behavior output. No Nemesis/Pandora regeneration is needed specifically for this add-on. Ground SE and Player Physics SE remain separate and unchanged; neither is required.

## 0.3.1 robustness fixes

Terrain calculations now use a single validated normal snapshot and reject invalid movement output. Losing contact between checks cannot cause division by zero. Stock flat timing and slope/recovery tuning are unchanged from 0.3.0.

If a forced stop occurs while the animation graph is unavailable, cleanup remains pending across local resets and save loading. It is retried on player updates when the graph returns. No stop notification is sent ahead of the cleanup event, and new slides are rejected until delivery completes. This lets SkyParkour release its own controls and yaw lock through its normal event handler. If the graph never returns, cleanup cannot be delivered; the plugin does not forcibly unlock controls belonging to other systems.

## Revised behavior

- **Flat ground:** SkyParkour's original animation timestep and movement are passed through unchanged. There is no held pose or independent friction timer. The supplied clip's stop annotation is at 0.85 animation seconds; SkyParkour's playback speed still applies.
- **Downhill:** slows the middle of the slide, extending its duration. At a downhill angle of 18 degrees along the direction of travel, that portion can remain held while the slope continues. Momentum receives downhill gravity, friction and a speed cap.
- **Uphill:** advances the middle faster to shorten the slide, with slope-aware velocity during that portion.
- **Reaching level ground:** resumes normal animation time and blends slope momentum into the original recovery, rather than immediately issuing a stop.
- **Cancel:** release the starting crouch press and press crouch again. The held pose is released and the remaining animation plays smoothly through its recovery and natural stop. Controls return through SkyParkour's normal exit. The mapped Sneak action supports keyboard and gamepad.

Only animation seconds 0.40–0.70 are retimed. Entry, recovery and exit retain normal playback. Slopes encountered after that middle portion cannot restart a slide that is already exiting. Small inclines within 3 degrees count as flat, and terrain measurements are smoothed to reduce jitter. The direction of travel, not camera pitch, determines uphill versus downhill.

## Abrupt-exit changes

The 0.2.0 test log identified 35 contact timeouts. Nineteen still had both native on-ground state and the support flag despite `surfaceInfo` reporting unsupported. This revision accepts the native support flag instead of letting `surfaceInfo` veto it. When native signals are inconclusive, a short vertical ray from 12 units above to 24 below the feet checks for nearby static floor. Probe evidence expires after 100 ms and never overrides an airborne state. It accepts static/terrain/ground/stair-helper collision layers, not actors, water or triggers. No teleport, ground snap or forced grounded state is used.

The 0.20-second contact grace remains, but expiration now starts recovery instead of synthesizing an immediate stop event. Crouch cancellation and sustained stalls also play the remaining animation. Recovery is latched: downhill detection cannot freeze it again. No extra sliding force is applied while airborne. If recovery makes no animation progress for one second, the fail-safe releases it. Water, mounting, death, ragdoll, missing graphs, controller replacement and incompatible actor/camera states still permit immediate cleanup.

## Settings and diagnostics

Edit `SKSE/Plugins/MomentumSlideSE.ini` and restart:

| Setting | Default | Effect |
|---|---:|---|
| FlatAngle | 3 | Inclines within this angle retain stock timing |
| SustainAngle | 18 | Downhill angle along travel that can sustain the middle pose |
| ContactGrace | 0.20 | Seconds of lost ground contact tolerated |
| Friction | 2 | Resistance during slope-adjusted movement, m/s² |
| Drag | 0.12 | Speed-dependent slope-slide resistance, 1/s |
| SlopeGravity | 10 | Downhill gravity scale, m/s² |
| MaxSpeed | 18 | Slope-slide speed cap, m/s |
| MinEntrySpeed | 2 | Minimum speed for this add-on to take over a slide |
| DebugLogging | 1 | Start, native exit, contact timeout and interruption logs |

Old `HoldPoseTime` and `StopSpeed` settings are no longer used. Flat-ground duration is determined by SkyParkour, not an INI timer.

The log is `Documents/My Games/Skyrim Special Edition/SKSE/MomentumSlideSE.log`. Check that its first line reports **0.3.1**. A normal cancellation should log `Recovery requested`, followed by `Native slide exit`. The log also records when the native support flag or nearby-floor probe resolves conflicting contact signals. Contact timeouts include state, support status, flags and normal; exits include elapsed and clip time.

## Verification and remaining test

Version 0.3.1 has standalone regression tests for the observed support/surfaceInfo disagreement, fresh probe evidence, airborne/ragdoll rejection, latched downhill recovery, stock flat timing, slope durations, sustained slopes, contact grace and speed limits. **This revision has not been tested in Skyrim.** The floor probe and visual recovery need another game test with the user's mod setup.

Compare a flat-ground slide with this mod disabled/enabled, then try uphill, a continuous downhill, downhill-to-flat, second-crouch cancellation, small terrain bumps, a wall and a ledge. Test first/third person and confirm normal controls return. If abrupt exits remain, the new log is needed to distinguish their causes.

## Independence and build

Ground SE has no public terrain API; this DLL reads the same underlying Skyrim controller support normal and surface velocity directly. Its ordinary uphill resistance and steep-slope slipping remain separate. Canceling a crouch slide does not disable Ground SE's ordinary slipping.

Build with `./Build.ps1` in an x64 MSVC developer shell using the neighboring CommonLibSSE-NG package, or pass `-PortableToolsRoot`. The script builds, runs CTest and packages the install ZIP. `verify_build.py` uses Python/pefile to check the DLL, package, original mod hashes and supplied animation event timing.

No SkyParkour DLL, ESP, HKX, behavior or source is redistributed. Reference checkouts and the user's diagnostic log are excluded from both archives. Credits: Tsptds and JellyFishInLoop for SkyParkour; Altimor for Player Physics inspiration; SKSE/CommonLibSSE-NG contributors. Third-party library licenses are included under `licenses/`.
