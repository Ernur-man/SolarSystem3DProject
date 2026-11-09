#include "Application.h"
#include "UiTheme.h"
#include "UiPanels.h"

#include <glad/glad.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glm/gtc/matrix_transform.hpp>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <random>
#include <regex>
#include <sstream>
#include <glm/gtc/type_ptr.hpp>

namespace {
constexpr float ORBIT_BASE_SECONDS = 120.0f;
constexpr float ORBIT_COMPRESSION_EXPONENT = 0.5f;
constexpr float ROTATION_SCALE = 0.0207f;
constexpr float EARTH_MOON_ORBIT_RADIUS = 1.02f;
constexpr float EARTH_MOON_RADIUS = 0.105f;
// One full ring-pattern revolution per 25 simulated seconds at 1x.
constexpr float kRingSpinSpeed = 6.2831853f / 25.0f;
// Galactic drift in scene units per simulated second at 1x.
constexpr float kGalaxySpeed = 0.34f;
constexpr float kTrailSampleSeconds = 0.12f;
constexpr std::size_t kTrailCapacity = 1200;
constexpr bool kPanelOnRight = true;
constexpr std::size_t ASTEROID_COUNT_HIGH = 4000;
constexpr std::size_t ASTEROID_COUNT_LOW = 1800;
// Three-radius clearances cross in this stylized system (8.0 > 6.95); use 1.5R margins.
constexpr float ASTEROID_BELT_INNER_RADIUS = 7.1f + 0.30f * 1.5f;
constexpr float ASTEROID_BELT_OUTER_RADIUS = 9.5f - 0.85f * 1.5f;
constexpr float ASTEROID_VERTICAL_SPREAD = 0.20f;
float scrollDelta = 0.0f;
void scrollCallback(GLFWwindow *, double, double y) {
    scrollDelta += static_cast<float>(y);
}

#ifndef NDEBUG
void APIENTRY debugMessageCallback(GLenum, GLenum type, GLuint id, GLenum severity, GLsizei,
                                   const GLchar *message, const void *) {
    std::cerr << "OpenGL debug message (type " << type << ", severity " << severity << ", id " << id
              << "): " << (message ? message : "(no message)") << '\n';
}
#endif

std::string field(const std::string &object, const char *key) {
    const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    return std::regex_search(object, match, pattern) ? match[1].str() : std::string();
}

double numberField(const std::string &object, const char *key) {
    const std::regex pattern(std::string("\\\"") + key +
                             "\\\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)");
    std::smatch match;
    if (!std::regex_search(object, match, pattern)) {
        std::cerr << "Warning: missing JSON field '" << key << "'\n";
        return 0.0;
    }
    return std::stod(match[1].str());
}

std::vector<std::string> stringArray(const std::string &object, const char *key) {
    const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*\\[([^\\]]*)\\]");
    std::smatch match;
    std::vector<std::string> values;
    if (!std::regex_search(object, match, pattern)) {
        std::cerr << "Warning: missing JSON field '" << key << "'\n";
        return values;
    }
    const std::regex strings("\\\"([^\\\"]*)\\\"");
    const std::string contents = match[1].str();
    for (std::sregex_iterator it(contents.begin(), contents.end(), strings), end; it != end; ++it) {
        values.push_back((*it)[1].str());
    }
    return values;
}

int surfaceTextureIndex(const std::string &name) {
    if (name == "Mercury") return 0;
    if (name == "Venus") return 1;
    if (name == "Mars") return 2;
    if (name == "Jupiter") return 3;
    if (name == "Saturn") return 4;
    if (name == "Uranus") return 5;
    if (name == "Neptune") return 6;
    return 7;
}

std::string readText(const char *path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Required data file missing: " << path << '\n';
        return {};
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

float hashEarth(glm::vec3 p) {
    return std::sin(glm::dot(p, glm::vec3(127.1f, 311.7f, 74.7f))) * 43758.5453f -
           std::floor(std::sin(glm::dot(p, glm::vec3(127.1f, 311.7f, 74.7f))) * 43758.5453f);
}

float earthNoise(glm::vec3 p) {
    const glm::vec3 cell(std::floor(p.x), std::floor(p.y), std::floor(p.z));
    glm::vec3 f(p.x - cell.x, p.y - cell.y, p.z - cell.z);
    f = f * f * (glm::vec3(3.0f) - 2.0f * f);
    const float a = glm::mix(hashEarth(cell), hashEarth(cell + glm::vec3(1, 0, 0)), f.x);
    const float b = glm::mix(hashEarth(cell + glm::vec3(0, 1, 0)), hashEarth(cell + glm::vec3(1, 1, 0)), f.x);
    const float c = glm::mix(hashEarth(cell + glm::vec3(0, 0, 1)), hashEarth(cell + glm::vec3(1, 0, 1)), f.x);
    const float d = glm::mix(hashEarth(cell + glm::vec3(0, 1, 1)), hashEarth(cell + glm::vec3(1, 1, 1)), f.x);
    return glm::mix(glm::mix(a, b, f.y), glm::mix(c, d, f.y), f.z);
}

float earthFbm(glm::vec3 p) {
    float value = 0.0f, amplitude = 0.5f;
    for (int i = 0; i < 4; ++i) {
        value += amplitude * earthNoise(p);
        p *= 2.03f;
        amplitude *= 0.5f;
    }
    return value;
}

glm::u8vec3 earthMapColor(float u, float v) {
    const float latitude = (0.5f - v) * 3.14159265f;
    const float longitude = u * 6.2831853f;
    const glm::vec3 p(std::cos(latitude) * std::cos(longitude), std::sin(latitude),
                      std::cos(latitude) * std::sin(longitude));
    const float continents = earthFbm(p * 2.7f + glm::vec3(11, 2, 7));
    const float land = glm::smoothstep(0.475f, 0.535f, continents);
    const float terrain = earthFbm(p * 15.0f + glm::vec3(3, 8, 1));
    const float height = glm::smoothstep(0.52f, 0.78f, continents);
    const float coast = 1.0f - glm::smoothstep(0.0f, 0.10f, 0.50f - continents);
    glm::vec3 color = glm::mix(glm::vec3(0.008f, 0.035f, 0.17f),
                               glm::vec3(0.12f, 0.72f, 0.82f), coast * (1.0f - land));
    const glm::vec3 low = glm::mix(glm::vec3(0.10f, 0.34f, 0.12f), glm::vec3(0.47f, 0.43f, 0.21f),
                                   glm::smoothstep(0.42f, 0.68f, terrain));
    const glm::vec3 high = glm::mix(glm::vec3(0.37f, 0.25f, 0.12f), glm::vec3(0.68f, 0.62f, 0.48f),
                                    glm::smoothstep(0.48f, 0.72f, terrain));
    glm::vec3 landColor = glm::mix(low, high, glm::smoothstep(0.18f, 0.68f, height));
    const float dryEquator = (1.0f - glm::smoothstep(0.17f, 0.38f, std::abs(p.y))) *
                             glm::smoothstep(0.52f, 0.64f, terrain);
    landColor = glm::mix(landColor, glm::vec3(0.72f, 0.58f, 0.30f),
                         dryEquator * (1.0f - height));
    landColor = glm::mix(landColor, glm::vec3(0.92f, 0.94f, 0.98f),
                         glm::smoothstep(0.72f, 0.86f, height));
    landColor = glm::mix(landColor, glm::vec3(0.92f, 0.94f, 0.98f), glm::smoothstep(0.82f, 0.94f, std::abs(p.y)));
    color = glm::mix(color, landColor, land);
    color = glm::clamp(color, glm::vec3(0), glm::vec3(1));
    return glm::u8vec3(color * 255.0f + glm::vec3(0.5f));
}
}

Application::~Application() {
    blackHole.release();
    postProcess.release();
    if (meteorQuadVbo != 0)
        glDeleteBuffers(1, &meteorQuadVbo);
    if (meteorQuadVao != 0)
        glDeleteVertexArrays(1, &meteorQuadVao);
    if (trailVbo != 0) glDeleteBuffers(1, &trailVbo);
    if (trailVao != 0) glDeleteVertexArrays(1, &trailVao);
    if (trailShader.id != 0) glDeleteProgram(trailShader.id);
    if (asteroidInstanceVbo != 0)
        glDeleteBuffers(1, &asteroidInstanceVbo);
    if (asteroidShader.id != 0)
        glDeleteProgram(asteroidShader.id);
    if (meteorShader.id != 0)
        glDeleteProgram(meteorShader.id);
    if (cityPinVbo != 0) glDeleteBuffers(1, &cityPinVbo);
    if (cityPinVao != 0) glDeleteVertexArrays(1, &cityPinVao);
    if (cityPinShader.id != 0) glDeleteProgram(cityPinShader.id);
    if (generatedEarthMap != 0) glDeleteTextures(1, &generatedEarthMap);
    skyTexture.release();
    earthDayTexture.release();
    earthNightTexture.release();
    earthCloudTexture.release();
    earthNormalTexture.release();
    earthSpecularTexture.release();
    saturnRingTexture.release();
    if (window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

void Application::loadCities() {
    const std::string source = readText("assets/cities.json");
    const std::regex objectPattern("\\{[^{}]*\\}");
    for (std::sregex_iterator it(source.begin(), source.end(), objectPattern), end; it != end; ++it) {
        const std::string object = it->str();
        CityMarker city;
        city.name = field(object, "name");
        city.country = field(object, "country");
        city.latitude = static_cast<float>(numberField(object, "lat"));
        city.longitude = static_cast<float>(numberField(object, "lon"));
        city.population = numberField(object, "population");
        if (!city.name.empty() && std::abs(city.latitude) <= 90.0f &&
            std::abs(city.longitude) <= 180.0f)
            cities.push_back(std::move(city));
    }
    std::stable_sort(cities.begin(), cities.end(), [](const CityMarker &a, const CityMarker &b) {
        return a.population > b.population;
    });
    if (cities.size() < 40 || cities.size() > 60)
        std::cerr << "Warning: expected 40-60 Earth city markers, loaded " << cities.size() << '\n';
    std::cout << "Loaded " << cities.size() << " offline Earth city markers. Astana map check: "
              << "u=" << 71.45f / 360.0f << ", v=" << (90.0f - 51.18f) / 180.0f << '\n';
}

void Application::createEarthMapFallback() {
    constexpr int mapWidth = 1024, mapHeight = 512;
    std::vector<glm::u8vec3> pixels(static_cast<std::size_t>(mapWidth * mapHeight));
    for (int y = 0; y < mapHeight; ++y)
        for (int x = 0; x < mapWidth; ++x)
            pixels[static_cast<std::size_t>(y * mapWidth + x)] =
                earthMapColor(static_cast<float>(x) / mapWidth,
                              1.0f - static_cast<float>(y) / mapHeight);
    glGenTextures(1, &generatedEarthMap);
    glBindTexture(GL_TEXTURE_2D, generatedEarthMap);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, mapWidth, mapHeight, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 pixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

glm::mat4 Application::earthModelMatrix() const {
    const BodyEffect effect = bodyEffect(2);
    const float spin = simTime * 6.2831853f * ROTATION_SCALE / bodies[2].rotationPeriodDays;
    const glm::vec3 scale(bodies[2].radius * effect.perpendicularScale,
                          bodies[2].radius * effect.perpendicularScale,
                          bodies[2].radius * effect.axialScale);
    return glm::translate(glm::mat4(1.0f), effect.position) * glm::mat4(effect.basis) *
           glm::rotate(glm::mat4(1.0f), spin, glm::vec3(0, 1, 0)) *
           glm::scale(glm::mat4(1.0f), scale);
}

void Application::drawCityPins(const glm::mat4 &view, const glm::mat4 &projection) {
    if (!cityMarkersEnabled || selected != 2 || cities.empty() || bodyEffect(2).swallowed)
        return;
    const glm::mat4 model = earthModelMatrix();
    const glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
    const glm::vec3 cameraDirection = glm::normalize(currentCamera - bodyEffect(2).position);
    const float opacity = bodyEffect(2).opacity;
    std::vector<CityPinVertex> pins;
    pins.reserve(cities.size());
    for (const CityMarker &city : cities) {
        const float lat = glm::radians(city.latitude), lon = glm::radians(city.longitude);
        const glm::vec3 local(std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon));
        const glm::vec3 world = glm::vec3(model * glm::vec4(local * 1.025f, 1.0f));
        const glm::vec3 normal = glm::normalize(normalMatrix * local);
        if (glm::dot(normal, glm::normalize(currentCamera - world)) <= 0.0f || opacity <= 0.01f)
            continue;
        pins.push_back({world, opacity});
    }
    if (pins.empty()) return;
    GLint oldProgram = 0, oldVao = 0;
    GLint oldBlendSrcRgb = GL_ONE, oldBlendDstRgb = GL_ZERO;
    GLint oldBlendSrcAlpha = GL_ONE, oldBlendDstAlpha = GL_ZERO;
    GLboolean oldDepthWrite = GL_TRUE, oldBlend = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
    glGetIntegerv(GL_BLEND_SRC_RGB, &oldBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &oldBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &oldBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &oldBlendDstAlpha);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &oldDepthWrite);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    cityPinShader.use();
    cityPinShader.set("view", view);
    cityPinShader.set("projection", projection);
    glBindVertexArray(cityPinVao);
    glBindBuffer(GL_ARRAY_BUFFER, cityPinVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(pins.size() * sizeof(CityPinVertex)),
                 pins.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(pins.size()));
    glBindVertexArray(static_cast<GLuint>(oldVao));
    glUseProgram(static_cast<GLuint>(oldProgram));
    glDepthMask(oldDepthWrite);
    if (!oldBlend) glDisable(GL_BLEND);
    glBlendFuncSeparate(static_cast<GLenum>(oldBlendSrcRgb), static_cast<GLenum>(oldBlendDstRgb),
                        static_cast<GLenum>(oldBlendSrcAlpha), static_cast<GLenum>(oldBlendDstAlpha));
}

void Application::drawCityLabels(const glm::mat4 &view, const glm::mat4 &projection) {
    if (!cityMarkersEnabled || selected != 2 || distance > 5.0f || cities.empty()) return;
    const glm::mat4 model = earthModelMatrix();
    const glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
    const glm::vec3 cameraDirection = glm::normalize(currentCamera - bodyEffect(2).position);
    const int maxLabels = highQuality ? 8 : 4;
    int shown = 0;
    ImDrawList *drawList = ImGui::GetBackgroundDrawList();
    for (const CityMarker &city : cities) {
        const float lat = glm::radians(city.latitude), lon = glm::radians(city.longitude);
        const glm::vec3 local(std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon));
        const glm::vec3 world = glm::vec3(model * glm::vec4(local * 1.025f, 1.0f));
        const glm::vec3 normal = glm::normalize(normalMatrix * local);
        if (glm::dot(normal, glm::normalize(currentCamera - world)) <= 0.15f ||
            glm::dot(glm::normalize(world - bodyEffect(2).position), cameraDirection) < 0.25f)
            continue;
        const glm::vec4 clip = projection * view * glm::vec4(world, 1.0f);
        if (clip.w <= 0.0f) continue;
        const float nx = clip.x / clip.w, ny = clip.y / clip.w;
        if (std::abs(nx) > 1.0f || std::abs(ny) > 1.0f) continue;
        const float fade = std::clamp((5.0f - distance) / 2.5f, 0.0f, 1.0f);
        const ImVec2 point((nx * 0.5f + 0.5f) * windowWidth, (0.5f - ny * 0.5f) * windowHeight);
        drawList->AddText(point, IM_COL32(255, 220, 128, static_cast<int>(230.0f * fade)),
                          city.name.c_str());
        if (++shown >= maxLabels) break;
    }
}

