# Black Hole Renderer Notes

## Render pass and draw order

```text
Sky -> orbit/selection lines -> planets/Sun with body effects
                         |
                         v
                 PostProcess HDR FBO
                 RGBA16F + depth buffer
                         |
                         v
         BlackHole lensing output FBO (RGBA16F)
                         |
                         v
 Bright-pass -> half-size blur ping-pong -> HDR composite
                         |
                         v
              ACES + sRGB -> default framebuffer
                         |
                         v
                      ImGui HUD
```

The scene input is the existing HDR texture from `PostProcess`; `BlackHole` owns only the lensing output target. Its API accepts a plain `GLuint` and width/height so the HDR source can be changed at the call site. The lensing output is then passed into the existing final composite, which draws the lensed, bloomed, tone-mapped image to the default framebuffer. No duplicate scene FBO is created.

Within the scene, the order is sky, orbit and selection lines, planets and Sun, the tilted disk, then the opaque horizon. The disk is a flat triangle ring; its transform shares the center and smooth visibility scale of the sphere and rotates 30 degrees. Its draw temporarily enables alpha blending, disables depth writes and back-face culling, and restores those states afterward.

## State machine, mission threat, and visibility

```text
Idle --B/mission start--> Approaching --p reaches 1--> Consuming
  ^                            |                            |
  |                            +-----------B----------------+
  |                                         v
  +---------- p reaches 0 ------------- Returning
```

Mission start resets the real threat and summons it from progress zero. Free Explore resets it to Idle and B summons it. B returns an approaching or consuming hole. R restarts the mission, restoring progress and all body transforms before the automatic new approach. `FallbackThreat` remains as an alternate implementation of `ThreatInterface`.

In Free Explore, R requests the return before the Sun is swallowed and restarts the whole scene after `p=1`. The centered ending panel offers **Restart** or **Keep watching**; keeping watch leaves the scene paused and hides the panel. Mission loss uses its centered result panel with **Restart** and **Back to menu**. Restart resets the hole, eased threat speed, mission/quiz state, selection, camera, pause state, body animation time, asteroid/meteor time, and rendering toggles. Since orbital and belt transforms are recalculated from simulation time, resetting that time restores their original arrangement without stored offsets. Each state transition is printed to the console and the HUD shows the state plus `p` as a percentage.

The progress `p` is the threat interface value. Correct quiz answers lower the target speed multiplier; wrong answers raise it. The current speed eases toward that target, so a quiz answer does not cause a progress jump:

```text
currentSpeed += (targetSpeed - currentSpeed) * (1 - exp(-3*dt))
step = dt * currentSpeed / kSwallowSecondsAtOneX
p(next) = clamp(p + step, 0, 1)        while approaching
p(next) = clamp(p - step, 0, 1)        while returning
targetSpeed = clamp(answerMultiplier, 0.25, 4.0)
```

`kSwallowSecondsAtOneX` is the single tuning constant in `BlackHole.h`, currently `90.0f`; at a steady 1x multiplier the one-way sequence takes about 90 seconds. Returning subtracts the same step from `p`, so it is the time-reversed path and reaches exactly zero without a jump. Quiz scoring is unchanged: visiting all nine bodies yields 45 points, so two correct answers (20 points) reach the 60-point target; quizzes pause simulation, leaving the full 90 seconds of threat time available at 1x. The mission remains winnable. Correct answers slow the eased threat multiplier; wrong answers speed it up.

Smoothstep is `S(x) = x*x*(3 - 2*x)` for `x` in `[0,1]`.

```text
approach = S(p)
holePosition = mix((6, 11, 18), (0, 0, 0), approach)
reveal = S(clamp(p / 0.12, 0, 1))
blackSphereScale = BLACK_HOLE_RADIUS * reveal
diskScale = reveal
diskOpacity = reveal
```

The hole's path eases across the whole approach. A separate short reveal curve makes it visible early in that path; both curves reach exactly zero at `p = 0`, so returning naturally hides the sphere and disk without a pop. Pausing or opening the quiz stops progress and disk animation time.

When `p` reaches 1, the progress interface reports the Sun swallowed and the mission enters its existing lose state. Restart resets the hole to progress zero. The real threat is used by default; setting `useFallbackThreat` to true selects the retained fallback timer.

## Body pull, stretch, and swallow equations

For each frame, the effect is recomputed from the original orbital position and current hole position; it stores no displaced position. `d` is the distance from the orbital position to the hole, and the activation threshold is 0.10 for planets and 0.92 for the Sun:

