# HDR, Bloom, and Tone-Mapping Notes

## Render pass diagram

```text
Sky + Sun + planets + orbit lines + black-hole objects
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

## FPS measurement template

Use the same scene, camera, resolution, and duration for each row. Record the machine and GPU driver with the results.

| Bloom | Quality | Resolution | Average FPS | Notes |
|---|---|---:|---:|---|
| On | High |  |  |  |
| Off | High |  |  |  |
| On | Low |  |  |  |
| Off | Low |  |  |  |

## Earth day/night, clouds, and atmosphere

Earth uses optional sRGB day, night, and cloud maps; normal and specular maps are sampled as linear data. Missing maps are reported with their file paths and use the corresponding shader fallback. In the fallback, smooth value noise is accumulated as four-octave FBM to make broad continents, smaller terrain variation, cloud patches, and city-light clusters. The surface combines the day color and night color with a soft terminator:

```text
N = normalized surface normal (optionally normal-mapped)
L = normalize(lightPosition - worldPosition)
dayFactor = smoothstep(-0.12, 0.12, dot(N, L))
surface = mix(nightColor, dayColor, dayFactor)
```

Procedural city lights come from `smoothstep(0.68, 0.79, FBM(localPosition*72 + offset))*2.4`; they are used as the night color's warm emission. The cloud sphere uses a different angular rate (`rotationAngle * 1.12`) and a scale factor of `1.025`. If no cloud texture is loaded, its coverage is `smoothstep(0.54, 0.72, FBM(localPosition*13 + offset))`, multiplied by 0.62 and the same black-hole fade as Earth.

The atmosphere shell is 1.07 times the body scale. Its additive blue rim uses the Fresnel approximation:

```text
rim = pow(1 - max(dot(N, V), 0), 3)
atmosphereRGB = (0.12, 0.42, 1.0) * rim * 1.8 * bodyOpacity
```

Here `V` points from the surface to the camera. The rim grows at grazing angles; additive blending makes the shell glow without writing depth. The planet and its shells reuse the black-hole-adjusted position, basis, stretch, and opacity, so they return together.

## Saturn ring and mutual shadows

The ring is a flat annulus with a 27-degree tilt and it shares Saturn's position, orientation, size scale, and swallow opacity. If `saturn_ring.png` is available its color and alpha are sampled by radial coordinate. Otherwise, the shader mixes radial sine bands with value noise and fades gaps at both annulus edges.

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

Saturn's ring is an annular mesh with transparent gaps. For each ring fragment, a short ray toward the Sun is checked against Saturn's sphere; if it hits before reaching the light, the ring is darkened. Saturn's surface performs the reciprocal check by intersecting its sunlight ray with the ring plane and checking whether the hit lies within the annulus. The same transformed position and opacity used for Saturn also drive its ring, while its shadow calculations keep using analytic sphere/plane tests.