bool Application::loadData() {
    const std::string planetsText = readText("assets/planets.json");
    if (planetsText.empty())
        return false;
    const std::regex objectPattern("\\{[^{}]*\\}");
    std::vector<std::string> planetObjects;
    for (std::sregex_iterator it(planetsText.begin(), planetsText.end(), objectPattern), end;
         it != end; ++it) {
        planetObjects.push_back(it->str());
    }
    for (Body &body : bodies) {
        const auto found =
            std::find_if(planetObjects.begin(), planetObjects.end(),
                         [&](const std::string &item) { return field(item, "name") == body.name; });
        if (found == planetObjects.end()) {
            std::cerr << "Warning: no JSON record for " << body.name << '\n';
            continue;
        }
        const std::string &item = *found;
        body.type = field(item, "type");
        if (body.type.empty())
            std::cerr << "Warning: missing JSON field 'type' for " << body.name << '\n';
        body.diameterKm = numberField(item, "diameter_km");
        body.massKg = numberField(item, "mass_kg");
        body.distanceMillionKm = numberField(item, "distance_million_km");
        body.orbitalDays = numberField(item, "orbital_period_days");
        body.rotationPeriodDays = static_cast<float>(numberField(item, "rotation_period_days"));
        body.gravity = numberField(item, "gravity_m_s2");
        body.moons = static_cast<int>(numberField(item, "moons"));
        body.facts = stringArray(item, "facts");
    }
    const auto sun =
        std::find_if(planetObjects.begin(), planetObjects.end(),
                     [](const std::string &item) { return field(item, "name") == "Sun"; });
    if (sun == planetObjects.end()) {
        std::cerr << "Warning: no JSON record for Sun\n";
        return false;
    }
    sunBody.name = "Sun";
    sunBody.type = field(*sun, "type");
    if (sunBody.type.empty())
        std::cerr << "Warning: missing JSON field 'type' for Sun\n";
    sunBody.diameterKm = numberField(*sun, "diameter_km");
    sunBody.massKg = numberField(*sun, "mass_kg");
    sunBody.distanceMillionKm = numberField(*sun, "distance_million_km");
    sunBody.orbitalDays = numberField(*sun, "orbital_period_days");
    sunBody.rotationPeriodDays = static_cast<float>(numberField(*sun, "rotation_period_days"));
    sunBody.gravity = numberField(*sun, "gravity_m_s2");
    sunBody.moons = static_cast<int>(numberField(*sun, "moons"));
    sunBody.facts = stringArray(*sun, "facts");

    const std::string quizText = readText("assets/quiz.json");
    if (quizText.empty())
        return false;
    for (std::sregex_iterator it(quizText.begin(), quizText.end(), objectPattern), end; it != end;
         ++it) {
        const std::string item = it->str();
        const std::vector<std::string> choices = stringArray(item, "choices");
        QuizQuestion question;
        question.planet = field(item, "planet");
        question.question = field(item, "question");
        question.answer = field(item, "answer");
        if (choices.size() != 4 || question.planet.empty() || question.question.empty() ||
            question.answer.empty()) {
            std::cerr << "Warning: invalid quiz question record\n";
            continue;
        }
        std::copy(choices.begin(), choices.end(), question.choices.begin());
        questions.push_back(question);
    }
    for (QuizRandomizer::QuestionBag &bag : quizQuestionBags)
        bag.pool.clear();
    for (std::size_t questionIndex = 0; questionIndex < questions.size(); ++questionIndex) {
        const QuizQuestion &question = questions[questionIndex];
        std::size_t worldIndex = 8;
        if (question.planet != "Sun") {
            const auto body = std::find_if(bodies.begin(), bodies.end(),
                                           [&](const Body &entry) {
                                               return entry.name == question.planet;
                                           });
            if (body == bodies.end())
                continue;
            worldIndex = static_cast<std::size_t>(std::distance(bodies.begin(), body));
        }
        quizQuestionBags[worldIndex].pool.push_back(questionIndex);
    }
    for (const std::string &name :
         {"Sun", "Mercury", "Venus", "Earth", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune"}) {
        std::vector<std::string> uniqueQuestions;
        for (const QuizQuestion &q : questions)
            if (q.planet == name)
                uniqueQuestions.push_back(q.question);
        std::sort(uniqueQuestions.begin(), uniqueQuestions.end());
        const auto uniqueEnd = std::unique(uniqueQuestions.begin(), uniqueQuestions.end());
        if (static_cast<std::size_t>(std::distance(uniqueQuestions.begin(), uniqueEnd)) < 6)
            std::cerr << "Warning: fewer than 6 unique quiz questions for " << name << '\n';
    }
    return true;
}

bool Application::init() {
    if (!glfwInit())
        return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window =
        glfwCreateWindow(width, height, "Event Horizon: Solar System Explorer", nullptr, nullptr);
    if (!window)
        return false;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetScrollCallback(window, scrollCallback);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        return false;
#ifndef NDEBUG
    if ((GLAD_GL_VERSION_4_3 || glfwExtensionSupported("GL_KHR_debug")) &&
        glDebugMessageCallback != nullptr) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(debugMessageCallback, nullptr);
        std::cout << "OpenGL debug output callback enabled.\n";
    } else {
        std::cerr << "OpenGL debug output is unavailable on this context.\n";
    }
#endif
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    sphere.makeSphere(32, 48);
    applyQualitySettings();
    highlightRing.makeOrbit(1.0f);
    if (!shader.load("shaders/planet.vert", "shaders/planet.frag") ||
        !sunShader.load("shaders/planet.vert", "shaders/sun.frag") ||
        !earthShader.load("shaders/earth.vert", "shaders/earth.frag") ||
        !atmosphereShader.load("shaders/atmosphere.vert", "shaders/atmosphere.frag") ||
        !ringShader.load("shaders/ring.vert", "shaders/ring.frag") ||
        !skyShader.load("shaders/sky.vert", "shaders/sky.frag") ||
        !orbitShader.load("shaders/orbit.vert", "shaders/orbit.frag") ||
        !trailShader.load("shaders/trail.vert", "shaders/trail.frag") ||
        !asteroidShader.load("shaders/asteroid.vert", "shaders/asteroid.frag") ||
        !meteorShader.load("shaders/meteor.vert", "shaders/meteor.frag") ||
        !cityPinShader.load("shaders/city_pin.vert", "shaders/city_pin.frag"))
        return false;
    glGenVertexArrays(1, &trailVao);
    glGenBuffers(1, &trailVbo);
    glBindVertexArray(trailVao);
    glBindBuffer(GL_ARRAY_BUFFER, trailVbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(TrailVertex),
                          reinterpret_cast<void *>(offsetof(TrailVertex, color)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(TrailVertex),
                          reinterpret_cast<void *>(offsetof(TrailVertex, edge)));
    glBindVertexArray(0);
    for (GalaxyTrail &trail : galaxyTrails) trail.points.resize(kTrailCapacity);
    glGenVertexArrays(1, &cityPinVao);
    glGenBuffers(1, &cityPinVbo);
    glBindVertexArray(cityPinVao);
    glBindBuffer(GL_ARRAY_BUFFER, cityPinVbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(CityPinVertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(CityPinVertex), reinterpret_cast<void *>(offsetof(CityPinVertex, opacity)));
    glBindVertexArray(0);
    skyTexture.load("assets/textures/stars_milky_way.jpg", true, "procedural starfield");
    earthDayTexture.load("assets/textures/earth_day.jpg", true, "procedural Earth surface");
    earthNightTexture.load("assets/textures/earth_night.jpg", true, "procedural city lights");
    earthCloudTexture.load("assets/textures/earth_clouds.jpg", true, "procedural clouds");
    earthNormalTexture.load("assets/textures/earth_normal.jpg", false, "geometric normals");
    earthSpecularTexture.load("assets/textures/earth_specular.jpg", false,
                              "constant ocean and land specular values");
    saturnRingTexture.load("assets/textures/saturn_ring.png", true, "procedural ring bands");
    const std::array<const char *, 8> worldTexturePaths = {
        "assets/textures/mercury.jpg", "assets/textures/venus.jpg", "assets/textures/mars.jpg",
        "assets/textures/jupiter.jpg", "assets/textures/saturn.jpg", "assets/textures/uranus.jpg",
        "assets/textures/neptune.jpg", "assets/textures/moon.jpg"};
    for (std::size_t i = 0; i < worldTextures.size(); ++i)
        worldTextures[i].load(worldTexturePaths[i], true, "procedural surface");
    loadCities();
    if (!earthDayTexture.isLoaded()) createEarthMapFallback();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    UiTheme::setup(window);
    bodies = {{"Mercury", 3, 0.22f, 58.6f, {0.62f, 0.57f, 0.52f}},
              {"Venus", 4.2f, 0.34f, 243.0f, {0.86f, 0.62f, 0.31f}},
              {"Earth", 5.7f, 0.38f, 0.997f, {0.14f, 0.42f, 0.93f}},
              {"Mars", 7.1f, 0.30f, 1.026f, {0.85f, 0.27f, 0.12f}},
              {"Jupiter", 9.5f, 0.85f, 0.414f, {0.82f, 0.56f, 0.37f}},
              {"Saturn", 12.3f, 0.72f, 0.444f, {0.85f, 0.76f, 0.53f}},
              {"Uranus", 15, 0.58f, 0.718f, {0.42f, 0.82f, 0.84f}},
              {"Neptune", 17.5f, 0.56f, 0.671f, {0.22f, 0.34f, 0.9f}}};
    if (!loadData())
        return false;
    for (const Body &body : bodies) {
        orbitLines.emplace_back();
        orbitLines.back().makeOrbit(body.orbit);
    }
    earthMoonOrbitLine.makeOrbit(EARTH_MOON_ORBIT_RADIUS);
    glfwGetFramebufferSize(window, &width, &height);
    if (!postProcess.initialize(width, height, highQuality ? 1.0f : 0.75f))
        return false;
    saturnRing.makeRing(1.35f, 2.6f);
    if (!blackHole.initialize(postProcess.getWidth(), postProcess.getHeight()))
        return false;

    const float meteorQuad[] = {-1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f,
                                1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f};
    glGenVertexArrays(1, &meteorQuadVao);
    glGenBuffers(1, &meteorQuadVbo);
    glBindVertexArray(meteorQuadVao);
    glBindBuffer(GL_ARRAY_BUFFER, meteorQuadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(meteorQuad), meteorQuad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    std::mt19937 random(std::random_device{}());
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> speedRange(0.20f, 0.45f);
    std::uniform_real_distribution<float> radiusRange(0.035f, 0.075f);
    meteors.reserve(36);
    for (int i = 0; i < 36; ++i) {
        glm::vec3 direction(unit(random) - 0.5f, (unit(random) - 0.5f) * 0.2f,
                            unit(random) - 0.5f);
        direction = glm::normalize(direction);
        const glm::vec3 start = -direction * (26.0f + unit(random) * 16.0f) +
                                glm::vec3((unit(random) - 0.5f) * 14.0f,
                                          (unit(random) - 0.5f) * 8.0f,
                                          (unit(random) - 0.5f) * 14.0f);
        meteors.push_back({start, direction,
                           glm::vec3(0.8f + unit(random) * 0.2f,
                                     0.48f + unit(random) * 0.36f, 0.2f + unit(random) * 0.2f),
                           unit(random), speedRange(random), radiusRange(random)});
    }

    asteroidMesh.makeSphere(6, 8);
    glGenBuffers(1, &asteroidInstanceVbo);
    glBindVertexArray(asteroidMesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, asteroidInstanceVbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    for (GLuint column = 0; column < 4; ++column) {
        glEnableVertexAttribArray(3 + column);
        glVertexAttribPointer(3 + column, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                              reinterpret_cast<void *>(static_cast<std::uintptr_t>(column * 4 * sizeof(float))));
        glVertexAttribDivisor(3 + column, 1);
    }
    glBindVertexArray(0);
    std::uniform_real_distribution<float> orbitRange(ASTEROID_BELT_INNER_RADIUS,
                                                       ASTEROID_BELT_OUTER_RADIUS);
    std::uniform_real_distribution<float> asteroidSize(0.018f, 0.055f);
    std::uniform_real_distribution<float> angularRange(0.008f, 0.022f);
    std::uniform_real_distribution<float> verticalRange(-ASTEROID_VERTICAL_SPREAD,
                                                          ASTEROID_VERTICAL_SPREAD);
    asteroids.reserve(ASTEROID_COUNT_HIGH);
    for (std::size_t i = 0; i < ASTEROID_COUNT_HIGH; ++i) {
        const float shade = 0.35f + unit(random) * 0.38f;
        asteroids.push_back({orbitRange(random), unit(random) * 6.2831853f,
                             angularRange(random), verticalRange(random), asteroidSize(random),
                             glm::vec3(shade, shade * 0.91f, shade * 0.78f)});
    }
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    return true;
}

void Application::framebufferSizeCallback(GLFWwindow *window, int framebufferWidth,
                                          int framebufferHeight) {
    auto *application = static_cast<Application *>(glfwGetWindowUserPointer(window));
    if (!application)
        return;
    application->width = framebufferWidth;
    application->height = framebufferHeight;
    glfwGetWindowSize(window, &application->windowWidth, &application->windowHeight);
    if (framebufferWidth == 0 || framebufferHeight == 0)
        return;

    glViewport(0, 0, framebufferWidth, framebufferHeight);
    // PostProcess::resize recreates the HDR scene and bloom ping-pong targets.
    if (!application->postProcess.resize(framebufferWidth, framebufferHeight,
                                         application->highQuality ? 1.0f : 0.75f)) {
        std::cerr << "Failed to resize HDR scene targets to " << framebufferWidth << 'x'
                  << framebufferHeight << '\n';
        return;
    }
    // BlackHole::resize recreates the lensing output target at the HDR scene resolution.
    if (!application->blackHole.resize(application->postProcess.getWidth(),
                                        application->postProcess.getHeight()))
        std::cerr << "Failed to resize black-hole lensing target.\n";
}

void Application::applyQualitySettings() {
    lowDetailSphere.makeSphere(highQuality ? 12u : 8u, highQuality ? 18u : 12u);
}

glm::vec3 Application::cameraPosition() const {
    const float cameraDistance = distance + galaxyBlend * overviewDistance * 1.35f;
    return cameraTarget + glm::vec3(cameraDistance * std::cos(pitch) * std::sin(yaw),
                                    cameraDistance * std::sin(pitch),
                                    cameraDistance * std::cos(pitch) * std::cos(yaw));
}

glm::vec3 Application::galaxyDisplacement() const {
    // The direction is 60 degrees above the ecliptic plane (the XZ plane).
    return glm::normalize(glm::vec3(0.5f, 0.8660254f, 0.0f)) *
           (kGalaxySpeed * galaxyTime * galaxyBlend);
}

void Application::resetGalaxyView() {
    galaxyViewTarget = false; galaxyBlend = 0.0f; galaxyTransition = 0.0f;
    galaxyTime = 0.0f; trailSampleClock = 0.0f;
    for (GalaxyTrail &trail : galaxyTrails) { trail.head = 0; trail.count = 0; }
}

void Application::updateGalaxy(float simulationDelta) {
    const float oldBlend = galaxyBlend;
    if (simulationDelta <= 0.0f) return;
    if (gameState == GameState::FreeExplore && galaxyViewTarget) galaxyTime += simulationDelta;
    const float direction = galaxyViewTarget ? 1.0f : -1.0f;
    galaxyTransition = std::clamp(galaxyTransition + direction * simulationDelta / 2.0f, 0.0f, 1.0f);
    galaxyBlend = galaxyTransition * galaxyTransition * (3.0f - 2.0f * galaxyTransition);
    if (galaxyViewTarget && gameState == GameState::FreeExplore && galaxyBlend > 0.0f) {
        const float interval = kTrailSampleSeconds;
        float timeToSample = interval - trailSampleClock;
        while (timeToSample <= simulationDelta) {
            const float fraction = timeToSample / simulationDelta;
            const float sampleTime = simTime - simulationDelta + timeToSample;
            const float sampleGalaxyTime = galaxyTime - simulationDelta + timeToSample;
            const float sampleBlend = glm::mix(oldBlend, galaxyBlend, fraction);
            const glm::vec3 offset = glm::normalize(glm::vec3(0.5f, 0.8660254f, 0.0f)) *
                                     (kGalaxySpeed * sampleGalaxyTime * sampleBlend);
            for (int i = 0; i < 10; ++i) {
                glm::vec3 position = bodyPosition(i, sampleTime);
                if (i == 9) {
                    const float moonAngle = sampleTime * 6.2831853f * 12.0f / ORBIT_BASE_SECONDS;
                    position = bodyPosition(2, sampleTime) + glm::vec3(
                        EARTH_MOON_ORBIT_RADIUS * std::cos(moonAngle), 0.0f,
                        EARTH_MOON_ORBIT_RADIUS * std::sin(moonAngle));
                }
                GalaxyTrail &trail = galaxyTrails[static_cast<std::size_t>(i)];
                trail.points[trail.head].position = offset + position;
                trail.head = (trail.head + 1) % trail.points.size();
                const std::size_t activeCapacity = highQuality ? kTrailCapacity : 500u;
                trail.count = std::min(trail.count + 1, activeCapacity);
            }
            timeToSample += interval;
        }
        trailSampleClock = std::fmod(trailSampleClock + simulationDelta, interval);
    }
    if (galaxyTransition == 0.0f && !galaxyViewTarget)
        for (GalaxyTrail &trail : galaxyTrails) { trail.head = 0; trail.count = 0; }
}

void Application::drawGalaxyTrails(const glm::mat4 &view, const glm::mat4 &projection) {
    if (galaxyBlend <= 0.001f) return;
    trailVertices.clear();
    const glm::vec3 camera = currentCamera;
    const std::array<glm::vec3, 10> colors = {
        glm::vec3(.20f,.52f,1), glm::vec3(.25f,.48f,1), glm::vec3(.15f,.90f,1),
        glm::vec3(.50f,.30f,1), glm::vec3(.65f,.42f,1), glm::vec3(.90f,.48f,.28f),
        glm::vec3(.30f,.62f,1), glm::vec3(.22f,.38f,1), glm::vec3(.35f,.65f,1),
        glm::vec3(.18f,.88f,1)};
    const glm::vec3 offset = galaxyDisplacement();
    for (std::size_t i = 0; i < galaxyTrails.size(); ++i) {
        const GalaxyTrail &trail = galaxyTrails[i];
        const std::size_t count = std::min(trail.count, highQuality ? kTrailCapacity : 500u);
        if (count < 2) continue;
        for (std::size_t p = 1; p < count; ++p) {
            const std::size_t oldIndex = (trail.head + trail.points.size() - count + p - 1) % trail.points.size();
            const std::size_t newIndex = (oldIndex + 1) % trail.points.size();
            const glm::vec3 a = trail.points[oldIndex].position - offset;
            const glm::vec3 b = trail.points[newIndex].position - offset;
            const glm::vec3 tangent = glm::normalize(b - a);
            glm::vec3 side = glm::cross(tangent, camera - (a + b) * .5f);
            if (glm::length(side) < .001f) side = glm::cross(tangent, glm::vec3(0,1,0));
            side = glm::normalize(side) * .045f;
            const float age = 1.0f - static_cast<float>(p - 1) /
                                           static_cast<float>(count - 1);
            const float alpha = (1.0f-age)*(1.0f-age)*galaxyBlend;
            const float light = 0.35f + 2.0f * (1.0f-age);
            const glm::vec4 color(colors[i] * light, alpha);
            const TrailVertex al{a-side,color,-1}, ar{a+side,color,1}, bl{b-side,color,-1}, br{b+side,color,1};
            trailVertices.insert(trailVertices.end(), {al, ar, bl, ar, br, bl});
        }
    }
    if (trailVertices.empty()) return;
    GLboolean oldDepthWrite = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &oldDepthWrite);
    const GLboolean oldBlend = glIsEnabled(GL_BLEND);
    GLint oldSrcRgb = GL_ONE, oldDstRgb = GL_ZERO, oldSrcAlpha = GL_ONE, oldDstAlpha = GL_ZERO;
    GLint oldProgram = 0, oldVao = 0, oldArrayBuffer = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldArrayBuffer);
    glGetIntegerv(GL_BLEND_SRC_RGB, &oldSrcRgb); glGetIntegerv(GL_BLEND_DST_RGB, &oldDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &oldSrcAlpha); glGetIntegerv(GL_BLEND_DST_ALPHA, &oldDstAlpha);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE);
    trailShader.use(); trailShader.set("view", view); trailShader.set("projection", projection);
    glBindVertexArray(trailVao); glBindBuffer(GL_ARRAY_BUFFER, trailVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(trailVertices.size()*sizeof(TrailVertex)), trailVertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(trailVertices.size()));
    glBindVertexArray(static_cast<GLuint>(oldVao));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(oldArrayBuffer));
    glUseProgram(static_cast<GLuint>(oldProgram));
    glDepthMask(oldDepthWrite);
    glBlendFuncSeparate(oldSrcRgb, oldDstRgb, oldSrcAlpha, oldDstAlpha);
    if (!oldBlend) glDisable(GL_BLEND);
}