```text
activation = smoothstep(threshold, 1, p)
pullStrength(p) = 8 * activation
w = clamp(pullStrength(p) / (1 + (d / 8)^2), 0, 1)
displaced = orbitPos + (holePos - orbitPos) * w
dAfterPull = length(holePos - displaced)
```

The inverse-square-style falloff makes a body closer to the hole receive more pull. The Sun's higher activation threshold makes it move and disappear last. The stretch axis is aligned with the direction from the displaced body to the hole. The code's descending smoothstep is written as one minus the increasing form:

```text
nearHole = 1 - smoothstep(Rs, diskOuterRadius, dAfterPull)
s = 1 + 4 * nearHole * activation
scaleAlongHole = s
scalePerpendicular = 1 / sqrt(s)
```

Swallowing fades and shrinks a body as its post-pull distance enters the horizon. The Sun uses a later end-stage fade than planets:

```text
horizonFade = 1 - smoothstep(0.65*Rs, 1.4*Rs, dAfterPull)
endStage = activation                         for planets
endStage = smoothstep(0.985, 1, p)            for the Sun
swallowedAmount = clamp(horizonFade * endStage, 0, 1)
remaining = 1 - swallowedAmount
scaleAlongHole *= remaining
scalePerpendicular *= remaining
opacity = remaining
```

The planet and Sun shaders multiply their linear RGB output by `opacity` (emission fade) while retaining opaque alpha. When remaining is at most 0.001, rendering and picking skip the body. At `p=0`, activation is zero and every body returns to its original position and size.

Asteroids use the matching pull, stretch, and fade in `shaders/asteroid.vert`, with the hole position, `p`, `Rs`, pull radius, maximum pull/stretch, stretch influence radius, and activation threshold supplied by `BlackHole`. For each asteroid center at the current simulation time:

```text
activation = smoothstep(activationThreshold, 1, p)
pullStrength(p) = maximumPullStrength * activation
w = clamp(pullStrength(p) / (1 + (d / pullRadius)^2), 0, 1)
displaced = instancePosition + (holePosition - instancePosition) * w
dAfterPull = length(holePosition - displaced)
nearHole = 1 - smoothstep(Rs, stretchInfluenceRadius, dAfterPull)
stretch = 1 + maximumBodyStretch * nearHole * activation
opacity = 1 - clamp((1 - smoothstep(0.65*Rs, 1.4*Rs, dAfterPull)) * activation, 0, 1)
```

The instance transform is evaluated from the current-time orbit position each frame, so instanced and individual comparison draws share the same effect and returning/restarting cannot accumulate drift. A short heat envelope briefly adds orange-white HDR emission above 1 before the horizon fade removes the asteroid. Meteors apply the same stateless center pull/stretch/fade to their head and streak using their current simulation-time position. Their movement, asteroid movement, and hole state pause and speed together with simulation speed. Existing asteroid-planet repulsion and its impact glow still run after the black-hole transform.

## Accretion disk equations

Let `r` be the ring-space radius and `theta = atan(y, x)`. The shader normalizes radius between its inner and outer mesh edges:

```text
rho = clamp((r - innerRadius) / (outerRadius - innerRadius), 0, 1)
angularSpeed = mix(0.8, 4.0, 1 - rho)
spiralAngle = theta + time * angularSpeed
polarNoisePosition = (cos(spiralAngle)*r*2.5,
                      sin(spiralAngle)*r*2.5,
                      rho*3.0 + time*0.12)
```

The angle is embedded as cosine and sine so the polar field wraps continuously at the seam. `valueNoise3` hashes the eight surrounding integer lattice corners and interpolates with the cubic curve `f*f*(3 - 2*f)`. Four-octave FBM sums progressively finer noise:

```text
FBM(q) = sum(i=0..3, amplitude[i] * valueNoise3(q[i]))
amplitude[0] = 0.5
amplitude[i+1] = amplitude[i] * 0.5
q[i+1] = q[i] * 2.03 + (7.1, 13.7, 5.3)
```

`turbulence = FBM(polarNoisePosition)` and `filaments = mix(0.55, 1.45, turbulence)` modulate the gas brightness.

The radial color gradient blends HDR white-hot gas to orange and dark red:

```text
c1 = mix((2.8, 2.15, 1.25), (1.65, 0.30, 0.03), smoothstep(0, 0.48, rho))
color = mix(c1, (0.25, 0.012, 0.004), smoothstep(0.45, 1, rho))
```

