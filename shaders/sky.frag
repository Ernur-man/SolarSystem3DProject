#version 330 core

in vec3 direction;
out vec4 FragColor;

uniform sampler2D starTexture;
uniform bool useTexture;
uniform vec3 skyParallax;

float hash3(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float hash2(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float valueNoise(vec2 p) {
    vec2 cell = floor(p);
    vec2 fraction = fract(p);
    fraction = fraction * fraction * (3.0 - 2.0 * fraction);
    float lower = mix(hash2(cell), hash2(cell + vec2(1.0, 0.0)), fraction.x);
    float upper = mix(hash2(cell + vec2(0.0, 1.0)), hash2(cell + vec2(1.0, 1.0)), fraction.x);
    return mix(lower, upper, fraction.y);
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

    // A Gaussian around the galactic plane modulates a low-frequency value-noise band.
    float bandDistance = latitude + 0.12 * sin(longitude * 2.0);
    float band = exp(-pow(bandDistance * 5.0, 2.0));
    float clouds = valueNoise(vec2(longitude * 2.4, latitude * 5.0));
    vec3 milkyWay = vec3(0.045, 0.052, 0.085) * band * (0.35 + 0.65 * clouds);
    vec3 color = vec3(0.0015, 0.002, 0.006) + milkyWay + star * starColor * brightness;
    FragColor = vec4(color, 1.0);
}
