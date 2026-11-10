# Gameplay and UI Equations

## Synthesized world audio

Each world gets a 9.5-second stereo loop generated from a ten-second source at 44.1 kHz. The basic
tone is a small sum of sine partials, with world-specific fundamentals and
weights:

```text
s(t) = sum(h[k] * sin(2*pi*f[k]*t))
```

A slow LFO shapes pulse or tremolo. Filtered wind/ocean textures use white
noise and a one-pole low-pass, with a moving coefficient `a`:

```text
y[n] = y[n-1] + a * (x[n] - y[n-1])
```

The last 0.5 seconds are mixed into the first 0.5 seconds with a linear equal
fade, then the loop begins at the first retained frame. This crossfade softens
the noise and modulation seam. Camera distance uses a smoothstep fade:

```text
u = clamp((cameraDistance - dNear) / (dFar - dNear), 0, 1)
distanceGain = 1 - u*u*(3 - 2*u)
worldGain = baseGain * distanceGain * bodyOpacity
```

The master volume and per-world gains move toward their targets over roughly
one second. The audio callback uses miniaudio's sound-volume API; it does not
read application state or call OpenGL. `K` toggles mute and the Mission Control
slider changes master volume. Missing optional recordings use the synthesized
loop. Runtime recording files must have a compatible licence and report
attribution.

## Milky Way overview yaw

The normal orbit camera uses azimuth `yaw` and pitch `pitch`:

```text
offset = (d*cos(pitch)*sin(yaw), d*sin(pitch), d*cos(pitch)*cos(yaw))
milkyWayOffset = (d*cos(pitch)*sin(yaw + kMilkyWayCameraYawDeg),
                  d*sin(pitch),
                  d*cos(pitch)*cos(yaw + kMilkyWayCameraYawDeg))
```

With the camera looking toward the target, its horizontal left direction is
`(-cos(yaw), 0, sin(yaw))`. Therefore `kMilkyWayCameraYawDeg = -90.0f`
rotates the camera's horizontal offset onto that left direction while keeping
target, pitch, and distance. The existing two-second smoothstep blend applies
the yaw on entry and removes it on return.

## Plain-language explanation

Each planet's sound is built from quiet tones and filtered generated noise, so
the project ships without audio files. A short crossfade closes each loop;
miniaudio plays it while the selected or nearby body is audible, and the
volume follows distance, mute, and the black-hole fade. In Milky Way mode the
camera keeps looking at the same target from the same height and distance, but
slides around to the left over two seconds. Turning the mode off eases it back.

## Planet position

The rendered orbit is circular, with the real orbital period compressed for readable motion:

```text
periodSeconds = ORBIT_BASE_SECONDS * (orbitalPeriodDays / 365.256)^ORBIT_COMPRESSION_EXPONENT
angle = simTime * 2*pi / periodSeconds
position = (orbitRadius*cos(angle), 0, orbitRadius*sin(angle))
```

`simTime` is accumulated from clamped frame time and the selected speed. It is never multiplied into an already accumulated angle.

## Ray-sphere picking

The mouse position is converted to normalized device coordinates and unprojected with the inverse view-projection matrix to form a world-space ray `r(t) = o + t*d`, where `d` is normalized. For a sphere centered at `c` with radius `R`, define `oc = o - c` and solve:

```text
t^2 + 2*dot(oc,d)*t + dot(oc,oc) - R^2 = 0
b = dot(oc,d)
discriminant = b^2 - (dot(oc,oc) - R^2)
tNear = -b - sqrt(discriminant)
```

A negative discriminant misses. The smallest positive `tNear` among the Sun and planets selects the nearest visible sphere.

## Camera transition and follow

For normalized transition time `s = clamp(elapsed / 1.5, 0, 1)`, smoothstep gives `h = s*s*(3 - 2*s)`. Camera position and target interpolate independently:

```text
camera = mix(startCamera, destinationCamera, h)
target = mix(startTarget, selectedBodyPosition, h)
```

After arrival, the camera keeps its offset from the selected body: each frame both camera and target are translated by the body's position change. Returning to overview uses the same eased interpolation.

## Quiz flow

