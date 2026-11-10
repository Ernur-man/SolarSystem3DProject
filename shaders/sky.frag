#version 330 core

in vec3 direction;
out vec4 FragColor;

uniform sampler2D starTexture;
uniform bool useTexture;
uniform vec3 skyParallax;
uniform bool highQuality;

float hash3(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float valueNoise3D(vec3 p) {
    vec3 cell = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float c000 = hash3(cell);
    float c100 = hash3(cell + vec3(1.0, 0.0, 0.0));
    float c010 = hash3(cell + vec3(0.0, 1.0, 0.0));
    float c110 = hash3(cell + vec3(1.0, 1.0, 0.0));
    float c001 = hash3(cell + vec3(0.0, 0.0, 1.0));
    float c101 = hash3(cell + vec3(1.0, 0.0, 1.0));
    float c011 = hash3(cell + vec3(0.0, 1.0, 1.0));
    float c111 = hash3(cell + vec3(1.0, 1.0, 1.0));
    float low = mix(mix(c000, c100, f.x), mix(c010, c110, f.x), f.y);
    float high = mix(mix(c001, c101, f.x), mix(c011, c111, f.x), f.y);
    return mix(low, high, f.z);
}

vec2 nebulaFbm(vec3 p, int octaves) {
    vec2 sum = vec2(0.0);
    float amplitude = 0.5;
    float amplitudeSum = 0.0;
    for (int octave = 0; octave < 5; ++octave) {
        if (octave >= octaves) break;
        sum.x += valueNoise3D(p) * amplitude;
        sum.y += valueNoise3D(p + vec3(13.7, 5.3, 21.1)) * amplitude;
        amplitudeSum += amplitude;
        p *= 2.03;
        amplitude *= 0.5;
    }
    return sum / max(amplitudeSum, 0.0001);
}

void main() {
    vec3 d = normalize(direction + skyParallax);
    float longitude = atan(d.z, d.x);
    float latitude = asin(clamp(d.y, -1.0, 1.0));
    vec2 uv = vec2(longitude / (2.0 * 3.14159265) + 0.5, latitude / 3.14159265 + 0.5);

    if (useTexture) {
        FragColor = texture(starTexture, uv);
        return;
    }

    // Quantized 3D direction hashes create sparse stars; a second hash varies their brightness and
    // tint.
    vec3 starCell = floor(d * 540.0);
    float starHash = hash3(starCell);
    float star = step(0.9988, starHash);
    float brightness = mix(0.25, 1.5, hash3(starCell + vec3(19.1, 7.7, 3.4)));
    float tint = hash3(starCell + vec3(5.2, 31.7, 11.3));
    vec3 starColor = mix(vec3(0.62, 0.76, 1.0), vec3(1.0, 0.88, 0.70), tint);

    // Keep the broad galactic band while warping the 3D view direction so clouds stay anchored
    // to the sky as the camera rotates.
    float bandDistance = latitude + 0.12 * sin(longitude * 2.0);
    float band = exp(-pow(bandDistance * 4.0, 2.0));
    float warpA = valueNoise3D(d * 2.2 + vec3(7.1, 19.4, 3.8));
    float warpB = valueNoise3D(d * 2.2 + vec3(23.6, 2.7, 11.5));
    vec3 warpedDirection = d * 3.0 +
        (vec3(warpA, warpB, warpA * 0.65 + warpB * 0.35) - 0.5) * 1.25;
    vec2 clouds = nebulaFbm(warpedDirection, highQuality ? 5 : 3);
    float tealCloud = smoothstep(0.38, 0.78, clouds.x) * band;
    float cyanCloud = smoothstep(0.58, 0.92, clouds.y) * band;
    float warmDust = smoothstep(0.69, 0.95, clouds.y) * band;

    // The component-wise maximum is (0.2075, 0.215, 0.263), or 0.217 Rec. 709
    // luminance, safely below the bright-pass soft-knee onset at 0.35.
    vec3 nebula = vec3(0.0015, 0.002, 0.006)
        + tealCloud * vec3(0.028, 0.110, 0.140)
        + cyanCloud * vec3(0.018, 0.065, 0.105)
        + warmDust * vec3(0.160, 0.038, 0.012);

    // A handful of fixed, larger points add depth without twinkling; the soft halo stays local.
    vec3 largeStarCell = floor(d * 112.0);
    float largeStar = step(0.99996, hash3(largeStarCell + vec3(41.3, 17.8, 6.2)));
    float largeStarDistance = length(fract(d * 112.0) - 0.5);
    float largeStarCore = (1.0 - smoothstep(0.12, 0.22, largeStarDistance)) * largeStar;
    float largeStarGlow = exp(-largeStarDistance * 13.0) * largeStar;
    vec3 largeStarLight = vec3(1.6, 1.45, 1.2) * largeStarCore
                        + vec3(1.25, 0.92, 0.68) * largeStarGlow;

    vec3 color = nebula + star * starColor * brightness + largeStarLight;
    FragColor = vec4(color, 1.0);
}
