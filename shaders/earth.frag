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

vec3 proceduralEarth(vec3 p, out float landMask, out float oceanMask) {
    // A low-frequency FBM field gives connected continental shapes instead of isolated spots.
    float continents = fbm(p * 2.7 + vec3(11.0, 2.0, 7.0));
    const float seaLevel = 0.50;
    landMask = smoothstep(seaLevel - 0.025, seaLevel + 0.035, continents);
    oceanMask = 1.0 - landMask;
    float height = smoothstep(seaLevel + 0.02, 0.78, continents);
    float terrain = fbm(p * 15.0 + vec3(3.0, 8.0, 1.0));
    float latitude = abs(p.y);

    // Coast color approaches pale cyan at sea level and deepens offshore.
    float coast = 1.0 - smoothstep(0.0, 0.10, seaLevel - continents);
    vec3 deepOcean = vec3(0.008, 0.035, 0.17);
    vec3 shallowOcean = vec3(0.12, 0.72, 0.82);
    vec3 ocean = mix(deepOcean, shallowOcean, coast * oceanMask);

    vec3 lowland = mix(vec3(0.10, 0.34, 0.12), vec3(0.47, 0.43, 0.21),
                       smoothstep(0.42, 0.68, terrain));
    vec3 highland = mix(vec3(0.37, 0.25, 0.12), vec3(0.68, 0.62, 0.48),
                        smoothstep(0.48, 0.72, terrain));
    vec3 land = mix(lowland, highland, smoothstep(0.18, 0.68, height));
    float dryEquator = (1.0 - smoothstep(0.17, 0.38, latitude)) *
                       smoothstep(0.52, 0.64, terrain);
    land = mix(land, vec3(0.72, 0.58, 0.30), dryEquator * (1.0 - height));
    land = mix(land, vec3(0.92, 0.94, 0.98), smoothstep(0.72, 0.86, height));
    land = mix(land, vec3(0.84, 0.91, 0.97), smoothstep(0.82, 0.94, latitude));
    return mix(ocean, land, landMask);
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

    float landMask = 0.0;
    float oceanMask = 0.0;
    vec3 proceduralColor = proceduralEarth(localPos, landMask, oceanMask);
    vec3 dayColor = hasDayTexture ? texture(dayTexture, uv).rgb : proceduralColor;
    float cityNoise = fbm(localPos * 72.0 + vec3(31.0, 4.0, 17.0));
    float cityLights = smoothstep(0.68, 0.79, cityNoise) * landMask * 2.4;
    vec3 nightColor = hasNightTexture
                          ? texture(nightTexture, uv).rgb * landMask * 1.8
                          : dayColor * 0.035 + vec3(1.0, 0.55, 0.15) * cityLights;

    // N dot L is the cosine of the sun angle; smoothstep softens the day/night terminator.
    float dayFactor = smoothstep(-0.12, 0.12, dot(surfaceNormal, L));
    vec3 albedo = mix(nightColor, dayColor, dayFactor);
    float diffuse = max(dot(surfaceNormal, L), 0.0);
    float specularMap = hasSpecularTexture ? texture(specularTexture, uv).r : 0.42;
    float specularMask = specularMap * oceanMask;
    // Blinn-Phong highlights are restricted to ocean, keeping procedural land matte.
    float specular = pow(max(dot(surfaceNormal, normalize(L + V)), 0.0), 56.0) * specularMask;
    vec3 lit = albedo * (0.10 + 0.90 * diffuse) + vec3(0.42, 0.50, 0.62) * specular;
    FragColor = vec4(lit * opacity, 1.0);
}