```text
Select world -> shuffle that world's questions -> take first 5
                                      |
                                      v
                             Show one question
                                      |
                            choose one of 4 options
                                      |
                         show answer + update score
                                      |
                    next question, or final score screen
                                      |
                                   Close
```

Opening the quiz pauses simulation-time accumulation. Closing restores the pause state from before the quiz opened.

## Mission state machine

```text
                    Start Mission
              +------------------------+
              |                        v
         [StartScreen]             [Mission] <--- R / Restart
              |                   /   |   \
      Free Explore              win  loss  select / quiz
              v                  v     v
        [FreeExplore]          [Won] [Lost]
                                  \    /
                                   Restart
                                      |
                                      +------> [Mission]
```

`StartScreen` waits for a choice. `Free Explore` opens the normal scene without updating the fallback threat or mission score. `Mission` advances the threat on wall-clock frame time, even while simulation time is paused for a quiz. A first visit to each body is worth 5 points. A correct quiz answer is worth 10 and multiplies threat speed by 0.8 (with a 0.25 minimum); a wrong answer multiplies it by 1.25 (with a 4.0 maximum). The threat begins at one progress unit per 120 seconds at multiplier 1.0, and progress is clamped to 1.0. Reaching 60 points enters `Won`; reaching threat progress 1.0 enters `Lost`. Both result states stop mission updates and offer a restart. Restart resets score, visited bodies, threat progress and speed, simulation time, selection, camera, quiz, light angle, and HUD toggle states without restarting the process.

The fallback threat equations are:

```text
threatSpeedMultiplier = clamp(threatSpeedMultiplier, 0.25, 4.0)
threatProgress = min(threatProgress + deltaTime * threatSpeedMultiplier / 120, 1)
sunSwallowed = (threatProgress >= 1)
```

The state flow in plain language is: choose a mission or free exploration at the start. In a mission, visiting new worlds and answering questions raises the score while the threat bar advances. Correct answers slow its progress and wrong answers speed it up. Reaching the target score wins; filling the threat bar loses. Either result can be restarted into a fresh mission.

## Gameplay and HUD controls

The HUD displays the shared team names, project title, frame rate, simulation speed and pause state, mission status and score, Bloom/Lensing/Instancing states, quality and black-hole state, light angle, and mission threat progress. `G`, `L`, and `I` toggle their rendering features. `Q` switches between the full and reduced sphere meshes. `B` summons or returns the black hole during gameplay.

`F1` or `H` toggles the key overlay. The listed controls are `Space`, `1`/`2`/`3`, `G`, `L`, `I`, `Q`, `B`, `R`, `F`, `[`/`]`, `Esc`, `F12`, left click, right mouse drag, wheel, `WASD`, and `Shift`. `F12` reads the rendered RGBA framebuffer and writes a vertically flipped PNG to `screenshots/`.
# Interface notes

The ImGui interface uses `UiTheme` for its near-black background, white/secondary/tertiary text, 14%-white hairlines, single amber accent, alert red, spacing, and font setup. It uses square corners and data-first rows instead of decorative cards. `kUiTextScale = 1.2` is combined with GLFW content DPI when fonts are rasterized; `FontGlobalScale` stays at 1.0. Static Inter Regular, Medium, and SemiBold files are discovered by filename for interface text. Static JetBrains Mono Regular and Medium files supply measurements and readouts. Variable fonts are rejected because the TrueType loader cannot select their axis values. The font atlas is rebuilt when content DPI changes; pixel snapping and 2x1 oversampling keep small labels crisp without excess vertical smoothing. Missing fonts use size-matched built-in fonts and a console warning. Both families use the SIL Open Font License; see `assets/fonts/README.txt` for expected names and attribution. `UiPanels` supplies centered hairline cards and a two-column key/action help table.

## Offline Earth map

Earth picking transforms the ray-sphere hit by the inverse of Earth's current model matrix (spin, black-hole basis and scale), then computes `latitude = asin(local.y / r)` and `longitude = atan2(local.z, local.x)`. Latitude is shown north-positive and longitude east-positive in the console and selected-Earth card. The UV convention follows the sphere mesh and Earth vertex shader: `u = fract(longitude / 2π)` and top-origin map `v = 0.5 - latitude / π`. Astana's reference marker uses 51.18°N, 71.45°E and therefore u≈0.1985, v≈0.2157.

