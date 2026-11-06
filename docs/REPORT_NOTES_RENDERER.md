# HDR, Bloom, and Tone-Mapping Notes

## Render pass diagram

```text
Sky + Sun + planets + orbit lines + black-hole objects + asteroid belt + meteors
                         |
                         v
                HDR scene FBO (RGBA16F + depth)
                         |
                  [lensing hook]
                         |
                         v
                  Bright-pass FBO
                         |
                         v
       Half-resolution Gaussian blur ping-pong
          horizontal <-> vertical, 8/4 iterations
                         |
                         v
       HDR scene + optional blurred bloom texture
                         |
                         v
             ACES tone map -> sRGB conversion
                         |
                         v
                       Screen
```

The scene target is `PostProcess`'s RGBA16F color texture plus a depth renderbuffer. After `endScene()`, the optional fullscreen pass reads that texture and writes into its own texture; that texture is passed to `composite()`. Black-hole lensing uses this hook. With bloom disabled, the same source still goes through exposure and tone mapping.

## Equations used

All color calculations below operate on linear RGB before the final sRGB encoding.

### Bright-pass luminance and soft knee

Rec. 709 luminance is:

```text
Y = dot(color, (0.2126, 0.7152, 0.0722))
T = 0.85                         // threshold
K = 0.5                          // soft knee
x = clamp(Y - T + K, 0, 2*K)
soft = x*x / (4*K + 0.00001)
contribution = max(Y - T, soft) / max(Y, 0.00001)
brightColor = color * contribution
```

The `soft` term eases highlights into the bloom range around the threshold. The small denominators avoid division by zero.

### Separable Gaussian blur

The shader uses the center tap and two symmetric pairs of taps along one axis per pass:

```text
w0 = 0.40262
w1 = 0.24420
w2 = 0.05449
blur(uv) = w0*sample(uv)
         + w1*(sample(uv + axis) + sample(uv - axis))
         + w2*(sample(uv + 2*axis) + sample(uv - 2*axis))
```

These are normalized samples of `exp(-x*x/2)` for offsets `x = 0, 1, 2`. The horizontal pass uses one bloom texel in x; the vertical pass uses one bloom texel in y. Bloom targets are half the HDR scene dimensions. Eight horizontal/vertical pairs run at High quality, and four pairs run at Low quality.

### Add, exposure, ACES, and gamma

When bloom is enabled, its blurred RGB is added to the scene RGB. Exposure multiplies the combined linear HDR value. The fitted ACES curve then maps it into display range:

```text
c = (scene + (bloomEnabled ? blurredBloom : 0)) * exposure
ACES(c) = clamp((c*(2.51*c + 0.03)) / (c*(2.43*c + 0.59) + 0.14), 0, 1)
```

The final linear display channel `c` is converted to sRGB with the piecewise transfer function:

```text
sRGB(c) = 12.92*c                         if c <= 0.0031308
          1.055*c^(1/2.4) - 0.055         otherwise
```

The shader applies this conversion at the end of the composite. The optional sky image is uploaded as `GL_SRGB8_ALPHA8`, so OpenGL decodes it to linear values when sampled into the HDR scene. The HDR scene and bloom targets are floating point and are not sRGB encoded.

## Quality and controls

`G` prints and toggles the bloom contribution. `Q` prints and toggles quality. High quality renders the scene at full framebuffer resolution, uses eight blur pairs, and selects the high-detail sphere mesh. Low quality renders at 0.75 scale, uses four blur pairs, and selects the lower-detail sphere mesh. A quality change recreates the HDR and blur targets; the lensing target is resized to match the HDR texture before the pass runs.

## Plain-language shader explanation

The bright-pass shader first estimates how bright each linear-color pixel is. Pixels above the chosen brightness threshold pass through, and the soft knee blends pixels near that cutoff instead of making a hard edge. The blur shader spreads those bright pixels with a small Gaussian kernel. It does one horizontal sweep and one vertical sweep, repeated several times, which creates a broad glow at lower cost than sampling a large two-dimensional blur kernel. The composite adds that glow to the original HDR image, applies exposure and the ACES curve, and converts the result to the monitor's sRGB color space. Turning bloom off skips only the added glow; the HDR tone-mapping path remains the same.