glm::vec3 Application::bodyPosition(int index, float time) const {
    if (index == 8 || index < 0)
        return glm::vec3(0);
    const Body &body = bodies[static_cast<std::size_t>(index)];
    const float yearDays = static_cast<float>(body.orbitalDays / 365.256);
    const float period = ORBIT_BASE_SECONDS * std::pow(yearDays, ORBIT_COMPRESSION_EXPONENT);
    const float angle = time * 6.2831853f / period;
    return {body.orbit * std::cos(angle), 0, body.orbit * std::sin(angle)};
}

glm::vec3 Application::earthMoonPosition() const {
    constexpr float twoPi = 6.2831853f;
    const float earthPeriod = ORBIT_BASE_SECONDS;
    const float moonAngle = simTime * twoPi * 12.0f / earthPeriod;
    return bodyPosition(2, simTime) +
           glm::vec3(EARTH_MOON_ORBIT_RADIUS * std::cos(moonAngle), 0.0f,
                     EARTH_MOON_ORBIT_RADIUS * std::sin(moonAngle));
}

void Application::selectBody(int index) {
    if (index < 0 || index > 8)
        return;
    const bool fromOverview = selected < 0;
    earthMapTargetActive = false;
    if (fromOverview)
        overviewDistance = distance;
    selected = index;
    recordVisit(index);
    startCamera = currentCamera;
    startTarget = cameraTarget;
    if (fromOverview)
        distance = 7.0f;
    flyCameraOffset =
        glm::vec3(distance * std::cos(pitch) * std::sin(yaw), distance * std::sin(pitch),
                  distance * std::cos(pitch) * std::cos(yaw));
    flyElapsed = 0;
    flying = true;
}

void Application::beginQuiz() {
    const std::string name =
        selected == 8 ? "Sun" : bodies[static_cast<std::size_t>(selected)].name;
    activeQuizSeed = QuizRandomizer::nextSeed(quizSeedSequence);
    quizRandom.seed(activeQuizSeed);
    std::cout << "Quiz seed for " << name << ": " << activeQuizSeed << '\n';
    QuizRandomizer::QuestionBag &bag = quizQuestionBags[static_cast<std::size_t>(selected)];
    if (bag.pool.size() < 5)
        std::cerr << "Warning: fewer than five quiz questions available for " << name << '\n';
    QuizRandomizer::prepareQuizRound(bag, quizRandom, 5, quizRound);
    quizQuestionIndex = 0;
    quizScore = 0;
    quizChoice = -1;
    quizAnswered = false;
    quizFinished = quizRound.empty();
    if (!quizFinished)
        shuffleDisplayedChoices();
    quizWasPaused = paused;
    quizOpen = true;
}

