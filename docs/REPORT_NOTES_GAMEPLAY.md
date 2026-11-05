# Gameplay and UI Equations

## Orbit-line depth handling

The old red screenshot showed the near side of Earth's circular solar orbit crossing the planet's projected disk. That part of the orbit is in front of Earth in camera space, so ordinary depth testing correctly lets it pass; it was not a missing depth attachment or a planet that failed to write depth. The diagnostic logs measured the same HDR draw FBO and depth renderbuffer after the opaque draws and before the orbit pass, with depth testing on and depth writes on for the opaque pass. The test's red-pixel count was the same for `GL_LEQUAL` and `GL_ALWAYS`, so that pixel-count probe did not distinguish the depth functions; no conclusive `GL_LESS` pixel result was captured.

To match the requested visual rule that a planet covers its orbit line anywhere the line projects over the disk, guides now draw before opaque bodies with depth writes disabled. The body color then overwrites the guide across the full planet silhouette. The guide shader uses an alpha uniform, and the normal pass uses a thin line with 0.48 alpha. The pass uses the same view and projection matrices as the planet draw. Inspection found no `gl_FragDepth` or `glDepthRange` override in either shader path.

Frame depth and framebuffer sequence: `PostProcess::beginScene` binds/clears the HDR scene FBO and enables depth testing; `Application::drawSky` uses `GL_LEQUAL` and depth writes off, then restores `GL_LESS` and writes on; the orbit pass uses `GL_LEQUAL` and depth writes off, then restores its prior values; Earth cloud and atmosphere and Saturn ring each disable depth writes only around their own draw and restore the saved mask; `BlackHole::draw` restores its saved depth mask; the selected ring saves and restores depth state; `PostProcess::endScene` restores the prior draw/read FBO; `BlackHole::renderLensing` binds its own lensing FBO, disables depth testing and writes for the fullscreen pass, then restores the prior framebuffer and depth state; `PostProcess::composite` binds bloom ping-pong targets as needed and restores the prior framebuffer, depth test, and write mask. No depth clear occurs between opaque bodies and orbit guides. Source locations: `src/PostProcess.cpp` lines 160-174, `src/Application.cpp` lines 657-674 and 694-730, `src/BlackHole.cpp` lines 296-306 and 351-384, and `src/PostProcess.cpp` lines 207-264.

## Selection ring placement

The previous ring model added `radius * 1.15` to world-space Y, which lifted the entire ecliptic ring above the planet (visible in the previous capture). The ring now translates to `bodyEffect(selected).position`, the same post-black-hole position used by the body model, and its radius is `1.35 * bodyRadius * effect.perpendicularScale`. Its XZ plane preserves the existing ecliptic orientation. Its alpha follows the body's swallow fade, while depth testing hides the half behind the sphere.

In plain language: I moved orbit guides underneath the planet draw so the planet itself covers any line over its disk. I removed the selection ring's extra upward offset and sized it from the planet's current rendered scale. I removed the temporary red test, forced Earth camera, debug prints, and automatic captures. The app built cleanly, but a final interactive view could not be captured because Windows did not expose the launched app as a targetable window.

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

The HUD displays the placeholder team name, project title, frame rate, simulation speed and pause state, mission status and score, Bloom/Lensing/Instancing states, quality and black-hole placeholder states, light angle, and mission threat progress. `G`, `L`, and `I` update and log their state; they do not connect effects that belong to teammate modules. `Q` switches between the full and reduced sphere meshes. `B` changes the black-hole HUD state only.

`F1` or `H` toggles the key overlay. The listed controls are `Space`, `1`/`2`/`3`, `G`, `L`, `I`, `Q`, `B`, `R`, `F`, `[`/`]`, `Esc`, `F12`, left click, right mouse drag, wheel, `WASD`, and `Shift`. `F12` reads the rendered RGBA framebuffer and writes a vertically flipped PNG to `screenshots/`.
