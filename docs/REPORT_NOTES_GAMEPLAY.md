# Gameplay and UI Equations

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