void Application::initializeQuizBags() {
    activeQuizSeed = QuizRandomizer::nextSeed(quizSeedSequence);
    quizRandom.seed(activeQuizSeed);
    std::cout << "Mission quiz seed: " << activeQuizSeed << '\n';
    for (QuizRandomizer::QuestionBag &bag : quizQuestionBags)
        QuizRandomizer::startMissionBag(bag, quizRandom);
}

void Application::shuffleDisplayedChoices() {
    if (quizQuestionIndex >= quizRound.size())
        return;
    const QuizQuestion &question = questions[quizRound[quizQuestionIndex]];
    quizDisplayChoices = QuizRandomizer::shuffleOptions(question.choices, quizRandom);
}

void Application::startMission(bool summonHole) {
    resetGalaxyView();
    gameState = GameState::Mission;
    missionScore = 0;
    visitedBodies.fill(false);
    blackHole.reset();
    fallbackThreat.reset();
    threatSpeedMultiplier = 1.0f;
    simTime = 0.0f;
    speed = 1;
    paused = false;
    selected = -1;
    yaw = 0.0f;
    pitch = 0.25f;
    distance = 22.0f;
    overviewDistance = distance;
    currentCamera = glm::vec3(0, 0, 22);
    cameraTarget = glm::vec3(0);
    startCamera = currentCamera;
    startTarget = cameraTarget;
    flyCameraOffset = glm::vec3(0, 3, 7);
    freeForward = glm::vec3(0, 0, -1);
    flying = false;
    freeFly = false;
    quizOpen = false;
    quizWasPaused = false;
    quizAnswered = false;
    quizFinished = false;
    quizQuestionIndex = 0;
    quizScore = 0;
    quizChoice = -1;
    initializeQuizBags();
    flyElapsed = 0.0f;
    lightAngle = 0.0f;
    bloom = true;
    lensing = true;
    instancing = true;
    highQuality = true;
    meteorsEnabled = true;
    freeExploreSwallowedDismissed = false;
    applyQualitySettings();
    blackHole.setThreatSpeedMultiplier(threatSpeedMultiplier);
    fallbackThreat.setThreatSpeedMultiplier(threatSpeedMultiplier);
    if (summonHole)
        blackHole.summon();
    help = false;
}

void Application::startFreeExplore() {
    startMission(false);
    gameState = GameState::FreeExplore;
}

void Application::returnToMenu() {
    startMission(false);
    gameState = GameState::StartScreen;
}

void Application::updateMission(float deltaTime) {
    if (gameState != GameState::Mission)
        return;
    activeThreat().update(deltaTime);
    if (activeThreat().isSunSwallowed()) {
        gameState = GameState::Lost;
        quizOpen = false;
        paused = true;
    }
}

ThreatInterface &Application::activeThreat() {
    if (useFallbackThreat)
        return fallbackThreat;
    return blackHole;
}

BodyEffect Application::bodyEffect(int index) const {
    const bool sun = index == 8;
    return blackHole.affectBody(bodyPosition(index, simTime), sun);
}

void Application::recordVisit(int index) {
    if (gameState != GameState::Mission || index < 0 ||
        index >= static_cast<int>(visitedBodies.size()))
        return;
    const std::size_t bodyIndex = static_cast<std::size_t>(index);
    if (!visitedBodies[bodyIndex]) {
        visitedBodies[bodyIndex] = true;
        missionScore += 5;
        if (missionScore >= MISSION_TARGET_SCORE) {
            gameState = GameState::Won;
            quizOpen = false;
            paused = true;
        }
    }
}

void Application::recordQuizAnswer(bool correct) {
    if (gameState != GameState::Mission)
        return;
    if (correct) {
        missionScore += 10;
        threatSpeedMultiplier = std::max(0.25f, threatSpeedMultiplier * 0.8f);
    } else {
        threatSpeedMultiplier = std::min(4.0f, threatSpeedMultiplier * 1.25f);
    }
    blackHole.setThreatSpeedMultiplier(threatSpeedMultiplier);
    fallbackThreat.setThreatSpeedMultiplier(threatSpeedMultiplier);
    if (missionScore >= MISSION_TARGET_SCORE) {
        gameState = GameState::Won;
        quizOpen = false;
        paused = true;
    }
}

void Application::saveScreenshot() {
    std::error_code error;
    std::filesystem::create_directories("screenshots", error);
    if (error) {
        std::cerr << "Could not create screenshots directory: " << error.message() << '\n';
        return;
    }
    const std::string path = "screenshots/solar_system_" +
                             std::to_string(static_cast<long long>(std::time(nullptr))) + "_" +
                             std::to_string(screenshotIndex++) + ".png";
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
    std::vector<unsigned char> pixels(rowBytes * static_cast<std::size_t>(height));
    GLint oldPackAlignment = 4;
    GLint oldReadBuffer = GL_BACK;
    glGetIntegerv(GL_PACK_ALIGNMENT, &oldPackAlignment);
    glGetIntegerv(GL_READ_BUFFER, &oldReadBuffer);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, oldPackAlignment);
    glReadBuffer(static_cast<GLenum>(oldReadBuffer));
    for (int row = 0; row < height / 2; ++row) {
        const std::size_t top = static_cast<std::size_t>(row) * rowBytes;
        const std::size_t bottom = static_cast<std::size_t>(height - row - 1) * rowBytes;
        std::swap_ranges(pixels.begin() + static_cast<std::ptrdiff_t>(top),
                         pixels.begin() + static_cast<std::ptrdiff_t>(top + rowBytes),
                         pixels.begin() + static_cast<std::ptrdiff_t>(bottom));
    }
    if (stbi_write_png(path.c_str(), width, height, 4, pixels.data(), static_cast<int>(rowBytes))) {
        std::cout << "Screenshot saved: " << path << '\n';
    } else {
        std::cerr << "Could not save screenshot: " << path << '\n';
    }
}

void Application::draw(const Body &body, const glm::mat4 &view, const glm::mat4 &projection,
                       bool sun) {
    const int index = sun ? 8 : static_cast<int>(&body - bodies.data());
    const BodyEffect effect = bodyEffect(index);
    if (effect.swallowed)
        return;
    const float bodyRadius = sun ? 1.25f : body.radius;
    const float rotationAngle = simTime * 6.2831853f * ROTATION_SCALE / body.rotationPeriodDays;
    const glm::mat4 axialTilt = !sun && body.name == "Saturn"
                                    ? glm::rotate(glm::mat4(1.0f), glm::radians(27.0f),
                                                  glm::vec3(1, 0, 0))
                                    : glm::mat4(1.0f);
    const glm::mat4 baseTransform =
        glm::translate(glm::mat4(1.0f), effect.position) * glm::mat4(effect.basis) * axialTilt *
        glm::rotate(glm::mat4(1.0f), rotationAngle, glm::vec3(0, 1, 0));
    const glm::vec3 bodyScale(bodyRadius * effect.perpendicularScale,
                              bodyRadius * effect.perpendicularScale,
                              bodyRadius * effect.axialScale);
    const glm::mat4 model = baseTransform * glm::scale(glm::mat4(1.0f), bodyScale);
    const glm::vec3 lightPosition(20.0f * (std::cos(lightAngle) - 1.0f), 0.0f,
                                  20.0f * std::sin(lightAngle));

    if (!sun && body.name == "Earth") {
        earthShader.use();
        earthShader.set("model", model);
        earthShader.set("view", view);
        earthShader.set("projection", projection);
        earthShader.set("lightPos", lightPosition);
        earthShader.set("cameraPos", currentCamera);
        earthShader.set("opacity", effect.opacity);
        earthShader.set("hasDayTexture", earthDayTexture.isLoaded() ? 1 : 0);
        earthShader.set("hasNightTexture", earthNightTexture.isLoaded() ? 1 : 0);
        earthShader.set("hasNormalTexture", earthNormalTexture.isLoaded() ? 1 : 0);
        earthShader.set("hasSpecularTexture", earthSpecularTexture.isLoaded() ? 1 : 0);
        earthShader.set("hasCloudTexture", earthCloudTexture.isLoaded() ? 1 : 0);
        earthShader.set("dayTexture", 0);
        earthShader.set("nightTexture", 1);
        earthShader.set("cloudTexture", 2);
        earthShader.set("normalTexture", 3);
        earthShader.set("specularTexture", 4);
        earthDayTexture.bind(0);
        earthNightTexture.bind(1);
        earthCloudTexture.bind(2);
        earthNormalTexture.bind(3);
        earthSpecularTexture.bind(4);
        earthShader.set("cloudPass", 0);
        sphere.draw();

        const glm::mat4 cloudModel =
            glm::translate(glm::mat4(1.0f), effect.position) * glm::mat4(effect.basis) *
            glm::rotate(glm::mat4(1.0f), rotationAngle * 1.12f, glm::vec3(0, 1, 0)) *
            glm::scale(glm::mat4(1.0f), bodyScale * 1.025f);
        earthShader.set("cloudPass", 1);
        earthShader.set("model", cloudModel);
        GLboolean cloudDepthWrite = GL_TRUE;
        const GLboolean cloudBlend = glIsEnabled(GL_BLEND);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &cloudDepthWrite);
        GLint cloudSrcRgb = GL_ONE;
        GLint cloudDstRgb = GL_ZERO;
        GLint cloudSrcAlpha = GL_ONE;
        GLint cloudDstAlpha = GL_ZERO;
        glGetIntegerv(GL_BLEND_SRC_RGB, &cloudSrcRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &cloudDstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &cloudSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &cloudDstAlpha);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        sphere.draw();
        glDepthMask(cloudDepthWrite);
        glBlendFuncSeparate(static_cast<GLenum>(cloudSrcRgb), static_cast<GLenum>(cloudDstRgb),
                            static_cast<GLenum>(cloudSrcAlpha),
                            static_cast<GLenum>(cloudDstAlpha));
        if (cloudBlend)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);

        const glm::mat4 atmosphereModel =
            baseTransform * glm::scale(glm::mat4(1.0f), bodyScale * 1.07f);
        atmosphereShader.use();
        atmosphereShader.set("model", atmosphereModel);
        atmosphereShader.set("view", view);
        atmosphereShader.set("projection", projection);
        atmosphereShader.set("cameraPos", currentCamera);
        atmosphereShader.set("opacity", effect.opacity);
        const GLboolean atmosphereBlend = glIsEnabled(GL_BLEND);
        GLboolean atmosphereDepthWrite = GL_TRUE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &atmosphereDepthWrite);
        GLint oldCullFace = GL_BACK;
        glGetIntegerv(GL_CULL_FACE_MODE, &oldCullFace);
        GLint atmosphereSrcRgb = GL_ONE;
        GLint atmosphereDstRgb = GL_ZERO;
        GLint atmosphereSrcAlpha = GL_ONE;
        GLint atmosphereDstAlpha = GL_ZERO;
        glGetIntegerv(GL_BLEND_SRC_RGB, &atmosphereSrcRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &atmosphereDstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &atmosphereSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &atmosphereDstAlpha);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDepthMask(GL_FALSE);
        glCullFace(GL_FRONT);
        sphere.draw();
        glCullFace(static_cast<GLenum>(oldCullFace));
        glDepthMask(atmosphereDepthWrite);
        if (atmosphereBlend)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);
        glBlendFuncSeparate(static_cast<GLenum>(atmosphereSrcRgb),
                            static_cast<GLenum>(atmosphereDstRgb),
                            static_cast<GLenum>(atmosphereSrcAlpha),
                            static_cast<GLenum>(atmosphereDstAlpha));
        glActiveTexture(GL_TEXTURE0);
        return;
    }

    Shader &activeShader = sun ? sunShader : shader;
    activeShader.use();
    activeShader.set("model", model);
    activeShader.set("opacity", effect.opacity);
    if (sun)
        activeShader.set("time", simTime);
    else {
        activeShader.set("color", body.color);
        activeShader.set("lightPos", lightPosition);
        activeShader.set("sunLightPos", glm::vec3(0.0f));
        activeShader.set("cameraPos", currentCamera);
        activeShader.set("emissive", 0.0f);
        const int textureIndex = surfaceTextureIndex(body.name);
        activeShader.set("proceduralType", textureIndex);
        activeShader.set("hasSurfaceTexture",
                         worldTextures[static_cast<std::size_t>(textureIndex)].isLoaded() ? 1 : 0);
        activeShader.set("surfaceTexture", 0);
        worldTextures[static_cast<std::size_t>(textureIndex)].bind(0);
        const bool isSaturn = body.name == "Saturn";
        activeShader.set("ringEnabled", isSaturn ? 1 : 0);
        if (isSaturn) {
            // The fixed axial tilt sets Saturn's equatorial plane; spin does not wobble it.
            const glm::mat4 ringOrientation = glm::mat4(effect.basis) * axialTilt;
            const glm::vec3 ringNormal =
                glm::normalize(glm::vec3(ringOrientation * glm::vec4(0, 0, 1, 0)));
            activeShader.set("ringCenter", effect.position);
            activeShader.set("ringNormal", ringNormal);
            activeShader.set("ringInnerRadius", bodyRadius * effect.perpendicularScale * 1.35f);
            activeShader.set("ringOuterRadius", bodyRadius * effect.perpendicularScale * 2.6f);
        }
    }

    (highQuality ? sphere : lowDetailSphere).draw();
}