## Resize, meteors, and asteroids

The GLFW framebuffer callback uses pixel dimensions to update `glViewport` and recreate the existing PostProcess HDR/bloom targets plus BlackHole's lensing output target. Zero-sized minimized framebuffers skip rendering. Logical window dimensions drive ImGui; mouse picking converts logical cursor coordinates to framebuffer pixels before building its ray.

Meteor travel uses accumulated simulation time, so pause freezes motion and speed changes affect it:

```text
phase = fract(initialPhase + simTime * speed / 32)
position = start + direction * (72 * phase)
tailAlpha = smoothstep(0, 0.26, v) * (1 - smoothstep(0.80, 1, v))
            * (1 - smoothstep(0.35, 0.5, abs(u - 0.5))) * 0.72
```

Each meteor draws a head and camera-facing tapered tail with additive blending and depth writes disabled. The pass restores the prior blend/depth/program/VAO state. M toggles meteors; High draws 36 and Low draws 18. They render inside the HDR scene before lensing and bloom.

Asteroids use a low-resolution sphere mesh. I switches between individual model draws and one instanced draw, and the HUD reports draw-call count. Counts are 4,000 High and 1,800 Low. Collision centers/radii come from the current BlackHole-adjusted body transforms. For distance `d` from a body with radius `R`:

```text
influence = 1.5 * R
falloff = 1 - smoothstep(R, influence, d)
position += normalize(position - planetPosition) * (influence - d) * falloff
heat = 1 - smoothstep(R, influence, d)
glow = mix((1.4, 0.18, 0.015), (3.2, 2.0, 0.72), heat) * heat
```

The glow is linear HDR emission above 1.0 and enters the bloom input. Current Mars orbit/radius (7.1/0.30) and Jupiter orbit/radius (9.5/0.85) make the requested three-radius bounds cross: 8.0 versus 6.95. The implemented available-gap choice uses 1.5R clearance, giving a belt interval of 7.55 to 8.225, and ±0.20 vertical spread.

No FPS measurements were collected in this environment; no performance values are claimed.

## Milky Way view trails

The view is a stylized visualization. Solar System physics, picking and black-hole effects stay in the original Sun-centred coordinates. The galactic drift is `D(t) = galaxyDir * kGalaxySpeed * galaxyTime`, where `galaxyDir` is tilted 60 degrees above the ecliptic and time advances only with the simulation while Milky Way mode is active. At fixed simulation-time intervals each final body position `p_i` is stored in a bounded ring buffer as `G_i = D(t_i) + p_i`; rendering subtracts the current offset, `G_i - D(now)`, so the bodies remain in the original frame while their trails stream behind them. Ribbon opacity follows `alpha = (1 - age)^2 * g`, where `g` is the eased two-second transition. The Sun has a straight trail, and planets plus the Moon use their black-hole-adjusted render positions. High quality retains up to 1,200 points per body; Low shows at most 500. Y or the panel toggle is available in Free Explore with the black hole Idle; Space pauses trail growth and speed changes its simulated-time rate.

## Earth day/night, clouds, and atmosphere

Earth uses optional sRGB day, night, and cloud maps; normal and specular maps are sampled as linear data. Missing maps are reported with their file paths and use the corresponding shader fallback. In the fallback, four-octave FBM at scale 2.7 makes connected continents, thresholded around sea level 0.50. A shallow-water transition adds cyan near coasts; a separate terrain FBM field selects green lowlands, brown/olive highlands, sandy dry equatorial areas, snowy peaks, and polar ice. The fallback ocean mask alone gets a Blinn-Phong specular highlight. The surface combines day and night with a soft terminator:

```text
N = normalized surface normal (optionally normal-mapped)
L = normalize(lightPosition - worldPosition)
dayFactor = smoothstep(-0.12, 0.12, dot(N, L))
surface = mix(nightColor, dayColor, dayFactor)
```

