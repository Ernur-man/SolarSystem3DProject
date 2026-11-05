# Black Hole Renderer Notes

## Draw order

```text
Sky background
      |
      v
Orbit lines and selection ring
      |
      v
Opaque planets and Sun (depth test/write ON, culling ON)
      |
      v
Tilted accretion disk (depth test ON, depth write OFF, blending ON, culling OFF)
      |
      v
Pure-black event horizon sphere (opaque, normal depth/culling state restored)
      |
      v
HDR bloom and tone-map composite
      |
      v
ImGui HUD
```

The disk mesh is a flat triangle ring. Its model transform shares the same world-space center and visibility scale as the black sphere, then rotates the ring plane by 30 degrees. The disk is blended only while it is drawn; depth write, culling, blending enable, and blend factors are restored immediately afterward.

## Motion and visibility

The B key changes the target direction. The progress changes at a 60-second rate and is accumulated, so reversing B changes only the direction of travel:

```text
step = dt * threatSpeedMultiplier / 60
p(next) = clamp(p + step, 0, 1)        while approaching
p(next) = clamp(p - step, 0, 1)        while returning
```

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

## Explanation for the defence

The disk shader treats the ring as a flat rotating gas sheet. At each pixel it measures distance and angle from the black hole. It rotates that angle over time, faster near the inner edge, and uses it to sample several layers of smooth random noise. Those layers make uneven, animated streams instead of a perfectly smooth ring. Radius selects the temperature colors: the inner gas is almost white and emits values above 1.0, the middle is orange, and the outside cools to dark red. A tangent vector represents the orbiting gas velocity. Comparing it with the camera direction brightens the side moving toward the viewer and dims the receding side. Smooth alpha fades hide the inner and outer mesh edges, while alpha blending lets the disk sit over the scene without writing depth.
