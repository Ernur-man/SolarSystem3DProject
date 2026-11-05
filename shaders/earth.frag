#version 330 core

in vec3 worldPos;
in vec3 localPos;
in vec3 normal;
in vec2 uv;
out vec4 FragColor;

uniform sampler2D dayTexture;
uniform sampler2D nightTexture;
uniform sampler2D cloudTexture;
uniform sampler2D normalTexture;
uniform sampler2D specularTexture;
uniform bool hasDayTexture;
uniform bool hasNightTexture;
uniform bool hasCloudTexture;
uniform bool hasNormalTexture;
uniform bool hasSpecularTexture;
uniform bool cloudPass;
uniform vec3 lightPos;
uniform vec3 cameraPos;
uniform float opacity;

float hash31(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float valueNoise3D(vec3 p) {
    vec3 cell = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = mix(hash31(cell), hash31(cell + vec3(1, 0, 0)), f.x);
    float b = mix(hash31(cell + vec3(0, 1, 0)), hash31(cell + vec3(1, 1, 0)), f.x);
    float c = mix(hash31(cell + vec3(0, 0, 1)), hash31(cell + vec3(1, 0, 1)), f.x);
    float d = mix(hash31(cell + vec3(0, 1, 1)), hash31(cell + vec3(1, 1, 1)), f.x);
    return mix(mix(a, b, f.y), mix(c, d, f.y), f.z);
}

float fbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int octave = 0; octave < 4; ++octave) {
        value += amplitude * valueNoise3D(p);
        p *= 2.03;
        amplitude *= 0.5;
    }
    return value;
}

vec3 proceduralLandColor(vec3 p) {
    // FBM supplies broad continents; a second scale breaks the coast and varies land color.
    float continents = fbm(p * 3.8 + vec3(11.0, 2.0, 7.0));
    float land = smoothstep(0.47, 0.57, continents);
    float detail = fbm(p * 18.0 + vec3(3.0, 8.0, 1.0));
    vec3 ocean = vec3(0.012, 0.08, 0.30);
    vec3 greenLand = vec3(0.09, 0.30, 0.10);
    vec3 dryLand = vec3(0.42, 0.30, 0.16);
    vec3 landColor = mix(greenLand, dryLand, smoothstep(0.48, 0.66, detail));
    return mix(ocean, landColor, land);
}

void main() {
    vec3 N = normalize(normal);
    vec3 L = normalize(lightPos - worldPos);
    vec3 V = normalize(cameraPos - worldPos);

    if (cloudPass) {
        float cloudDensity;
        if (hasCloudTexture) {
            vec4 cloudSample = texture(cloudTexture, uv);
            float luminance = dot(cloudSample.rgb, vec3(0.2126, 0.7152, 0.0722));
            cloudDensity = min(luminance, cloudSample.a);
        } else {
            // Thresholded low-frequency FBM makes broken procedural cloud patches.
            cloudDensity = smoothstep(0.54, 0.72, fbm(localPos * 13.0 + vec3(5.0)));
        }
        float cloudAlpha = cloudDensity * 0.62 * opacity;
        FragColor = vec4(vec3(0.78, 0.86, 0.96), cloudAlpha);
        return;
    }

    vec3 surfaceNormal = N;
    if (hasNormalTexture) {
        // Screen derivatives build a local tangent frame for the sampled tangent-space normal.
        vec3 dpdx = dFdx(worldPos);
        vec3 dpdy = dFdy(worldPos);
        vec2 duvdx = dFdx(uv);
        vec2 duvdy = dFdy(uv);
        vec3 tangent = normalize(dpdx * duvdy.y - dpdy * duvdx.y);
        vec3 bitangent = normalize(-dpdx * duvdy.x + dpdy * duvdx.x);
        vec3 mapped = texture(normalTexture, uv).xyz * 2.0 - 1.0;
        surfaceNormal = normalize(mat3(tangent, bitangent, N) * mapped);
    }

    vec3 dayColor = hasDayTexture ? texture(dayTexture, uv).rgb : proceduralLandColor(localPos);
    float cityNoise = fbm(localPos * 72.0 + vec3(31.0, 4.0, 17.0));
    float cityLights = smoothstep(0.68, 0.79, cityNoise) * 2.4;
    vec3 nightColor = hasNightTexture
                          ? texture(nightTexture, uv).rgb * 1.8
                          : dayColor * 0.035 + vec3(1.0, 0.55, 0.15) * cityLights;

    // N dot L is the cosine of the sun angle; smoothstep softens the day/night terminator.
    float dayFactor = smoothstep(-0.12, 0.12, dot(surfaceNormal, L));
    vec3 albedo = mix(nightColor, dayColor, dayFactor);
    float diffuse = max(dot(surfaceNormal, L), 0.0);
    float specularMask = hasSpecularTexture ? texture(specularTexture, uv).r : 0.28;
    float specular = pow(max(dot(surfaceNormal, normalize(L + V)), 0.0), 48.0) * specularMask;
    vec3 lit = albedo * (0.10 + 0.90 * diffuse) + vec3(0.25) * specular;
    FragColor = vec4(lit * opacity, 1.0);
}