Procedural city lights come from `smoothstep(0.68, 0.79, FBM(localPosition*72 + offset))*2.4*landMask`, so the fallback emits them only on land. A supplied night map is also masked by the procedural land mask. Ocean specular is similarly multiplied by the ocean mask, keeping the procedural land matte. The cloud sphere uses a different angular rate (`rotationAngle * 1.12`) and a scale factor of `1.025`. If no cloud texture is loaded, its coverage is `smoothstep(0.54, 0.72, FBM(localPosition*13 + offset))`, multiplied by 0.62 and the same black-hole fade as Earth. Missing Earth maps print console warnings and use procedural surface, lights, and clouds.

The atmosphere shell is 1.07 times the body scale. Its additive blue rim uses the Fresnel approximation:

```text
rim = pow(1 - max(dot(N, V), 0), 3)
atmosphereRGB = (0.12, 0.42, 1.0) * rim * 1.8 * bodyOpacity
```

Here `V` points from the surface to the camera. The rim grows at grazing angles; additive blending makes the shell glow without writing depth. The planet and its shells reuse the black-hole-adjusted position, basis, stretch, and opacity, so they return together.

## Saturn ring and mutual shadows

The ring is a flat annulus with Saturn's fixed 27-degree axial tilt. Its plane shares Saturn's black-hole-adjusted center, basis, size scale, and swallow opacity. Surface rotation must not rotate the ring normal: Saturn uses `translate * blackHoleBasis * rotateX(27 degrees) * rotateY(spin) * scale`, while the ring and its normal use `blackHoleBasis * rotateX(27 degrees)` without the spin matrix. This keeps the polar axis and equatorial ring plane fixed while the texture moves horizontally around Saturn's own vertical axis. Other planets use local-Y spin; Uranus has no separate axial tilt in this stylized scene. If `saturn_ring.png` is available its color and alpha are sampled by radial coordinate. Otherwise, the shader mixes radial sine bands with value noise and fades gaps at both annulus edges.

The ring pattern rotates around local +Z using `ringAngle = simTime * kRingSpinSpeed`, with `kRingSpinSpeed = 2*pi/25` radians per simulated second (one inner-edge turn per 25 seconds at 1x).
Its model is `orbitTranslate * blackHoleBasis * tiltX * rotateZ(ringAngle) * ringScale`; both shadow tests use the fixed plane normal from `blackHoleBasis * tiltX`, without the spin.
For local annulus coordinates `(x,y)`, `r=length(x,y)` and `v(r)=clamp((innerRadius/r)^1.5,0.32,1)`, giving `omega(r)=kRingSpinSpeed*v(r)` so inner streaks move faster than outer ones.
The shader uses `patternAngle=atan(y,x)+ringAngle*(1-v(r))` to compensate for mesh spin; animated angular streak/noise overlays procedural bands and radial ring textures. Because `ringAngle` uses `simTime`, Space pauses it and simulation speed scales it without a real-time jump.

For ring fragments, the Sun ray is tested against Saturn's sphere. With `o = fragmentPosition - saturnCenter`, normalized ray `D`, and Saturn radius `R`:

```text
b = dot(o, D)
c = dot(o, o) - R*R
discriminant = b*b - c
tNear = -b - sqrt(max(discriminant, 0))
shadow if discriminant >= 0 and 0 < tNear < distanceToSun
ringLight = 0.22 in shadow, otherwise 1
```

For the reverse shadow on Saturn, the shader intersects its point-to-Sun ray with the ring plane. With ring center `C`, plane normal `n`, and ray direction `L`:

```text
t = dot(C - fragmentPosition, n) / dot(L, n)
intersection = fragmentPosition + t*L
r = length(intersection - C - n*dot(intersection - C, n))
shadow if 0 < t < distanceToSun and ringInnerRadius <= r <= ringOuterRadius
directLight = 0.22 in shadow, otherwise 1
```

The ring draw disables culling only for its annulus, alpha blends it, and turns off depth writes while leaving depth testing active. It is rendered after the opaque bodies and inside the scene HDR pass, so bloom and lensing operate on it along with the rest of the scene.

## Plain-language explanation for the defence

