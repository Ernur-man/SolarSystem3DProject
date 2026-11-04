# Renderer Notes: HDR and Bloom

## Render passes

```text
Complete scene (sky, orbit lines, Sun, planets)
                    |
                    v
             RGBA16F HDR FBO
                    |
          +---------+---------------------> HDR texture hook for lensing
          |                                  (optional teammate pass)
          v
      Bright-pass
          |
          v
  Blur ping-pong: horizontal <-> vertical (half resolution)
          |
          +---------------------+
                                v
HDR scene + blurred highlights -> composite -> ACES tone map -> sRGB encode -> screen
```

`PostProcess::beginScene()` and `endScene()` enclose the complete 3D scene. After `endScene()`, a teammate can sample `getHdrSceneColorTexture()` at `getWidth()` by `getHeight()`, render a lensing result into a separate texture, and pass that texture to `composite()`. `composite()` then adds bloom, tone maps, and writes the final display image. ImGui is drawn afterward so its colors are not tone mapped.

## Equations used

**Bright-pass luminance** (linear Rec. 709 RGB):

```text
Y = dot(rgb, (0.2126, 0.7152, 0.0722))
soft = clamp(Y - threshold + knee, 0, 2*knee)^2 / (4*knee + 0.00001)
contribution = max(Y - threshold, soft) / max(Y, 0.00001)
brightRGB = rgb * contribution
threshold = 0.85; knee = 0.5
```

The Gaussian blur is separable. Each 1D pass samples the center and two pixels on either side; the second pass runs across the other axis. With `t` as one texel along the active axis:

```text
blur(c) = 0.40262*c(0) + 0.24420*(c(-t) + c(t))
                         + 0.05449*(c(-2t) + c(2t))
```

Weights sum to approximately 1.0. High quality uses eight horizontal/vertical iteration pairs, and Low uses four. The blur targets are half the selected HDR scene width and height.

**Composite and ACES fitted tone curve** (per linear RGB channel):

```text
hdr = scene + bloomTexture                 // bloomTexture omitted when G is OFF
x = hdr * exposure                         // exposure defaults to 1.0
aces(x) = clamp(x*(2.51*x + 0.03) / (x*(2.43*x + 0.59) + 0.14), 0, 1)
```

**Linear-to-sRGB output encoding** (per channel, after tone mapping):

```text
sRGB(x) = 12.92*x                           when x <= 0.0031308
          1.055*x^(1/2.4) - 0.055            otherwise
```

Scene shaders produce linear values. The optional sky image is uploaded as `GL_SRGB8_ALPHA8`, so OpenGL decodes its sRGB samples to linear values before shading. The final pass manually encodes to sRGB with framebuffer sRGB conversion disabled, avoiding a second gamma conversion.

## Bloom shaders in plain language

The bright-pass shader estimates each pixel's brightness from its linear color. Dark pixels contribute nothing. Bright pixels are retained, and a soft knee gradually adds pixels near the cutoff so the halo does not have a sharp edge. The blur shader spreads those highlights in two directions using normalized Gaussian weights. Doing horizontal and vertical passes separately gives a broad soft glow with far fewer samples than a two-dimensional blur. The composite shader adds that glow back to the original HDR scene, then compresses the HDR range with ACES and converts the result to the monitor's sRGB encoding. Turning bloom off skips only the highlight extraction and blur contribution; it still uses HDR and tone mapping.

## FPS measurement template

Record the same scene, window size, and camera for each row. Fill the measured value during a run; no performance values are assumed here.

| Bloom | Quality | Window size | Average FPS | Notes |
|---|---|---:|---:|---|
| On | High |  |  |  |
| Off | High |  |  |  |
| On | Low |  |  |  |
| Off | Low |  |  |  |
