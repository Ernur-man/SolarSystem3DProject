#version 330 core

in vec3 direction;
out vec4 FragColor;

uniform sampler2D starTexture;
uniform bool useTexture;
uniform vec3 skyParallax;
uniform bool highQuality;
uniform vec3 cameraParallax;
uniform vec3 skyDrift;
uniform int uSkyDebug;

const float kDensityThresholdLow = 0.44;
const float kDensityThresholdHigh = 0.68;
const float kDensityPower = 1.35;
const float kDustStrength = 0.78;
const float kStarDustFade = 0.8;
const float kNebulaPeak = 0.20;
const mat3 kFbmRotation = mat3(
     0.36,  0.80, -0.48,
    -0.48,  0.60,  0.64,
     0.80,  0.00,  0.60);

float hash3(vec3 p) {
    vec3 q = fract(p * vec3(0.1173, 0.0971, 0.1319));
    float crossMix = dot(q, q.yzx + vec3(11.73, 27.19, 5.31));
    q = fract(q + vec3(crossMix * 0.37, crossMix * 0.61, crossMix * 0.83));
    vec3 products = fract(q * vec3(17.17, 29.29, 43.43));
    return fract(dot(products, vec3(1.0, 2.0, 3.0)) * 0.318309886);
}

float valueNoise3D(vec3 p) {
    vec3 cell = floor(p);
    vec3 f = p - cell;
    f = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
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

float scalarFbm(vec3 p, int octaves) {
    float sum = 0.0;
    float amplitude = 0.5;
    float amplitudeSum = 0.0;
    for (int octave = 0; octave < 5; ++octave) {
        if (octave >= octaves) break;
        sum += valueNoise3D(p) * amplitude;
        amplitudeSum += amplitude;
        p = kFbmRotation * p * 2.03;
        amplitude *= 0.5;
    }
    // Each octave is in [0,1]; normalized positive weights keep FBM in [0,1].
    return clamp(sum / max(amplitudeSum, 0.0001), 0.0, 1.0);
}

void main() {
    vec3 directionInput = direction + skyParallax;
    vec3 d = directionInput / max(length(directionInput), 0.0001);
    float longitude = atan(d.z, d.x);
    float latitude = asin(clamp(d.y, -1.0, 1.0));
    vec2 uv = vec2(longitude / (2.0 * 3.14159265) + 0.5, latitude / 3.14159265 + 0.5);

    if (useTexture && uSkyDebug == 0) {
        FragColor = texture(starTexture, uv);
        return;
    }

    vec3 starCell = floor(d * 540.0);
    float star = step(0.9988, hash3(starCell));
    float brightness = mix(0.25, 1.5, hash3(starCell + vec3(19.1, 7.7, 3.4)));
    float starTemperature = hash3(starCell + vec3(5.2, 31.7, 11.3));
    vec3 starColor = mix(vec3(0.62, 0.76, 1.0), vec3(1.0, 0.88, 0.70), starTemperature);

    float warpA = valueNoise3D(d * 1.4 + skyDrift + vec3(7.1, 19.4, 3.8));
    float warpB = valueNoise3D(d * 1.4 + skyDrift + vec3(23.6, 2.7, 11.5));
    vec3 warp = (vec3(warpA, warpB, warpA * 0.65 + warpB * 0.35) - 0.5) * 0.22;
    vec3 warpedDirection = d * 2.35 + warp;

    float largeStructure = scalarFbm(d * 0.55 + skyDrift * 0.15, highQuality ? 2 : 1);
    float largeMask = smoothstep(0.34, 0.56, largeStructure);
    float rawFbm = scalarFbm(warpedDirection, highQuality ? 5 : 3);
    if (uSkyDebug == 1) {
        FragColor = vec4(vec3(rawFbm), 1.0);
        return;
    }

    float densityNoise = rawFbm;
    if (highQuality) {
        vec3 farInput = d * 1.1 + cameraParallax + skyDrift * 0.5 + vec3(5.4, 12.8, 7.1);
        densityNoise = mix(rawFbm, valueNoise3D(farInput), 0.22);
    }
    float thresholded = clamp(smoothstep(kDensityThresholdLow, kDensityThresholdHigh,
                                         densityNoise), 0.0, 1.0);
    float density = pow(max(thresholded, 0.0), kDensityPower) * largeMask;
    if (uSkyDebug == 2) {
        FragColor = vec4(vec3(density), 1.0);
        return;
    }

    float dustNoise = valueNoise3D(warpedDirection * vec3(0.72, 2.85, 0.72)
                                 + vec3(warpB, warpA, warpB) * 0.18);
    float densityAmount = 1.0 - exp(-2.5 * max(density, 0.0));
    float dust = smoothstep(0.53, 0.75, dustNoise) * densityAmount;
    if (uSkyDebug == 3) {
        FragColor = vec4(vec3(dust), 1.0);
        return;
    }

    float colourVariation = highQuality
        ? valueNoise3D(d * 14.0 + vec3(3.7, 11.1, 7.3)) : 0.5;
    float cyanBlend = 1.0 - exp(-3.0 * max(density - 0.10, 0.0));
    float orangeBlend = 1.0 - exp(-6.0 * max(density - 0.80, 0.0));
    vec3 gasColor = mix(vec3(0.010, 0.032, 0.055), vec3(0.022, 0.125, 0.165), cyanBlend);
    vec3 palette = mix(gasColor, vec3(0.19, 0.055, 0.014), orangeBlend);
    palette *= 0.93 + colourVariation * 0.14;

    float densityGradient = length(vec2(dFdx(density), dFdy(density)));
    float edgeGlow = clamp(densityGradient * 18.0, 0.0, 1.0) * densityAmount;
    vec3 nebula = vec3(0.001, 0.0015, 0.004)
                + palette * densityAmount * 0.70
                + vec3(0.009, 0.035, 0.055) * edgeGlow;
    nebula *= 1.0 - dust * 0.78;
    bool nebulaInvalid = any(isnan(nebula)) || any(isinf(nebula));
    if (nebulaInvalid) nebula = vec3(0.0);
    // This cap is above the conservative component bounds; peak Rec. 709 luminance is ~0.139,
    // below the bright-pass soft-knee onset at 0.35.
    nebula = min(nebula, vec3(kNebulaPeak));

    float starVisibility = 1.0 - dust * 0.8;
    vec3 largeStarCell = floor(d * 112.0);
    float largeStar = step(0.99996, hash3(largeStarCell + vec3(41.3, 17.8, 6.2)));
    float largeStarDistance = length(fract(d * 112.0) - 0.5);
    float largeStarCore = 1.0 - smoothstep(0.12, 0.22, largeStarDistance);
    float largeStarGlow = exp(-largeStarDistance * 13.0);
    float largeStarTemperature = hash3(largeStarCell + vec3(11.4, 37.2, 4.9));
    vec3 largeStarColor = mix(vec3(1.05, 1.28, 1.65), vec3(1.65, 1.30, 0.94),
                              largeStarTemperature);
    vec3 largeStarLight = largeStar * largeStarColor
                        * (1.6 * largeStarCore + 0.9 * largeStarGlow) * starVisibility;
    vec3 color = nebula + star * starColor * brightness * starVisibility + largeStarLight;

    if (uSkyDebug == 4) {
        bool invalid = nebulaInvalid || any(isnan(color)) || any(isinf(color));
        FragColor = invalid ? vec4(1.0, 0.0, 1.0, 1.0) : vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    if (any(isnan(color)) || any(isinf(color))) color = vec3(0.0);
    FragColor = vec4(color, 1.0);
}
