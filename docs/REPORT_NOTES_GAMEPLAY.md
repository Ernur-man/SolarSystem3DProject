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