void Application::drawSaturnRing(const glm::mat4 &view, const glm::mat4 &projection) {
    constexpr int saturnIndex = 5;
    const Body &saturn = bodies[static_cast<std::size_t>(saturnIndex)];
    const BodyEffect effect = bodyEffect(saturnIndex);
    if (effect.swallowed || effect.opacity <= 0.0f)
        return;

    const float bodyRadius = saturn.radius;
    // The ring stays in the tilted equatorial plane; only Saturn's surface spins.
    const glm::mat4 orientation =
        glm::mat4(effect.basis) *
        glm::rotate(glm::mat4(1.0f), glm::radians(27.0f), glm::vec3(1, 0, 0));
    const float ringAngle = simTime * kRingSpinSpeed;
    // Spin around local +Z (the ring normal) before the fixed tilt and black-hole orientation.
    const glm::mat4 ringSpin =
        glm::rotate(glm::mat4(1.0f), ringAngle, glm::vec3(0, 0, 1));
    const glm::mat4 model =
        glm::translate(glm::mat4(1.0f), effect.position) * orientation * ringSpin *
        glm::scale(glm::mat4(1.0f),
                   glm::vec3(bodyRadius * effect.perpendicularScale,
                             bodyRadius * effect.perpendicularScale,
                             bodyRadius * effect.axialScale));
    const glm::vec3 lightPosition(20.0f * (std::cos(lightAngle) - 1.0f), 0.0f,
                                  20.0f * std::sin(lightAngle));

    ringShader.use();
    ringShader.set("model", model);
    ringShader.set("view", view);
    ringShader.set("projection", projection);
    ringShader.set("lightPos", lightPosition);
    ringShader.set("sunLightPos", glm::vec3(0.0f));
    ringShader.set("saturnCenter", effect.position);
    ringShader.set("ringNormal", glm::normalize(glm::vec3(orientation * glm::vec4(0, 0, 1, 0))));
    ringShader.set("saturnRadius", bodyRadius * effect.perpendicularScale);
    ringShader.set("innerRadius", 1.35f);
    ringShader.set("outerRadius", 2.6f);
    ringShader.set("ringAngle", ringAngle);
    ringShader.set("opacity", effect.opacity);
    ringShader.set("hasRingTexture", saturnRingTexture.isLoaded() ? 1 : 0);
    ringShader.set("ringTexture", 0);
    saturnRingTexture.bind();

    const GLboolean wasCulled = glIsEnabled(GL_CULL_FACE);
    const GLboolean wasBlended = glIsEnabled(GL_BLEND);
    GLboolean depthWrite = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
    GLint blendSource = GL_ONE;
    GLint blendDestination = GL_ZERO;
    GLint blendSourceAlpha = GL_ONE;
    GLint blendDestinationAlpha = GL_ZERO;
    glGetIntegerv(GL_BLEND_SRC_RGB, &blendSource);
    glGetIntegerv(GL_BLEND_DST_RGB, &blendDestination);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSourceAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDestinationAlpha);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    saturnRing.draw();
    glDepthMask(depthWrite);
    glBlendFuncSeparate(static_cast<GLenum>(blendSource), static_cast<GLenum>(blendDestination),
                        static_cast<GLenum>(blendSourceAlpha),
                        static_cast<GLenum>(blendDestinationAlpha));
    if (wasCulled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    if (wasBlended)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
}

void Application::drawEarthMoon(const glm::mat4 &view, const glm::mat4 &projection) {
    const BodyEffect effect = blackHole.affectBody(earthMoonPosition(), false);
    if (effect.swallowed || effect.opacity <= 0.0f)
        return;
    constexpr float twoPi = 6.2831853f;
    const float moonAngle = simTime * twoPi * 12.0f / ORBIT_BASE_SECONDS;
    const float moonSpinAngle = -moonAngle - glm::radians(90.0f);
    const glm::mat4 model =
        glm::translate(glm::mat4(1.0f), effect.position) * glm::mat4(effect.basis) *
        glm::rotate(glm::mat4(1.0f), moonSpinAngle, glm::vec3(0, 1, 0)) *
        glm::scale(glm::mat4(1.0f),
                   glm::vec3(EARTH_MOON_RADIUS * effect.perpendicularScale,
                             EARTH_MOON_RADIUS * effect.perpendicularScale,
                             EARTH_MOON_RADIUS * effect.axialScale));
    shader.use();
    shader.set("model", model);
    shader.set("view", view);
    shader.set("projection", projection);
    shader.set("color", glm::vec3(0.57f, 0.57f, 0.54f));
    shader.set("lightPos", glm::vec3(20.0f * (std::cos(lightAngle) - 1.0f), 0.0f,
                                      20.0f * std::sin(lightAngle)));
    shader.set("sunLightPos", glm::vec3(0.0f));
    shader.set("cameraPos", currentCamera);
    shader.set("emissive", 0.0f);
    shader.set("opacity", effect.opacity);
    shader.set("ringEnabled", 0);
    shader.set("proceduralType", 7);
    shader.set("hasSurfaceTexture", worldTextures[7].isLoaded() ? 1 : 0);
    shader.set("surfaceTexture", 0);
    worldTextures[7].bind(0);
    (highQuality ? sphere : lowDetailSphere).draw();
}

void Application::drawMeteors(const glm::mat4 &view, const glm::mat4 &projection) {
    if (!meteorsEnabled)
        return;

    GLint oldProgram = 0, oldVao = 0, oldSrcRgb = GL_ONE, oldDstRgb = GL_ZERO;
    GLint oldSrcAlpha = GL_ONE, oldDstAlpha = GL_ZERO;
    GLboolean oldDepthWrite = GL_TRUE;
    const GLboolean oldDepthTest = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean oldBlend = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
    glGetIntegerv(GL_BLEND_SRC_RGB, &oldSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &oldDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &oldSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &oldDstAlpha);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &oldDepthWrite);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    const glm::vec3 cameraForward = glm::normalize(cameraTarget - currentCamera);
    const glm::vec3 worldUp = std::abs(glm::dot(cameraForward, glm::vec3(0, 1, 0))) > 0.98f
                                  ? glm::vec3(0, 0, 1)
                                  : glm::vec3(0, 1, 0);
    const glm::vec3 cameraRight = glm::normalize(glm::cross(cameraForward, worldUp));
    const glm::vec3 cameraUp = glm::normalize(glm::cross(cameraRight, cameraForward));
    const std::size_t meteorCount = highQuality ? meteors.size() : meteors.size() / 2;
    meteorShader.use();
    meteorShader.set("view", view);
    meteorShader.set("projection", projection);
    meteorShader.set("holePosition", blackHole.getPosition());
    meteorShader.set("progress", blackHole.getThreatProgress());
    meteorShader.set("horizonRadius", blackHole.getHorizonRadius());
    meteorShader.set("pullRadius", blackHole.getPullRadius());
    meteorShader.set("maximumPullStrength", blackHole.getMaxPullStrength());
    meteorShader.set("maximumBodyStretch", blackHole.getMaxBodyStretch());
    meteorShader.set("stretchInfluenceRadius", blackHole.getStretchInfluenceRadius());
    meteorShader.set("activationThreshold", blackHole.getBodyActivationThreshold());
    meteorShader.set("drawTail", 1);
    for (std::size_t i = 0; i < meteorCount; ++i) {
        const Meteor &meteor = meteors[i];
        // Accumulated simulation time freezes with pause and responds to speed changes.
        const float phase = std::fmod(meteor.phase + simTime * meteor.speed / 32.0f, 1.0f);
        const glm::vec3 position = meteor.start + meteor.direction * (phase * 72.0f);
        glm::vec3 tailDirection = -meteor.direction;
        tailDirection -= cameraForward * glm::dot(tailDirection, cameraForward);
        if (glm::dot(tailDirection, tailDirection) < 0.0001f)
            tailDirection = cameraUp;
        tailDirection = glm::normalize(tailDirection);
        const glm::vec3 tailSide = glm::normalize(glm::cross(cameraForward, tailDirection));
        meteorShader.set("center", position);
        meteorShader.set("tailDirection", tailDirection);
        meteorShader.set("tailSide", tailSide);
        meteorShader.set("tailLength", 0.9f + meteor.radius * 12.0f);
        meteorShader.set("tailWidth", meteor.radius * 0.34f);
        meteorShader.set("color", meteor.color);
        glBindVertexArray(meteorQuadVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        meteorShader.set("drawTail", 0);
        const glm::mat4 model = glm::translate(glm::mat4(1.0f), position) *
                                glm::scale(glm::mat4(1.0f), glm::vec3(meteor.radius));
        meteorShader.set("model", model);
        meteorShader.set("color", meteor.color);
        sphere.draw();
        meteorShader.set("drawTail", 1);
    }

    glDepthMask(oldDepthWrite);
    if (oldDepthTest)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    glBlendFuncSeparate(static_cast<GLenum>(oldSrcRgb), static_cast<GLenum>(oldDstRgb),
                        static_cast<GLenum>(oldSrcAlpha), static_cast<GLenum>(oldDstAlpha));
    if (oldBlend)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    glUseProgram(static_cast<GLuint>(oldProgram));
    glBindVertexArray(static_cast<GLuint>(oldVao));
}

void Application::drawAsteroids(const glm::mat4 &view, const glm::mat4 &projection) {
    const std::size_t count = std::min(asteroids.size(),
                                       highQuality ? ASTEROID_COUNT_HIGH : ASTEROID_COUNT_LOW);
    if (count == 0)
        return;
    struct InstanceData {
        glm::mat4 model;
        glm::vec4 color;
    };
    std::vector<InstanceData> instances;
    instances.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const Asteroid &asteroid = asteroids[i];
        const float angle = asteroid.angle + simTime * asteroid.angularSpeed;
        const glm::vec3 position(asteroid.orbitRadius * std::cos(angle), asteroid.verticalOffset,
                                 asteroid.orbitRadius * std::sin(angle));
        const glm::mat4 model = glm::translate(glm::mat4(1.0f), position) *
                                glm::rotate(glm::mat4(1.0f), angle * 1.7f, glm::vec3(1, 1, 0)) *
                                glm::scale(glm::mat4(1.0f), glm::vec3(asteroid.size));
        instances.push_back({model, glm::vec4(asteroid.color, 1.0f)});
    }

    glm::vec3 planetPositions[9];
    float planetRadii[9];
    int planetCount = 0;
    for (int i = 0; i <= 8; ++i) {
        const BodyEffect effect = bodyEffect(i);
        if (effect.swallowed || effect.opacity <= 0.0f)
            continue;
        const float radius = i == 8 ? 1.25f : bodies[static_cast<std::size_t>(i)].radius;
        planetPositions[planetCount] = effect.position;
        planetRadii[planetCount] = radius * std::max(effect.axialScale, effect.perpendicularScale);
        ++planetCount;
    }

    asteroidShader.use();
    asteroidShader.set("view", view);
    asteroidShader.set("projection", projection);
    asteroidShader.set("lightPosition", glm::vec3(20.0f * (std::cos(lightAngle) - 1.0f),
                                                    0.0f, 20.0f * std::sin(lightAngle)));
    asteroidShader.set("cameraPosition", currentCamera);
    asteroidShader.set("holePosition", blackHole.getPosition());
    asteroidShader.set("progress", blackHole.getThreatProgress());
    asteroidShader.set("horizonRadius", blackHole.getHorizonRadius());
    asteroidShader.set("pullRadius", blackHole.getPullRadius());
    asteroidShader.set("maximumPullStrength", blackHole.getMaxPullStrength());
    asteroidShader.set("maximumBodyStretch", blackHole.getMaxBodyStretch());
    asteroidShader.set("stretchInfluenceRadius", blackHole.getStretchInfluenceRadius());
    asteroidShader.set("activationThreshold", blackHole.getBodyActivationThreshold());
    asteroidShader.set("planetCount", planetCount);
    const GLint positionsLocation = glGetUniformLocation(asteroidShader.id, "planetPositions[0]");
    const GLint radiiLocation = glGetUniformLocation(asteroidShader.id, "planetRadii[0]");
    if (planetCount > 0) {
        glUniform3fv(positionsLocation, planetCount, &planetPositions[0].x);
        glUniform1fv(radiiLocation, planetCount, planetRadii);
    }

    GLint oldVao = 0, oldArrayBuffer = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldArrayBuffer);
    if (instancing) {
        glBindVertexArray(asteroidMesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, asteroidInstanceVbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(instances.size() * sizeof(InstanceData)),
                     instances.data(), GL_STREAM_DRAW);
        for (GLuint column = 0; column < 4; ++column) {
            glVertexAttribPointer(3 + column, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                                  reinterpret_cast<void *>(static_cast<std::uintptr_t>(column * 4 * sizeof(float))));
        }
        glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                              reinterpret_cast<void *>(sizeof(glm::mat4)));
        glEnableVertexAttribArray(7);
        glVertexAttribDivisor(7, 1);
        asteroidShader.set("useInstance", 1);
        glDrawElementsInstanced(GL_TRIANGLES, asteroidMesh.count, GL_UNSIGNED_INT, nullptr,
                                static_cast<GLsizei>(count));
        asteroidDrawCalls = 1;
    } else {
        asteroidShader.set("useInstance", 0);
        asteroidDrawCalls = 0;
        for (const InstanceData &instance : instances) {
            asteroidShader.set("model", instance.model);
            asteroidShader.set("baseColor", glm::vec3(instance.color));
            asteroidMesh.draw();
            ++asteroidDrawCalls;
        }
    }
    glBindVertexArray(static_cast<GLuint>(oldVao));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(oldArrayBuffer));
}