City pins use `positionLocal = R * (cos(lat) cos(lon), sin(lat), cos(lat) sin(lon))`, then the same Earth model matrix used for the globe. Front-side filtering keeps far-side city pins and labels hidden; the pin points are depth-tested, additive and depth-write disabled. Labels are population-sorted, distance limited, and reduced in Low quality.

When `earth_day.jpg` is absent, a 1024×512 equirectangular map is generated once at startup using the Earth shader's four-octave value-noise land/ocean field and palette. Map UVs use the same longitude seam and north-at-top convention as the shader; dragging pans, wheel zooms, and clicking sets a smoothly followed local surface target. **This is an offline map, not live tiles.**

The right panel width is 26% of the logical window, clamped to 300–460 pixels and capped at 40% of the window; `kPanelOnRight` selects the dock side. The pinned status strip stays visible above the scrollable ImGui content while scene rendering and mouse-picking's existing `WantCaptureMouse` guard stay unchanged. Planet cards preserve the existing selection/fly-to call, and state chips update the same rendering flags as their keyboard shortcuts. This interface is a presentation-only change: it does not alter mission, quiz, physics, or black-hole rules.

Selected-world details use the same fact strings as `assets/planets.json`. The category adds the order from the Sun and upper-cases the stored type. Stats rows show fields available in the data; values and source JSON are not modified. For a distance stored in kilometres, the conversions are:

```text
AU = distance_km / 149.6e6
light_minutes = distance_km / (299792.458 km/s * 60 s/min)
```

The JSON distance is stored in millions of kilometres, so the implementation multiplies by 1,000,000 before applying the light-time formula and divides by 149.6 to obtain AU. Team names live in one constant array and are formatted for both the splash credit and the HUD.

The panel is arranged as an overview table or selected-world datasheet, followed by simulation, instruments, mission, and controls sections. Labels are compact, values align in monospaced type with units, and hairlines separate groups. Amber marks active controls; red signals only danger or an incorrect quiz answer. Corner readouts use draw lists and do not capture mouse input.

## Startup intro timeline

The startup splash draws over the regular scene after the HDR, bloom, and tone-mapping composite. It runs once per process; it does not alter mission state or block event polling. The timeline uses real frame time clamped to 0.05 seconds. Every transition uses smootherstep:

```text
s(t) = 6t^5 - 15t^4 + 10t^3, where t is clamped to [0, 1]
```

From 0.0 to 0.8 seconds the dark overlay falls from 1.0 to 0.55. Between 0.8 and 2.2 seconds the title appears and scales from 0.94 to 1.0. The yellow line grows from 1.4 to 2.4 seconds; the credit fades in from 2.0 to 3.0 seconds. The scene holds until 4.4 seconds, then all overlay content fades away by 5.2 seconds. Any key or mouse click starts a 0.4-second fade from the current visual state; Escape is included as an ordinary key. `--no-intro` disables the splash for that launch. The GLFW window remains hidden until its first complete frame is swapped, avoiding an uninitialized clear-color flash.

In plain language, the title and credit are drawn on top of the actual animated Solar System, rather than on a video or still image. Smooth transitions prevent abrupt changes, and the same team-name constant keeps the splash and HUD in sync.

## Quiz randomization

Questions remain grouped by world from `assets/quiz.json`. A Fisher–Yates shuffle fills a per-world bag; each question is drawn once before the bag refills, and refill swaps the first entry away from the last drawn question to prevent an immediate repeat across the boundary. A new quiz shuffles the remaining bag again, while mission start/restart builds fresh bag orders that are forced not to match the previous full order. Each displayed question independently Fisher–Yates shuffles its answer strings; correctness is checked by comparing the selected string to the stored correct answer, so it does not depend on the original option index. The shuffle is skipped if any option contains “all of the above” or begins with “both ” and contains “ and ”; no current quiz option matches either rule. `kQuizFixedSeed = 0` selects `std::random_device` with a clock fallback; nonzero values produce a reproducible sequence. The seed is printed at mission and quiz start.
