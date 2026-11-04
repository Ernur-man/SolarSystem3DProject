# Event Horizon: Solar System Explorer — Graphics Report Notes

This note describes only equations currently present in shader source. The post shader is a sample fragment shader and is not wired into the current frame renderer.

## Current render flow

```text
UV sphere mesh -> planet.vert (model/view/projection, inverse-transpose normal)
               -> planet.frag (Blinn-Phong) -> default framebuffer
Sun sphere     -> planet.vert -> sun.frag (four-octave value-noise FBM) -> default framebuffer
```

## Blinn-Phong equations (`planet.frag`)

`N = normalize(normal)`, `L = normalize(lightPos - worldPos)`, `V = normalize(cameraPos - worldPos)`.

`diffuse = max(dot(N,L), 0)`; `specular = max(dot(N, normalize(L+V)), 0)^32`.

`colorOut = color * (0.12 + 0.88 * diffuse) + 0.22 * specular + color * emissive`.

## Solar surface (`sun.frag`)

Value noise is smoothed by `f = f*f*(3-2*f)` and interpolated over eight lattice corners. FBM is `sum(i=0..3, amplitude_i * noise(frequency_i * p))`, with amplitude halved and frequency multiplied by 2.03 per octave. `p` receives an animated y offset. This is a procedural animated surface, not a physically based solar simulation.

## Post shader prototype (`post.frag`)

The current UV deflection is `bent = uv - normalize(uv-holeUV) * min(0.018/(r+0.035),0.12)` where `r=length(uv-holeUV)`. Composite is Reinhard `c/(c+1)` followed by gamma `c^(1/2.2)`. No HDR scene texture, bright-pass extraction, ping-pong Gaussian blur, event-horizon projection or disk is connected, so lensing and bloom are not delivered render features.

## Required benchmark table template

| Technique configuration | Resolution | Average FPS | GPU / driver | Notes |
|---|---:|---:|---|---|
| Baseline | 1280x720 | | | |
| Bloom off / on | 1280x720 | | | Not implemented |
| Lensing off / on | 1280x720 | | | Not implemented |
| Instancing off / on | 1280x720 | | | Not implemented |
| Atmosphere off / on | 1280x720 | | | Not implemented |