Earth's shader checks whether a surface point faces the Sun. A smooth transition around a zero normal/light dot product makes day fade into night. On the dark side it adds either the night texture or small procedural warm lights. When maps are absent, layered noise builds continents and clouds, so no image assets are required. A second, slightly larger sphere carries clouds and rotates at a different rate. The outer atmosphere shell uses the view angle: it adds little light when looked at face-on and more at the silhouette, producing a blue rim.

Saturn's ring is an annular mesh with transparent gaps. For each ring fragment, a ray toward each light is checked against Saturn's sphere; if it hits before reaching the light, that light contribution is darkened. Saturn's surface performs the reciprocal check by intersecting each point-to-light ray with the ring plane and checking whether the hit lies within the annulus. The same transformed position and opacity used for Saturn also drive its ring, while its shadow calculations keep using analytic sphere/plane tests.

## Earth’s Moon and optional planet maps

The scene did not previously render a Moon; the planet data already reported `moons: 1` for Earth. The new Moon is a small grey cratered sphere and is not selectable. Earth’s stylized orbital period is:

```text
earthYearSeconds = ORBIT_BASE_SECONDS * (earthOrbitalDays / 365.256)^ORBIT_COMPRESSION_EXPONENT
moonPeriodSeconds = earthYearSeconds / 12
moonAngle = simTime * 2*pi / moonPeriodSeconds
moonPosition = earthPosition(simTime) + (moonRadiusOrbit*cos(moonAngle), 0,
                                          moonRadiusOrbit*sin(moonAngle))
moonSpinAngle = -moonAngle - pi/2                 # local +Z faces Earth; tidal lock
```

`moonRadiusOrbit` is 1.02 scene units and Moon radius is 0.105; Earth radius is 0.38. The orbit guide is centered on transformed Earth and drawn with the other guides before opaque bodies with depth writes off. The Moon's position is evaluated from accumulated simulation time, then passed through the same pull/stretch/fade function as other planet bodies. Pause, speed, return, and restart therefore share their existing time and black-hole behavior.

The common planet mesh supplies equirectangular UVs. Optional `mercury.jpg`, `venus.jpg`, `mars.jpg`, `jupiter.jpg`, `saturn.jpg`, `uranus.jpg`, `neptune.jpg`, and `moon.jpg` maps are uploaded as `GL_SRGB8_ALPHA8`, mipmapped, and given up to 8x anisotropy if `GL_EXT_texture_filter_anisotropic` is present. A missing file logs its full path and selects procedural shading. `Q` only changes render resolution, blur work, and sphere mesh detail, so UV textures and mipmaps remain enabled at Low quality. No Earth texture path or shader is changed.

The fallback shader uses four-octave value-noise FBM `F(p)=sum(a_i*N(p_i))`, with `a_(i+1)=0.5*a_i` and `p_(i+1)=2.03*p_i+(7.1,13.7)`. Venus offsets its latitude cloud sine by warped FBM. Gas-giant bands use `sin(v*frequency + FBM*warp)` so noise breaks up band edges. Jupiter overlays an elliptical storm centered at UV `(0.72,0.59)`. Mars thresholds broad FBM into rust-dark regions and blends white polar caps with `1-smoothstep(0.035,0.075,min(v,1-v))`. Mercury and Moon use a 3x3 cellular-neighbor crater scan with dark bowl/rim thresholds; Uranus uses low-amplitude cyan bands, Saturn uses a fine pale-gold latitude sine, and Neptune thresholds stretched FBM into sparse blue streaks. The UVs are part of the spinning sphere mesh, so all fallback patterns rotate with the body.

Surface color is either the sampled sRGB-decoded map or the procedural albedo, multiplied by the body's base tint. Two point-light terms (Sun at origin and movable light) use diffuse and restrained Blinn-Phong highlights. Saturn's ring visibility multiplies each light's contribution on its surface; the ring shader likewise tests Saturn's sphere against both lights. Final planet radiance is multiplied by black-hole opacity, preserving the existing fade/stretch. Solar System Scope texture credit: **Textures by Solar System Scope, licensed CC BY 4.0; attribution is required when using them.**
