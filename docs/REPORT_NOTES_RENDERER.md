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