The HDR white/orange values exceed 1.0 so the renderer's bloom threshold can catch them. The tangent to circular flow and view direction set the Doppler factor:

```text
diskVelocityDir = normalize(model * (-sin(theta), cos(theta), 0))
viewDir = normalize(cameraPosition - worldPosition)
dopplerBrightness = 1 + 0.85 * dot(diskVelocityDir, viewDir)
color *= filaments * dopplerBrightness
```

The edge alpha is the product of two smooth fades and the shared reveal:

```text
innerFade = smoothstep(0, 0.10, rho)
outerFade = 1 - smoothstep(0.82, 1, rho)
alpha = reveal * innerFade * outerFade
```

## Lensing shader equations

`lensing.vert` uses `gl_VertexID` to emit one oversized triangle, so no mesh buffer is needed for the fullscreen pass. `lensing.frag` samples the scene texture, projects the hole center, warps sample coordinates, fades the warp near the viewport and its influence radius, and paints the projected horizon dark. The result stays in the lensing module's RGBA16F target until the existing HDR composite sends it to the default framebuffer.

The lens pass projects the world-space hole center using the same view and projection matrices as the scene:

```text
holeClip = projection * view * vec4(holeCenter, 1)
holeNdc = holeClip.xyz / holeClip.w
holeUV = holeNdc.xy * 0.5 + 0.5
delta = uv - holeUV
delta.x *= aspect
r = length(delta)
RsScreen = abs(projection[1][1] * Rs / holeClip.w) * 0.5
```

The radial deflection is bounded, and the horizontal correction is undone before sampling:

```text
deflection = clamp(0.06 * RsScreen / max(r, 0.00001), 0, 0.045)
direction = delta / max(r, 0.00001)
sampleOffset = (direction.x / aspect, direction.y) * deflection
sampleUV = clamp(uv - sampleOffset, (0.001, 0.001), (0.999, 0.999))
```

The hole mask is `1 - smoothstep(0.98*RsScreen, 1.02*RsScreen, r)`. Inside it the shader mixes toward black. The distortion amount is multiplied by four smooth gates: radial falloff `1-smoothstep(max(1.2*RsScreen,0.001), max(18*RsScreen,0.22),r)`, offscreen falloff `1-smoothstep(0,0.35,outsideNdc)`, screen-border falloff `smoothstep(0,max(2*RsScreen,0.035),borderDistance)`, and clip-depth falloff `1-smoothstep(1,1.15,abs(holeNdc.z))`. A hole behind the camera (`holeClip.w <= 0`) returns the original sample. Idle and L OFF bypass the pass; the progress-based reveal fades it in and out.

## Explanation for the defence

The disk shader treats the ring as a flat rotating gas sheet. At each pixel it measures distance and angle from the black hole. It rotates that angle over time, faster near the inner edge, and uses it to sample several layers of smooth random noise. Those layers make uneven, animated streams instead of a perfectly smooth ring. Radius selects the temperature colors: the inner gas is almost white and emits values above 1.0, the middle is orange, and the outside cools to dark red. A tangent vector represents the orbiting gas velocity. Comparing it with the camera direction brightens the side moving toward the viewer and dims the receding side. Smooth alpha fades hide the inner and outer mesh edges, while alpha blending lets the disk sit over the scene without writing depth.

The lensing shader receives the scene texture and the hole's center and radius. It projects that center into the image, measures each pixel's distance from it, and samples the scene at a shifted coordinate. The shift bends stars and planets toward the hole, while the projected horizon is painted black. Aspect correction keeps the radial math round on wide screens. The effect fades near screen edges and outside a radius around the hole, and is disabled behind the camera or while Idle, so there is no abrupt rectangular seam.

Spaghettification here is a controllable visual approximation: the body model's local axis pointing toward the hole grows by `s`, and the two sideways axes shrink by `1/sqrt(s)`. Recomputing from the original orbit position makes the same formulas run backward during Returning, restoring bodies without accumulated drift. The state machine accumulates `p`, eases speed changes, and uses B to summon/return; mission completion/loss still comes from the existing score target and `isSunSwallowed()` interface.

## FPS measurement template

Use the same scene, camera, resolution, and quality setting for each pair. The values have not been measured in this environment.

| Lensing | Quality | Resolution | Average FPS | Notes |
|---|---|---:|---:|---|
| On | High |  |  |  |
| Off | High |  |  |  |
| On | Low |  |  |  |
| Off | Low |  |  |  |