void Application::drawSky(const glm::mat4 &view, const glm::mat4 &projection) {
    skyShader.use();
    skyShader.set("view", glm::mat4(glm::mat3(view)));
    skyShader.set("projection", projection);
    skyShader.set("skyParallax", -galaxyDisplacement() * 0.0008f);
    skyShader.set("model", glm::scale(glm::mat4(1.0f), glm::vec3(90.0f)));
    skyShader.set("useTexture", skyTexture.isLoaded() ? 1 : 0);
    skyTexture.bind();
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    sphere.draw();
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

void Application::input(float deltaTime) {
    const bool escape = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (escape && !lastEscapeDown) {
        selected = -1;
        distance = overviewDistance;
        flying = true;
        flyElapsed = 0;
        startCamera = currentCamera;
        startTarget = cameraTarget;
    }
    lastEscapeDown = escape;
    const bool space = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    static bool spaceWasDown = false;
    if (space && !spaceWasDown)
        paused = !paused;
    spaceWasDown = space;
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
        speed = 1;
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
        speed = 5;
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
        speed = 10;
    const bool g = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
    const bool l = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
    const bool i = glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS;
    const bool m = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
    const bool q = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
    const bool b = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
    const bool f1 = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
    const bool h = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
    const bool r = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    const bool keyY = glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS;
    const bool keyN = glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS;
    const bool keyP = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
    if (keyN && !lastNDown) {
        cityMarkersEnabled = !cityMarkersEnabled;
        std::cout << "Earth city markers: " << (cityMarkersEnabled ? "ON" : "OFF") << '\n';
    }
    if (keyP && !lastPDown && selected == 2) {
        mapPanelOpen = !mapPanelOpen;
        if (mapPanelOpen && distance > 4.5f) {
            distance = 4.5f;
            flyCameraOffset = glm::vec3(distance * std::cos(pitch) * std::sin(yaw),
                                        distance * std::sin(pitch),
                                        distance * std::cos(pitch) * std::cos(yaw));
            startCamera = currentCamera;
            startTarget = cameraTarget;
            flyElapsed = 0.0f;
            flying = true;
        }
    }
    lastNDown = keyN;
    lastPDown = keyP;
    if (keyY && !lastYDown && gameState == GameState::FreeExplore && blackHole.isIdle()) {
        galaxyViewTarget = !galaxyViewTarget;
        if (galaxyViewTarget && galaxyTransition == 0.0f)
            for (GalaxyTrail &trail : galaxyTrails) { trail.head = 0; trail.count = 0; }
        std::cout << "View: " << (galaxyViewTarget ? "Milky Way" : "Solar System") << '\n';
    }
    lastYDown = keyY;
    const bool f12 = glfwGetKey(window, GLFW_KEY_F12) == GLFW_PRESS;
    if (g && !lastGDown) {
        bloom = !bloom;
        std::cout << "Bloom: " << (bloom ? "ON" : "OFF") << '\n';
    }
    if (l && !lastLDown) {
        lensing = !lensing;
        std::cout << "Lensing: " << (lensing ? "ON" : "OFF") << '\n';
    }
    if (i && !lastIDown) {
        instancing = !instancing;
        std::cout << "Instancing: " << (instancing ? "ON" : "OFF") << '\n';
    }
    if (m && !lastMDown) {
        meteorsEnabled = !meteorsEnabled;
        std::cout << "Meteors: " << (meteorsEnabled ? "ON" : "OFF") << '\n';
    }
    if (q && !lastQDown) {
        highQuality = !highQuality;
        applyQualitySettings();
        std::cout << "Quality: " << (highQuality ? "High" : "Low") << '\n';
    }
    if (b && !lastBDown) {
        if (gameState == GameState::Mission || gameState == GameState::FreeExplore) {
            if (blackHole.isIdle()) {
                if (galaxyViewTarget) {
                    galaxyViewTarget = false;
                    std::cout << "View: Solar System (black hole active)\n";
                }
                freeExploreSwallowedDismissed = false;
                blackHole.summon();
            } else {
                blackHole.returnBack();
            }
        }
    }
    if ((f1 && !lastF1Down) || (h && !lastHDown))
        help = !help;
    if (r && !lastRDown) {
        if (gameState == GameState::FreeExplore) {
            if (blackHole.isSunSwallowed())
                startFreeExplore();
            else
                blackHole.returnBack();
        } else if (gameState == GameState::Mission || gameState == GameState::Won ||
                 gameState == GameState::Lost)
            startMission();
    }
    if (f12 && !lastF12Down)
        screenshotRequested = true;
    lastGDown = g;
    lastLDown = l;
    lastIDown = i;
    lastMDown = m;
    lastQDown = q;
    lastBDown = b;
    lastF1Down = f1;
    lastHDown = h;
    lastRDown = r;
    lastF12Down = f12;

    const bool leftBracket = glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS;
    const bool rightBracket = glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
    if (leftBracket)
        lightAngle -= deltaTime * 0.8f;
    if (rightBracket)
        lightAngle += deltaTime * 0.8f;
    lightAngle = std::fmod(lightAngle + 6.2831853f, 6.2831853f);

    const bool f = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    if (f && !lastFDown) {
        freeFly = !freeFly;
        freeForward = glm::normalize(cameraTarget - currentCamera);
    }
    lastFDown = f;
    const bool overUi = ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse;
    double x = 0, y = 0;
    glfwGetCursorPos(window, &x, &y);
    const bool right = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const float dx = static_cast<float>(x) - mouseX, dy = static_cast<float>(y) - mouseY;
    if (!overUi && right) {
        if (freeFly) {
            const glm::vec3 worldUp(0, 1, 0);
            glm::vec3 rightVec = glm::normalize(glm::cross(freeForward, worldUp));
            freeForward = glm::vec3(glm::normalize(
                glm::rotate(glm::mat4(1), -dx * 0.004f, worldUp) * glm::vec4(freeForward, 0)));
            rightVec = glm::normalize(glm::cross(freeForward, worldUp));
            freeForward = glm::vec3(glm::normalize(
                glm::rotate(glm::mat4(1), -dy * 0.004f, rightVec) * glm::vec4(freeForward, 0)));
            cameraTarget = currentCamera + freeForward;
        } else {
            yaw -= dx * 0.005f;
            pitch = std::clamp(pitch + dy * 0.005f, -1.45f, 1.45f);
            if (selected >= 0) {
                flyCameraOffset = glm::vec3(distance * std::cos(pitch) * std::sin(yaw),
                                            distance * std::sin(pitch),
                                            distance * std::cos(pitch) * std::cos(yaw));
            }
        }
    }
    if (!overUi && !freeFly) {
        distance = std::clamp(distance - scrollDelta * 0.8f, 3.0f, 75.0f);
        if (selected >= 0) {
            flyCameraOffset =
                glm::vec3(distance * std::cos(pitch) * std::sin(yaw), distance * std::sin(pitch),
                          distance * std::cos(pitch) * std::cos(yaw));
        }
    }
    scrollDelta = 0;
    if (!overUi && freeFly) {
        const float fast = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 3.0f : 1.0f;
        const glm::vec3 rightVec = glm::normalize(glm::cross(freeForward, glm::vec3(0, 1, 0)));
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            currentCamera += freeForward * deltaTime * 8.0f * fast;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            currentCamera -= freeForward * deltaTime * 8.0f * fast;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            currentCamera -= rightVec * deltaTime * 8.0f * fast;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            currentCamera += rightVec * deltaTime * 8.0f * fast;
        cameraTarget = currentCamera + freeForward;
    }
    const bool left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool gameplayActive =
        gameState == GameState::Mission || gameState == GameState::FreeExplore;
    if (left && !lastLeftDown && !overUi && !quizOpen && gameplayActive &&
        windowWidth > 0 && windowHeight > 0 && width > 0 && height > 0) {
        const float framebufferX = static_cast<float>(x) * width / windowWidth;
        const float framebufferY = static_cast<float>(y) * height / windowHeight;
        const float ndcX = 2.0f * framebufferX / static_cast<float>(width) - 1.0f;
        const float ndcY = 1.0f - 2.0f * framebufferY / static_cast<float>(height);
        const glm::mat4 projection =
            glm::perspective(glm::radians(45.0f), static_cast<float>(width) / height, 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(currentCamera, cameraTarget, glm::vec3(0, 1, 0));
        const glm::mat4 inverse = glm::inverse(projection * view);
        glm::vec4 nearPoint = inverse * glm::vec4(ndcX, ndcY, -1, 1);
        glm::vec4 farPoint = inverse * glm::vec4(ndcX, ndcY, 1, 1);
        nearPoint /= nearPoint.w;
        farPoint /= farPoint.w;
        const glm::vec3 origin(nearPoint);
        const glm::vec3 ray = glm::normalize(glm::vec3(farPoint - nearPoint));
        float closest = 1.0e30f;
        int hit = -1;
        for (int i = 0; i <= 8; ++i) {
            const BodyEffect effect = bodyEffect(i);
            if (effect.swallowed)
                continue;
            const glm::vec3 center = effect.position;
            const float baseRadius = i == 8 ? 1.25f : bodies[static_cast<std::size_t>(i)].radius;
            const float radius = baseRadius * std::max(effect.axialScale, effect.perpendicularScale);
            const glm::vec3 oc = origin - center;
            const float b = glm::dot(oc, ray);
            const float c = glm::dot(oc, oc) - radius * radius;
            const float discriminant = b * b - c;
            if (discriminant < 0)
                continue;
            const float t = -b - std::sqrt(discriminant);
            if (t > 0 && t < closest) {
                closest = t;
                hit = i;
            }
        }
        if (hit >= 0) {
            selectBody(hit);
            if (hit == 2) {
                const glm::vec3 worldHit = origin + ray * closest;
                const glm::vec3 localHit(glm::inverse(earthModelMatrix()) * glm::vec4(worldHit, 1.0f));
                const glm::vec3 unit = glm::normalize(localHit);
                pickedLatitude = glm::degrees(std::asin(std::clamp(unit.y, -1.0f, 1.0f)));
                pickedLongitude = glm::degrees(std::atan2(unit.z, unit.x));
                hasEarthCoordinate = true;
                std::cout << "Earth pick: Lat " << std::fixed << std::setprecision(2)
                          << std::abs(pickedLatitude) << (pickedLatitude >= 0 ? " N" : " S")
                          << ", Lon " << std::abs(pickedLongitude)
                          << (pickedLongitude >= 0 ? " E" : " W") << std::defaultfloat << '\n';
            }
        }
    }
    lastLeftDown = left;
    mouseX = static_cast<float>(x);
    mouseY = static_cast<float>(y);
}

void Application::run() {
    float last = static_cast<float>(glfwGetTime());
    while (!glfwWindowShouldClose(window)) {
        const float now = static_cast<float>(glfwGetTime());
        const float dt = std::clamp(now - last, 0.0f, 0.05f);
        last = now;
        glfwGetFramebufferSize(window, &width, &height);
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        if (width == 0 || height == 0) {
            glfwWaitEvents();
            continue;
        }
        input(dt);
        if (!postProcess.resize(width, height, highQuality ? 1.0f : 0.75f)) {
            glfwPollEvents();
            continue;
        }
        if (!blackHole.resize(postProcess.getWidth(), postProcess.getHeight())) {
            glfwPollEvents();
            continue;
        }
        const bool gameplayActive =
            gameState == GameState::Mission || gameState == GameState::FreeExplore;
        const float simulationDelta = gameplayActive && !paused && !quizOpen
                                          ? dt * static_cast<float>(speed) : 0.0f;
        if (simulationDelta > 0.0f)
            simTime += simulationDelta;
        const float threatDeltaTime = simulationDelta;
        if (gameState == GameState::Mission)
            updateMission(threatDeltaTime);
        else if (gameState == GameState::FreeExplore) {
            blackHole.update(threatDeltaTime);
            if (blackHole.isSunSwallowed())
                paused = true;
        }
        updateGalaxy(simulationDelta);
        if (selected >= 0 && bodyEffect(selected).swallowed) {
            selected = -1;
            distance = overviewDistance;
            flying = true;
            flyElapsed = 0.0f;
            startCamera = currentCamera;
            startTarget = cameraTarget;
        }
        if (flying) {
            flyElapsed += dt;
            const float t = std::clamp(flyElapsed / 1.5f, 0.0f, 1.0f);
            const float eased =
                t * t * (3.0f - 2.0f); // Smoothstep eases both ends of the camera path.
            const glm::vec3 endTarget = selected == 2 && earthMapTargetActive
                ? glm::vec3(earthModelMatrix() * glm::vec4(earthMapTargetLocal, 1.0f))
                : (selected >= 0 ? bodyEffect(selected).position : glm::vec3(0));
            const glm::vec3 endCamera = selected >= 0
                                            ? endTarget + flyCameraOffset * (1.0f + galaxyBlend * 1.35f)
                                            : endTarget + (cameraPosition() - cameraTarget);
            cameraTarget = glm::mix(startTarget, endTarget, eased);
            currentCamera = glm::mix(startCamera, endCamera, eased);
            if (t >= 1.0f)
                flying = false;
        } else if (selected >= 0 && !freeFly) {
            const glm::vec3 target = selected == 2 && earthMapTargetActive
                ? glm::vec3(earthModelMatrix() * glm::vec4(earthMapTargetLocal, 1.0f))
                : bodyEffect(selected).position;
            cameraTarget = target;
            currentCamera = target + flyCameraOffset * (1.0f + galaxyBlend * 1.35f);
        } else if (!freeFly) {
            currentCamera = cameraPosition();
        }
        if (!postProcess.beginScene()) {
            glfwPollEvents();
            continue;
        }
        const glm::mat4 view = glm::lookAt(currentCamera, cameraTarget, glm::vec3(0, 1, 0));
        const glm::mat4 projection =
            glm::perspective(glm::radians(45.0f), static_cast<float>(width) / height, 0.1f, 100.0f);
        drawSky(view, projection);
        orbitShader.use();
        orbitShader.set("view", view);
        orbitShader.set("projection", projection);
        orbitShader.set("alpha", 1.0f - galaxyBlend);
        const GLboolean oldLineBlend = glIsEnabled(GL_BLEND);
        GLint oldLineSrc = GL_ONE, oldLineDst = GL_ZERO, oldLineSrcAlpha = GL_ONE,
              oldLineDstAlpha = GL_ZERO;
        glGetIntegerv(GL_BLEND_SRC_RGB, &oldLineSrc);
        glGetIntegerv(GL_BLEND_DST_RGB, &oldLineDst);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &oldLineSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &oldLineDstAlpha);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        GLboolean oldOrbitDepthWrite = GL_TRUE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &oldOrbitDepthWrite);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            orbitShader.set("model", glm::mat4(1));
            orbitShader.set("color", glm::vec3(0.26f, 0.31f, 0.42f));
            orbitLines[i].drawOrbit();
        }
        const BodyEffect earthEffect = bodyEffect(2);
        orbitShader.set("model", glm::translate(glm::mat4(1.0f), earthEffect.position));
        orbitShader.set("color", glm::vec3(0.38f, 0.48f, 0.64f));
        earthMoonOrbitLine.drawOrbit();
        if (selected >= 0) {
            const glm::vec3 position = bodyEffect(selected).position;
            const float radius =
                selected == 8 ? 1.25f : bodies[static_cast<std::size_t>(selected)].radius;
            const glm::mat4 ringModel =
                glm::translate(glm::mat4(1), position) *
                glm::scale(glm::mat4(1), glm::vec3(radius * 1.35f, 1, radius * 1.35f));
            orbitShader.set("model", ringModel);
            orbitShader.set("color", glm::vec3(1.0f, 0.85f, 0.25f));
            highlightRing.drawOrbit();
        }
        glDepthMask(oldOrbitDepthWrite);
        glEnable(GL_CULL_FACE);
        glBlendFuncSeparate(oldLineSrc, oldLineDst, oldLineSrcAlpha, oldLineDstAlpha);
        if (!oldLineBlend) glDisable(GL_BLEND);
        shader.use();
        shader.set("view", view);
        shader.set("projection", projection);
        for (const Body &body : bodies)
            draw(body, view, projection);
        sunBody.radius = 1.25f;
        sunShader.use();
        sunShader.set("view", view);
        sunShader.set("projection", projection);
        draw(sunBody, view, projection, true);
        drawEarthMoon(view, projection);
        blackHole.draw(view, projection, currentCamera, simTime);
        drawSaturnRing(view, projection);
        drawAsteroids(view, projection);
        drawMeteors(view, projection);
        drawCityPins(view, projection);
        drawGalaxyTrails(view, projection);

        postProcess.endScene();
        const GLuint hdrSceneTexture = postProcess.getHdrSceneColorTexture();
        const GLuint sceneTextureForComposite =
            blackHole.renderLensing(hdrSceneTexture, postProcess.getWidth(), postProcess.getHeight(),
                                    view, projection, lensing);
        postProcess.composite(sceneTextureForComposite, bloom, highQuality ? 8 : 4);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        UiTheme::beginFrame();
        drawCityLabels(view, projection);
        const float maxPanelWidth = std::min(460.0f, static_cast<float>(windowWidth) * 0.40f);
        const float requestedPanelWidth = static_cast<float>(windowWidth) * 0.26f;
        const float panelWidth = maxPanelWidth >= 300.0f
                                     ? std::clamp(requestedPanelWidth, 300.0f, maxPanelWidth)
                                     : maxPanelWidth;
        ImGui::SetNextWindowPos(ImVec2(kPanelOnRight ? static_cast<float>(windowWidth) - panelWidth : 0.0f, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panelWidth, static_cast<float>(windowHeight)), ImGuiCond_Always);
        ImGui::Begin("Mission Control", nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        ImGui::Text("Team: Your Team");
        ImGui::TextWrapped("Event Horizon: Solar System Explorer");
        ImGui::Text("FPS %.0f | Speed %dx | %s", ImGui::GetIO().Framerate, speed,
                    paused ? "Paused" : "Running");
        const char *missionStatus = "Start screen";
        if (gameState == GameState::FreeExplore)
            missionStatus = "Free explore";
        else if (gameState == GameState::Mission)
            missionStatus = "Mission active";
        else if (gameState == GameState::Won)
            missionStatus = "Mission won";
        else if (gameState == GameState::Lost)
            missionStatus = "Sun swallowed";
        ImGui::TextWrapped("Status: %s | Score: %d / %d", missionStatus, missionScore,
                    MISSION_TARGET_SCORE);
        ImGui::SeparatorText("SYSTEM STATUS");
        const float chipAvail = ImGui::GetContentRegionAvail().x;
        float chipRowWidth = 0.0f;
        auto drawStatusChip = [&](const char *label, bool active, auto action) {
            const float itemWidth = ImGui::CalcTextSize(label).x +
                                    ImGui::GetStyle().FramePadding.x * 2.0f +
                                    ImGui::GetStyle().ItemSpacing.x;
            if (chipRowWidth > 0.0f && chipRowWidth + itemWidth <= chipAvail) {
                ImGui::SameLine();
                chipRowWidth += itemWidth;
            } else {
                chipRowWidth = itemWidth;
            }
            if (UiTheme::chip(label, active, ImVec2(0, 30))) action();
        };
        drawStatusChip(bloom ? "Bloom ON" : "Bloom OFF", bloom, [&] { bloom = !bloom; });
        drawStatusChip(lensing ? "Lensing ON" : "Lensing OFF", lensing, [&] { lensing = !lensing; });
        drawStatusChip(instancing ? "Instancing ON" : "Instancing OFF", instancing, [&] { instancing = !instancing; });
        drawStatusChip(meteorsEnabled ? "Meteors ON" : "Meteors OFF", meteorsEnabled,
                       [&] { meteorsEnabled = !meteorsEnabled; });
        drawStatusChip(cityMarkersEnabled ? "Cities ON (N)" : "Cities OFF (N)", cityMarkersEnabled,
                       [&] { cityMarkersEnabled = !cityMarkersEnabled; });
        drawStatusChip(highQuality ? "Quality High" : "Quality Low", highQuality, [&] {
            highQuality = !highQuality;
            applyQualitySettings();
        });
        char holeChip[64];
        std::snprintf(holeChip, sizeof(holeChip), "Black hole %s p=%.0f%%", blackHole.getStateName(),
                      blackHole.getThreatProgress() * 100.0f);
        drawStatusChip(holeChip, !blackHole.isIdle(), [&] {
          if (gameplayActive) {
            if (blackHole.isIdle()) {
                if (galaxyViewTarget) galaxyViewTarget = false;
                blackHole.summon();
            } else {
                blackHole.returnBack();
            }
          }
        });
        ImGui::Separator();
        ImGui::BeginChild("panel_content", ImVec2(0, 0), false);
        ImGui::TextWrapped("Bloom %s | Instancing %s | Meteors: %s | Lensing: %s",
                    bloom ? "ON" : "OFF", instancing ? "ON" : "OFF",
                    meteorsEnabled ? "ON" : "OFF", lensing ? "ON" : "OFF");
        ImGui::TextWrapped("Quality: %s | Black hole: %s p=%.0f%% | Light %.1f deg",
                    highQuality ? "High" : "Low", blackHole.getStateName(),
                    blackHole.getThreatProgress() * 100.0f, lightAngle * 57.2957795f);
        UiTheme::header();
        const char *viewName = galaxyViewTarget ? "Milky Way" : "Solar System";
        ImGui::Text("View: %s", viewName);
        const bool canChangeView = gameState == GameState::FreeExplore && blackHole.isIdle();
        const float segmentWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (UiTheme::SmallFont) ImGui::PushFont(UiTheme::SmallFont);
        ImGui::BeginDisabled(!canChangeView);
        if (UiTheme::chip("Solar System", !galaxyViewTarget, ImVec2(segmentWidth / UiTheme::UiScale, 30))) {
            galaxyViewTarget = false;
            std::cout << "View: Solar System\n";
        }
        ImGui::SameLine();
        if (UiTheme::chip("Milky Way", galaxyViewTarget, ImVec2(segmentWidth / UiTheme::UiScale, 30))) {
            if (!galaxyViewTarget && galaxyTransition == 0.0f)
                for (GalaxyTrail &trail : galaxyTrails) { trail.head = 0; trail.count = 0; }
            galaxyViewTarget = true;
            std::cout << "View: Milky Way\n";
        }
        ImGui::EndDisabled();
        if (UiTheme::SmallFont) ImGui::PopFont();
        if (!canChangeView && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Available in Free Explore while the black hole is Idle.");
        UiTheme::hero();
        ImGui::TextDisabled("Milky Way mode is a stylized visualization.");
        ImGui::TextWrapped("Asteroids: %zu | draw calls: %d",
                    highQuality ? ASTEROID_COUNT_HIGH : ASTEROID_COUNT_LOW, asteroidDrawCalls);
        if (gameState == GameState::Mission)
            ImGui::ProgressBar(activeThreat().getThreatProgress(), ImVec2(-1, 0),
                               "Threat approaching");
        ImGui::Separator();
        if (gameplayActive) {
            ImGui::SeparatorText("WORLDS");
            for (std::size_t i = 0; i < bodies.size(); ++i) {
                if (bodyEffect(static_cast<int>(i)).swallowed) continue;
                char subtitle[128];
                std::snprintf(subtitle, sizeof(subtitle), "%s  |  %.1f million km from Sun",
                              bodies[i].type.c_str(), bodies[i].distanceMillionKm);
                const glm::vec3 tint = bodies[i].color;
                if (UiTheme::planetCard(bodies[i].name.c_str(), subtitle,
                                        ImVec4(tint.r, tint.g, tint.b, 1.0f),
                                        selected == static_cast<int>(i)))
                    selectBody(static_cast<int>(i));
            }
            if (!bodyEffect(8).swallowed &&
                UiTheme::planetCard("Sun", "Star  |  centre of the system",
                                    ImVec4(1.0f, 0.72f, 0.20f, 1.0f), selected == 8))
                selectBody(8);
        }
        if (selected >= 0) {
            const Body &b = selected == 8 ? sunBody : bodies[static_cast<std::size_t>(selected)];
            ImGui::Separator();
            ImGui::BeginChild("world_info_card", ImVec2(0, 460.0f * UiTheme::UiScale), true);
            ImGui::TextColored(UiTheme::Accent, "%s", b.name.c_str());
            ImGui::TextColored(UiTheme::Secondary, "%s", b.type.c_str());
            ImGui::Separator();
            char value[96];
            std::snprintf(value, sizeof(value), "%.0f km", b.diameterKm);
            UiTheme::keyValue("Diameter", value);
            std::snprintf(value, sizeof(value), "%.4g kg", b.massKg);
            UiTheme::keyValue("Mass", value);
            std::snprintf(value, sizeof(value), "%.1f million km", b.distanceMillionKm);
            UiTheme::keyValue("Distance from Sun", value);
            std::snprintf(value, sizeof(value), "%.2f days", b.orbitalDays);
            UiTheme::keyValue("Orbital period", value);
            std::snprintf(value, sizeof(value), "%.3f days", b.rotationPeriodDays);
            UiTheme::keyValue("Rotation period", value);
            std::snprintf(value, sizeof(value), "%.2f m/s^2", b.gravity);
            UiTheme::keyValue("Gravity", value);
            if (b.name == "Earth")
                UiTheme::keyValue("Moons", "1");
            else {
                std::snprintf(value, sizeof(value), "%d", b.moons);
                UiTheme::keyValue("Moons", value);
            }
            if (b.name == "Earth" && hasEarthCoordinate) {
                char coordinate[96];
                std::snprintf(coordinate, sizeof(coordinate), "Lat %.2f %c, Lon %.2f %c",
                              std::abs(pickedLatitude), pickedLatitude >= 0.0f ? 'N' : 'S',
                              std::abs(pickedLongitude), pickedLongitude >= 0.0f ? 'E' : 'W');
                UiTheme::keyValue("Coordinates", coordinate);
            }
            if (b.name == "Earth" && distance <= 5.0f) {
                ImGui::SetNextItemOpen(mapPanelOpen, ImGuiCond_Always);
                mapPanelOpen = ImGui::CollapsingHeader("Map");
                if (mapPanelOpen) {
                    const ImVec2 mapSize(std::max(180.0f, ImGui::GetContentRegionAvail().x), 132.0f);
                    ImGui::InvisibleButton("Earth map viewport", mapSize);
                    const ImVec2 topLeft = ImGui::GetItemRectMin();
                    const ImVec2 bottomRight = ImGui::GetItemRectMax();
                    const bool hovered = ImGui::IsItemHovered();
                    if (hovered && ImGui::GetIO().MouseWheel != 0.0f)
                        mapZoom = std::clamp(mapZoom * std::pow(1.18f, ImGui::GetIO().MouseWheel), 1.0f, 5.0f);
                    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                        const ImVec2 delta = ImGui::GetIO().MouseDelta;
                        mapPan += glm::vec2(-delta.x / mapSize.x / mapZoom, -delta.y / mapSize.y / mapZoom);
                        const float panLimit = 0.5f * (1.0f - 1.0f / mapZoom);
                        mapPan = glm::clamp(mapPan, glm::vec2(-panLimit), glm::vec2(panLimit));
                    }
                    const float visible = 1.0f / mapZoom;
                    const float u0 = 0.5f - visible * 0.5f + mapPan.x;
                    const float v0 = 0.5f - visible * 0.5f + mapPan.y;
                    const float u1 = u0 + visible, v1 = v0 + visible;
                    const GLuint mapTexture = earthDayTexture.isLoaded() ? earthDayTexture.id() : generatedEarthMap;
                    const ImTextureID mapId = (ImTextureID)(intptr_t)mapTexture;
                    ImDrawList *mapDraw = ImGui::GetWindowDrawList();
                    mapDraw->AddImage(mapId, topLeft, bottomRight, ImVec2(u0, 1.0f - v0), ImVec2(u1, 1.0f - v1));
                    if (cityMarkersEnabled) {
                        for (const CityMarker &city : cities) {
                            float cityU = city.longitude / 360.0f;
                            if (cityU < 0.0f) cityU += 1.0f;
                            const float cityV = 0.5f - city.latitude / 180.0f;
                            if (cityU < u0 || cityU > u1 || cityV < v0 || cityV > v1) continue;
                            const ImVec2 point(topLeft.x + (cityU - u0) / visible * mapSize.x,
                                               topLeft.y + (cityV - v0) / visible * mapSize.y);
                            mapDraw->AddCircleFilled(point, 2.0f, IM_COL32(255, 220, 135, 235));
                        }
                    }
                    const ImVec2 center((topLeft.x + bottomRight.x) * 0.5f, (topLeft.y + bottomRight.y) * 0.5f);
                    mapDraw->AddCircle(center, 5.0f, IM_COL32(255, 215, 70, 255), 0, 2.0f);
                    mapDraw->AddLine(ImVec2(center.x - 8, center.y), ImVec2(center.x + 8, center.y), IM_COL32(255, 215, 70, 210));
                    mapDraw->AddLine(ImVec2(center.x, center.y - 8), ImVec2(center.x, center.y + 8), IM_COL32(255, 215, 70, 210));
                    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                        const ImVec2 mouse = ImGui::GetIO().MousePos;
                        const float mapU = u0 + (mouse.x - topLeft.x) / mapSize.x * visible;
                        const float mapV = v0 + (mouse.y - topLeft.y) / mapSize.y * visible;
                        pickedLatitude = 90.0f - mapV * 180.0f;
                        pickedLongitude = mapU <= 0.5f ? mapU * 360.0f : (mapU - 1.0f) * 360.0f;
                        const float lat = glm::radians(pickedLatitude), lon = glm::radians(pickedLongitude);
                        earthMapTargetLocal = glm::vec3(std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon));
                        earthMapTargetActive = true;
                        hasEarthCoordinate = true;
                        startCamera = currentCamera;
                        startTarget = cameraTarget;
                        flyElapsed = 0.0f;
                        flying = true;
                        std::cout << "Earth map target: Lat " << std::fixed << std::setprecision(2)
                                  << std::abs(pickedLatitude) << (pickedLatitude >= 0 ? " N" : " S")
                                  << ", Lon " << std::abs(pickedLongitude)
                                  << (pickedLongitude >= 0 ? " E" : " W") << std::defaultfloat << '\n';
                    }
                    ImGui::Text("Wheel: zoom | Drag: pan | Click: fly to location");
                }
            }
            if (UiTheme::SmallFont) ImGui::PushFont(UiTheme::SmallFont);
            for (const std::string &fact : b.facts) {
                ImGui::Bullet();
                ImGui::SameLine();
                ImGui::PushTextWrapPos();
                ImGui::TextColored(UiTheme::Secondary, "%s", fact.c_str());
                ImGui::PopTextWrapPos();
            }
            if (UiTheme::SmallFont) ImGui::PopFont();
            const float actionWidth = (ImGui::GetContentRegionAvail().x - 10.0f) * 0.5f /
                                      UiTheme::UiScale;
            if (UiTheme::button("Start Quiz", true, ImVec2(actionWidth, 38)))
                beginQuiz();
            ImGui::SameLine();
            if (UiTheme::button("Back", false, ImVec2(actionWidth, 38))) {
                selected = -1;
                distance = overviewDistance;
                flying = true;
                flyElapsed = 0;
                startCamera = currentCamera;
                startTarget = cameraTarget;
            }
            ImGui::EndChild();
        }
        ImGui::Separator();
        if (gameState == GameState::FreeExplore)
            ImGui::Text("Esc: back to overview");
        if (selected == 2)
            ImGui::TextWrapped("N: cities %s | P: map panel %s", cityMarkersEnabled ? "ON" : "OFF",
                               mapPanelOpen ? "open" : "closed");
        ImGui::TextWrapped(
            "RMB drag: camera | Wheel: zoom | F: free-fly | WASD move | Shift faster");
        if (ImGui::CollapsingHeader("CONTROLS"))
            UiPanels::controlsList();
        ImGui::EndChild();
        ImGui::End();
        if (help) {
            UiPanels::beginCenteredCard("Controls", static_cast<float>(windowWidth),
                                        static_cast<float>(windowHeight), 620, 480);
            ImGui::TextColored(UiTheme::Accent, "CONTROLS");
            UiPanels::controlsList();
            UiPanels::endCenteredCard();
        }
        if (gameState == GameState::StartScreen) {
            UiPanels::beginCenteredCard("Solar System Explorer", static_cast<float>(windowWidth),
                                        static_cast<float>(windowHeight), 500, 270);
            if (UiTheme::TitleFont) ImGui::PushFont(UiTheme::TitleFont);
            ImGui::TextColored(UiTheme::White, "Explore the system");
            if (UiTheme::TitleFont) ImGui::PopFont();
            ImGui::TextWrapped(
                "Visit worlds and answer questions before the threat reaches the Sun.");
            if (UiTheme::button("Start Mission", true, ImVec2(-1, 40)))
                startMission();
            if (UiTheme::button("Free Explore", false, ImVec2(-1, 40)))
                startFreeExplore();
            UiPanels::endCenteredCard();
        }
        if (gameState == GameState::Won || gameState == GameState::Lost) {
            UiPanels::beginCenteredCard("Mission Result", static_cast<float>(windowWidth),
                                        static_cast<float>(windowHeight), 460, 250);
            ImGui::TextColored(UiTheme::Accent, gameState == GameState::Won ? "MISSION COMPLETE"
                                                                            : "MISSION ENDED");
            if (UiTheme::TitleFont) ImGui::PushFont(UiTheme::TitleFont);
            ImGui::TextUnformatted(gameState == GameState::Won ? "Mission complete"
                                                               : "The Sun was swallowed");
            if (UiTheme::TitleFont) ImGui::PopFont();
            ImGui::Text("Final score: %d / %d", missionScore, MISSION_TARGET_SCORE);
            if (UiTheme::button("Restart", true, ImVec2(-1, 40)))
                startMission();
            if (gameState == GameState::Lost && UiTheme::button("Back to menu", false, ImVec2(-1, 38)))
                returnToMenu();
            UiPanels::endCenteredCard();
        }
        if (gameState == GameState::FreeExplore && blackHole.isSunSwallowed() &&
            !freeExploreSwallowedDismissed) {
            UiPanels::beginCenteredCard("Free Explore Ending", static_cast<float>(windowWidth),
                                        static_cast<float>(windowHeight), 460, 230);
            ImGui::TextColored(UiTheme::Accent, "SIMULATION ENDED");
            if (UiTheme::TitleFont) ImGui::PushFont(UiTheme::TitleFont);
            ImGui::TextUnformatted("The Sun was swallowed");
            if (UiTheme::TitleFont) ImGui::PopFont();
            if (UiTheme::button("Restart", true, ImVec2(-1, 40)))
                startFreeExplore();
            if (UiTheme::button("Keep watching", false, ImVec2(-1, 38)))
                freeExploreSwallowedDismissed = true;
            UiPanels::endCenteredCard();
        }
        if (quizOpen) {
            UiPanels::beginCenteredCard("Planet Quiz", static_cast<float>(windowWidth),
                                        static_cast<float>(windowHeight), 560, 390, &quizOpen);
            if (quizFinished || quizQuestionIndex >= quizRound.size()) {
                ImGui::Text("Quiz complete");
                ImGui::Text("Final score: %d / %d", quizScore, static_cast<int>(quizRound.size()));
                if (UiTheme::button("Close", true, ImVec2(-1, 38))) {
                    quizOpen = false;
                    paused = quizWasPaused;
                }
            } else {
                const QuizQuestion &q = questions[quizRound[quizQuestionIndex]];
                ImGui::Text("Question %d / %d | Score %d", static_cast<int>(quizQuestionIndex + 1),
                            static_cast<int>(quizRound.size()), quizScore);
                ImGui::Separator();
                ImGui::TextWrapped("%s", q.question.c_str());
                if (!quizAnswered) {
                    for (int i = 0; i < 4; ++i)
                        if (UiTheme::button(quizDisplayChoices[static_cast<std::size_t>(i)].c_str(), false, ImVec2(-1, 36))) {
                            quizChoice = i;
                            quizAnswered = true;
                            const bool correct = quizDisplayChoices[static_cast<std::size_t>(i)] == q.answer;
                            if (correct)
                                ++quizScore;
                            recordQuizAnswer(correct);
                        }
                } else {
                    ImGui::TextColored(
                        quizChoice >= 0 &&
                                quizDisplayChoices[static_cast<std::size_t>(quizChoice)] == q.answer
                            ? ImVec4(0.3f, 1, 0.4f, 1)
                            : ImVec4(1, 0.35f, 0.3f, 1),
                        "%s",
                        quizChoice >= 0 &&
                                quizDisplayChoices[static_cast<std::size_t>(quizChoice)] == q.answer
                            ? "Correct!"
                            : "Not quite.");
                    if (quizChoice < 0 ||
                        quizDisplayChoices[static_cast<std::size_t>(quizChoice)] != q.answer)
                        ImGui::Text("Correct answer: %s", q.answer.c_str());
                    if (UiTheme::button(quizQuestionIndex + 1 == quizRound.size() ? "Show result"
                                                                                  : "Next", true, ImVec2(-1, 38))) {
                        ++quizQuestionIndex;
                        quizAnswered = false;
                        if (quizQuestionIndex >= quizRound.size()) {
                            quizFinished = true;
                        } else {
                            quizChoice = -1;
                            shuffleDisplayedChoices();
                        }
                    }
                }
                if (UiTheme::button("Close", false, ImVec2(-1, 36))) {
                    quizOpen = false;
                    paused = quizWasPaused;
                }
            }
            UiPanels::endCenteredCard();
            if (!quizOpen)
                paused = quizWasPaused;
        }
        ImGui::PopFont();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (screenshotRequested) {
            saveScreenshot();
            screenshotRequested = false;
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}
