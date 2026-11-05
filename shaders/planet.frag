#version 330 core

in vec3 worldPos;
in vec3 normal;
in vec2 surfaceUV;
out vec4 FragColor;

uniform vec3 color;
uniform vec3 lightPos;
uniform vec3 sunLightPos;
uniform vec3 cameraPos;
uniform float emissive;
uniform float opacity;
uniform bool ringEnabled;
uniform vec3 ringCenter;
uniform vec3 ringNormal;
uniform float ringInnerRadius;
uniform float ringOuterRadius;
uniform sampler2D surfaceTexture;
uniform bool hasSurfaceTexture;
uniform int proceduralType;

float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float valueNoise2D(vec2 p) {
    vec2 cell = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(cell), hash21(cell + vec2(1, 0)), f.x),
               mix(hash21(cell + vec2(0, 1)), hash21(cell + vec2(1, 1)), f.x), f.y);
}

float fbm(vec2 p) {
    float sum = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; ++i) {
        sum += amplitude * valueNoise2D(p);
        p = p * 2.03 + vec2(7.1, 13.7);
        amplitude *= 0.5;
    }
    return sum;
}

float craterNoise(vec2 uv) {
    vec2 p = uv * vec2(52.0, 27.0);
    vec2 cell = floor(p);
    float craters = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 id = cell + vec2(x, y);
            vec2 center = vec2(float(x), float(y)) + vec2(hash21(id), hash21(id + 19.7)) * 0.72 + 0.14;
            float radius = mix(0.12, 0.43, hash21(id + 47.2));
            float d = length(fract(p) - center);
            float bowl = 1.0 - smoothstep(radius * 0.38, radius * 0.86, d);
            float rim = smoothstep(radius * 0.78, radius * 0.94, d) *
                        (1.0 - smoothstep(radius * 0.94, radius * 1.18, d));
            craters = max(craters, bowl * 0.58 - rim * 0.28);
        }
    }
    return craters;
}

vec3 proceduralSurface() {
    vec2 uv = surfaceUV;
    float broad = fbm(uv * vec2(9.0, 15.0));
    if (proceduralType == 0 || proceduralType == 7) {
        float crater = craterNoise(uv);
        float grain = fbm(uv * 95.0) * 0.10;
        vec3 grey = proceduralType == 7 ? vec3(0.49, 0.49, 0.47) : vec3(0.40, 0.39, 0.37);
        return grey + vec3(crater + grain);
    }
    if (proceduralType == 1) {
        vec2 warp = vec2(fbm(uv * vec2(5.0, 18.0)), fbm(uv * vec2(7.0, 13.0) + 8.0));
        float clouds = sin(uv.y * 76.0 + (warp.x - 0.5) * 8.0 + sin(uv.x * 8.0 + warp.y));
        return mix(vec3(0.64, 0.38, 0.16), vec3(0.96, 0.76, 0.43), smoothstep(-0.35, 0.45, clouds));
    }
    if (proceduralType == 2) {
        float regions = fbm(uv * vec2(12.0, 8.0));
        float latitude = min(uv.y, 1.0 - uv.y);
        vec3 rust = mix(vec3(0.30, 0.075, 0.035), vec3(0.78, 0.23, 0.075), smoothstep(0.30, 0.68, regions));
        rust = mix(rust, vec3(0.86, 0.77, 0.64), 1.0 - smoothstep(0.035, 0.075, latitude));
        return rust;
    }
    if (proceduralType == 3) {
        float warp = (fbm(uv * vec2(8.0, 20.0)) - 0.5) * 0.25;
        float band = sin((uv.y + warp) * 108.0 + (fbm(uv * vec2(17.0, 42.0)) - 0.5) * 2.6);
        vec3 bands = mix(vec3(0.39, 0.19, 0.10), vec3(0.88, 0.67, 0.43), smoothstep(-0.38, 0.42, band));
        vec2 spot = vec2((uv.x - 0.72) * 1.5, (uv.y - 0.59) * 2.5);
        spot.x = min(abs(spot.x), 1.5 - abs(spot.x));
        float storm = 1.0 - smoothstep(0.05, 0.22, length(spot));
        float eye = 1.0 - smoothstep(0.02, 0.085, length(spot * vec2(1.0, 1.3)));
        bands = mix(bands, vec3(0.72, 0.25, 0.10), storm * 0.85);
        return mix(bands, vec3(0.9, 0.53, 0.30), eye * 0.55);
    }
    if (proceduralType == 4) {
        float band = sin(uv.y * 132.0 + (broad - 0.5) * 5.0);
        return mix(vec3(0.60, 0.47, 0.29), vec3(0.91, 0.79, 0.56), smoothstep(-0.32, 0.42, band));
    }
    if (proceduralType == 5) {
        float band = sin(uv.y * 25.0 + (broad - 0.5) * 1.7);
        return mix(vec3(0.26, 0.65, 0.70), vec3(0.38, 0.78, 0.80), smoothstep(0.35, 0.8, band) * 0.20);
    }
    float streakNoise = fbm(uv * vec2(7.0, 29.0));
    float streak = smoothstep(0.66, 0.84, streakNoise + sin(uv.y * 46.0 + uv.x * 3.0) * 0.08);
    return mix(vec3(0.055, 0.12, 0.39), vec3(0.22, 0.62, 0.95), streak * 0.72);
}

float ringLightFor(vec3 lightPosition) {
    if (!ringEnabled)
        return 1.0;
    vec3 toLight = lightPosition - worldPos;
    float distanceToLight = length(toLight);
    vec3 ray = toLight / max(distanceToLight, 0.00001);
    float denominator = dot(ray, normalize(ringNormal));
    if (abs(denominator) <= 0.0001)
        return 1.0;
    float t = dot(ringCenter - worldPos, normalize(ringNormal)) / denominator;
    if (t <= 0.0 || t >= distanceToLight)
        return 1.0;
    vec3 intersection = worldPos + ray * t;
    vec3 offset = intersection - ringCenter;
    float radius = length(offset - normalize(ringNormal) * dot(offset, normalize(ringNormal)));
    return radius >= ringInnerRadius && radius <= ringOuterRadius ? 0.22 : 1.0;
}

void main() {
    vec3 albedo = hasSurfaceTexture ? texture(surfaceTexture, surfaceUV).rgb : proceduralSurface();
    albedo *= color;
    vec3 N = normalize(normal);
    vec3 V = normalize(cameraPos - worldPos);
    vec3 LSun = normalize(sunLightPos - worldPos);
    vec3 LMove = normalize(lightPos - worldPos);
    float sunDiffuse = max(dot(N, LSun), 0.0) * ringLightFor(sunLightPos);
    float moveDiffuse = max(dot(N, LMove), 0.0) * ringLightFor(lightPos);
    float sunSpecular = pow(max(dot(N, normalize(LSun + V)), 0.0), 32.0) * ringLightFor(sunLightPos);
    float moveSpecular = pow(max(dot(N, normalize(LMove + V)), 0.0), 32.0) * ringLightFor(lightPos);
    vec3 lit = albedo * (0.10 + 0.44 * sunDiffuse + 0.44 * moveDiffuse) +
               vec3(0.11) * (sunSpecular + moveSpecular);
    FragColor = vec4((lit + albedo * emissive) * opacity, 1.0);
}
