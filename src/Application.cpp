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
#include <cctype>
#include <cfloat>
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
constexpr float kMilkyWayCameraYawDeg = -90.0f; // Negative means left; tune this live.
constexpr float kTrailSampleSeconds = 0.12f;
constexpr std::size_t kTrailCapacity = 1200;
constexpr float kLensingDemoHoldSeconds = 7.0f;
constexpr float kSidebarExpandedWidth = 150.0f;
constexpr float kSidebarCollapsedWidth = 56.0f;
constexpr float kSidebarFlyoutWidth = 320.0f;
constexpr float kSidebarMargin = 12.0f;
constexpr float kSidebarGap = 10.0f;
constexpr float kHudMaxWidth = 620.0f;
constexpr std::size_t ASTEROID_COUNT_HIGH = 4000;
constexpr std::size_t ASTEROID_COUNT_LOW = 1800;
// Three-radius clearances cross in this stylized system (8.0 > 6.95); use 1.5R margins.
constexpr float ASTEROID_BELT_INNER_RADIUS = 7.1f + 0.30f * 1.5f;
constexpr float ASTEROID_BELT_OUTER_RADIUS = 9.5f - 0.85f * 1.5f;
constexpr float ASTEROID_VERTICAL_SPREAD = 0.20f;

struct SidebarLayout {
    float sidebarX = 0.0f;
    float sidebarY = 0.0f;
    float sidebarWidth = 0.0f;
    float panelHeight = 0.0f;
    float flyoutX = 0.0f;
    float flyoutWidth = 0.0f;
    float hudWidth = 0.0f;
    float hudHeight = 0.0f;
};

SidebarLayout sidebarLayoutForWindow(int logicalWidth, int logicalHeight, float scale,
                                     bool collapsed, bool flyoutOpen) {
    SidebarLayout layout;
    const float windowWidth = std::max(0.0f, static_cast<float>(logicalWidth));
    const float windowHeight = std::max(0.0f, static_cast<float>(logicalHeight));
    const float uiScale = std::max(0.0f, scale);
    const float margin = std::min(kSidebarMargin * uiScale, windowWidth * 0.02f);
    const float maxOverlayWidth = std::min(std::max(0.0f, windowWidth - 2.0f * margin),
                                           windowWidth * 0.55f);
    const float sidebarTarget = (collapsed ? kSidebarCollapsedWidth : kSidebarExpandedWidth) * uiScale;
    layout.sidebarWidth = std::min(sidebarTarget, maxOverlayWidth);
    float gap = flyoutOpen ? std::min(kSidebarGap * uiScale,
                                      std::max(0.0f, maxOverlayWidth - layout.sidebarWidth)) : 0.0f;
    if (flyoutOpen) {
        layout.flyoutWidth = std::min(kSidebarFlyoutWidth * uiScale,
                                      std::max(0.0f, maxOverlayWidth - layout.sidebarWidth - gap));
        if (layout.flyoutWidth <= 0.0f) gap = 0.0f;
    }
    layout.sidebarX = std::clamp(windowWidth - margin - layout.sidebarWidth, 0.0f,
                                 std::max(0.0f, windowWidth - layout.sidebarWidth));
    layout.flyoutX = std::max(0.0f, layout.sidebarX - gap - layout.flyoutWidth);
    layout.panelHeight = std::max(0.0f, windowHeight - 2.0f *
                                  std::min(kSidebarMargin * uiScale, windowHeight * 0.02f));
    layout.sidebarY = std::max(0.0f, (windowHeight - layout.panelHeight) * 0.5f);
    const float hudRight = flyoutOpen && layout.flyoutWidth > 0.0f
                               ? layout.flyoutX : layout.sidebarX;
    layout.hudWidth = std::min(kHudMaxWidth * uiScale,
                               std::max(0.0f, hudRight - 2.0f * margin));
    layout.hudHeight = std::min(windowHeight, 128.0f * uiScale);
    return layout;
}

void drawSidebarIcon(ImDrawList *draw, ImVec2 center, float size, int icon, ImU32 color) {
    const float u = size / 18.0f;
    const float stroke = std::max(1.0f, u * 1.6f);
    if (icon == 0) {
        for (int row = 0; row < 2; ++row)
            for (int col = 0; col < 2; ++col) {
                const ImVec2 a(center.x + (col ? 1.5f : -6.5f) * u,
                               center.y + (row ? 1.5f : -6.5f) * u);
                draw->AddRect(a, ImVec2(a.x + 5.0f * u, a.y + 5.0f * u), color, 1.0f * u, 0, stroke);
            }
    } else if (icon == 1) {
        draw->AddCircle(center, 7.0f * u, color, 0, stroke);
        draw->AddLine(center, ImVec2(center.x, center.y - 4.0f * u), color, stroke);
        draw->AddLine(center, ImVec2(center.x + 4.0f * u, center.y + 2.0f * u), color, stroke);
    } else if (icon == 2) {
        for (int row = -1; row <= 1; ++row) {
            const float y = center.y + row * 5.5f * u;
            draw->AddLine(ImVec2(center.x - 7.0f * u, y), ImVec2(center.x + 7.0f * u, y), color, stroke);
            draw->AddCircleFilled(ImVec2(center.x + (row == 0 ? 2.0f : -2.0f) * u, y), 1.8f * u, color);
        }
    } else if (icon == 3) {
        draw->AddLine(ImVec2(center.x - 5.0f * u, center.y + 7.0f * u),
                      ImVec2(center.x - 5.0f * u, center.y - 7.0f * u), color, stroke);
        draw->AddTriangleFilled(ImVec2(center.x - 4.0f * u, center.y - 7.0f * u),
                                ImVec2(center.x + 7.0f * u, center.y - 4.0f * u),
                                ImVec2(center.x - 4.0f * u, center.y), color);
        draw->AddLine(ImVec2(center.x - 8.0f * u, center.y + 8.0f * u),
                      ImVec2(center.x + 7.0f * u, center.y + 8.0f * u), color, stroke);
    } else {
        draw->AddCircle(center, 7.0f * u, color, 0, stroke);
        draw->AddLine(ImVec2(center.x - 2.0f * u, center.y - 2.0f * u),
                      ImVec2(center.x + 1.0f * u, center.y - 5.0f * u), color, stroke);
        draw->AddLine(ImVec2(center.x + 1.0f * u, center.y - 5.0f * u),
                      ImVec2(center.x + 4.0f * u, center.y - 2.0f * u), color, stroke);
        draw->AddLine(ImVec2(center.x + 4.0f * u, center.y - 2.0f * u),
                      ImVec2(center.x, center.y + 1.0f * u), color, stroke);
        draw->AddCircleFilled(ImVec2(center.x, center.y + 4.5f * u), 0.9f * u, color);
    }
}

bool drawSidebarItem(const char *id, const char *label, int icon, bool active,
                     bool collapsed, float width, float height) {
    width = std::max(0.0f, width);
    height = std::max(0.0f, height);
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList *draw = ImGui::GetWindowDrawList();
    if (active)
        draw->AddRectFilled(position, ImVec2(position.x + width, position.y + height),
                            ImGui::ColorConvertFloat4ToU32(UiTheme::Accent), 10.0f * UiTheme::UiScale);
    else if (hovered)
        draw->AddRectFilled(position, ImVec2(position.x + width, position.y + height),
                            IM_COL32(255, 255, 255, 12), 10.0f * UiTheme::UiScale);
    const ImU32 tint = ImGui::ColorConvertFloat4ToU32(active ? UiTheme::NearBlack : UiTheme::Secondary);
    drawSidebarIcon(draw, ImVec2(position.x + height * 0.5f, position.y + height * 0.5f),
                    std::min(18.0f * UiTheme::UiScale, height * 0.56f), icon, tint);
    if (!collapsed) {
        const ImVec2 textSize = ImGui::CalcTextSize(label);
        draw->AddText(ImVec2(position.x + height + 5.0f * UiTheme::UiScale,
                             position.y + (height - textSize.y) * 0.5f), tint, label);
    } else if (hovered) {
        ImGui::SetTooltip("%s", label);
    }
    return clicked;
}

void drawQuickIcon(ImDrawList *draw, ImVec2 center, float radius, int icon, ImU32 color) {
    const float r = std::max(1.0f, radius * 0.48f);
    const float stroke = std::max(1.0f, radius * 0.10f);
    if (icon == 0) {
        draw->AddLine(ImVec2(center.x - r, center.y), ImVec2(center.x + r, center.y), color, stroke);
        draw->AddLine(ImVec2(center.x, center.y - r), ImVec2(center.x, center.y + r), color, stroke);
        draw->AddLine(ImVec2(center.x - r * 0.7f, center.y - r * 0.7f),
                      ImVec2(center.x + r * 0.7f, center.y + r * 0.7f), color, stroke);
        draw->AddLine(ImVec2(center.x - r * 0.7f, center.y + r * 0.7f),
                      ImVec2(center.x + r * 0.7f, center.y - r * 0.7f), color, stroke);
    } else if (icon == 1) {
        draw->AddCircle(center, r * 0.72f, color, 0, stroke);
        draw->AddLine(ImVec2(center.x - r, center.y), ImVec2(center.x + r, center.y), color, stroke);
    } else {
        draw->AddTriangleFilled(ImVec2(center.x - r, center.y - r * 0.55f),
                                ImVec2(center.x - r, center.y + r * 0.55f),
                                ImVec2(center.x, center.y + r * 0.55f), color);
        draw->AddLine(ImVec2(center.x, center.y - r * 0.55f),
                      ImVec2(center.x, center.y + r * 0.55f), color, stroke);
        draw->AddLine(ImVec2(center.x + r * 0.35f, center.y - r * 0.4f),
                      ImVec2(center.x + r, center.y), color, stroke);
        draw->AddLine(ImVec2(center.x + r, center.y),
                      ImVec2(center.x + r * 0.35f, center.y + r * 0.4f), color, stroke);
    }
}

float scrollDelta = 0.0f;
void scrollCallback(GLFWwindow *, double, double y) {
    scrollDelta += static_cast<float>(y);
}

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
        const ImU32 labelColor = ImGui::ColorConvertFloat4ToU32(ImVec4(
            UiTheme::Accent.x, UiTheme::Accent.y, UiTheme::Accent.z, 0.90f * fade));
        drawList->AddText(UiTheme::LabelFont, UiTheme::LabelFont->FontSize, point, labelColor,
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

bool Application::init(bool introEnabled) {
    if (!glfwInit())
        return false;
    audio.initialize();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window =
        glfwCreateWindow(width, height, "Event Horizon: Solar System Explorer", nullptr, nullptr);
    if (!window)
        return false;
    intro.start(introEnabled);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetScrollCallback(window, scrollCallback);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        return false;
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
    const float cameraDistance = distance;
    const float cameraYaw = yaw + glm::radians(kMilkyWayCameraYawDeg) * galaxyBlend;
    return cameraTarget + glm::vec3(cameraDistance * std::cos(pitch) * std::sin(cameraYaw),
                                    cameraDistance * std::sin(pitch),
                                    cameraDistance * std::cos(pitch) * std::cos(cameraYaw));
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

void Application::setGalaxyViewTarget(bool enabled) {
    if (enabled && !galaxyViewTarget) {
        const glm::vec3 before = currentCamera;
        const float horizontalRadius = distance * std::cos(pitch);
        const glm::vec3 oldLeft(-horizontalRadius * std::cos(yaw), 0.0f,
                                horizontalRadius * std::sin(yaw));
        const glm::vec3 after = cameraTarget + glm::vec3(
            oldLeft.x, distance * std::sin(pitch), oldLeft.z);
        std::cout << "Milky Way camera transition: before (" << before.x << ", " << before.y
                  << ", " << before.z << "), after target (" << after.x << ", " << after.y
                  << ", " << after.z << "), previous left horizontal (" << oldLeft.x
                  << ", 0, " << oldLeft.z << ")\n";
        if (galaxyTransition == 0.0f)
            for (GalaxyTrail &trail : galaxyTrails) { trail.head = 0; trail.count = 0; }
    }
    galaxyViewTarget = enabled;
}

void Application::updateGalaxy(float deltaTime, float simulationDelta) {
    const float oldBlend = galaxyBlend;
    if (gameState == GameState::FreeExplore && galaxyViewTarget) galaxyTime += simulationDelta;
    const float direction = galaxyViewTarget ? 1.0f : -1.0f;
    galaxyTransition = std::clamp(galaxyTransition + direction * deltaTime / 2.0f, 0.0f, 1.0f);
    galaxyBlend = galaxyTransition * galaxyTransition * (3.0f - 2.0f * galaxyTransition);
    if (simulationDelta > 0.0f && galaxyViewTarget && gameState == GameState::FreeExplore && galaxyBlend > 0.0f) {
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
    sidebarSection = 0;
    sidebarFlyoutOpen = true;
    factsExpanded = false;
    audio.playBlip();
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
    lensingDemoPending = false;
    lensingDemoActive = false;
    lensingDemoReturning = false;
    lensingDemoElapsed = 0.0f;
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
    skyShader.set("highQuality", highQuality ? 1 : 0);
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
    if (intro.active()) {
        bool skip = false;
        for (int button = GLFW_MOUSE_BUTTON_1; button <= GLFW_MOUSE_BUTTON_LAST; ++button)
            skip = skip || glfwGetMouseButton(window, button) == GLFW_PRESS;
        for (int key = 0; !skip && key <= GLFW_KEY_LAST; ++key)
            skip = glfwGetKey(window, key) == GLFW_PRESS;
        intro.update(deltaTime, skip);
        lastSpaceDown = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
        lastFDown = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
        lastGDown = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
        lastLDown = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
        lastIDown = glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS;
        lastMDown = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
        lastQDown = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
        lastBDown = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
        lastF1Down = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
        lastHDown = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
        lastKDown = glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS;
        lastRDown = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
        lastEscapeDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        lastYDown = glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS;
        lastNDown = glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS;
        lastPDown = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
        lastF12Down = glfwGetKey(window, GLFW_KEY_F12) == GLFW_PRESS;
        lastTabDown = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
        lastEscapeDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        lastLeftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        return;
    }
    if (lensingDemoPending && gameState == GameState::StartScreen) {
        lensingDemoPending = false;
        lensingDemoActive = true;
        lensingDemoReturning = false;
        lensingDemoElapsed = 0.0f;
        blackHole.summon();
    }
    if (lensingDemoActive && gameState == GameState::StartScreen && !lensingDemoReturning) {
        bool skipDemo = false;
        for (int button = GLFW_MOUSE_BUTTON_1; button <= GLFW_MOUSE_BUTTON_LAST; ++button)
            skipDemo = skipDemo || glfwGetMouseButton(window, button) == GLFW_PRESS;
        for (int key = 0; !skipDemo && key <= GLFW_KEY_LAST; ++key)
            skipDemo = glfwGetKey(window, key) == GLFW_PRESS;
        if (skipDemo) {
            blackHole.reset();
            lensingDemoActive = false;
            lensingDemoReturning = false;
            lensingDemoElapsed = 0.0f;
        }
    }
    const bool escape = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (escape && !lastEscapeDown) {
        selected = -1;
        factsExpanded = false;
        distance = overviewDistance;
        flying = true;
        flyElapsed = 0;
        startCamera = currentCamera;
        startTarget = cameraTarget;
    }
    lastEscapeDown = escape;
    const bool space = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (space && !lastSpaceDown)
        paused = !paused;
    lastSpaceDown = space;
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
    const bool keyK = glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS;
    if (keyK && !lastKDown) audio.toggleMute();
    lastKDown = keyK;
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
        setGalaxyViewTarget(!galaxyViewTarget);
        std::cout << "View: " << (galaxyViewTarget ? "Milky Way" : "Solar System") << '\n';
    }
    lastYDown = keyY;
    const bool f12 = glfwGetKey(window, GLFW_KEY_F12) == GLFW_PRESS;
    const bool tab = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
    if (tab && !lastTabDown)
        sidebarCollapsed = !sidebarCollapsed;
    lastTabDown = tab;
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
                    setGalaxyViewTarget(false);
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
    bool overUi = ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse;
    double x = 0, y = 0;
    glfwGetCursorPos(window, &x, &y);
    const SidebarLayout layout = sidebarLayoutForWindow(windowWidth, windowHeight, UiTheme::UiScale,
                                                         sidebarCollapsed, sidebarFlyoutOpen);
    const bool overSidebar = x >= layout.sidebarX && x <= layout.sidebarX + layout.sidebarWidth &&
                             y >= layout.sidebarY && y <= layout.sidebarY + layout.panelHeight;
    const bool overFlyout = sidebarFlyoutOpen && layout.flyoutWidth > 0.0f &&
                            x >= layout.flyoutX && x <= layout.flyoutX + layout.flyoutWidth &&
                            y >= layout.sidebarY && y <= layout.sidebarY + layout.panelHeight;
    const float hudLeft = std::min(kSidebarMargin * UiTheme::UiScale,
                                   static_cast<float>(windowWidth) * 0.02f);
    const bool overHud = x >= hudLeft && x <= hudLeft + layout.hudWidth &&
                         y >= hudLeft && y <= hudLeft + layout.hudHeight;
    overUi = overUi || overSidebar || overFlyout || overHud;
    overUi = overUi || help || quizOpen || (gameState == GameState::FreeExplore && blackHole.isSunSwallowed() &&
                                !freeExploreSwallowedDismissed);
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
        } else if (gameState == GameState::StartScreen && lensingDemoActive) {
            blackHole.update(dt);
            if (!lensingDemoReturning) {
                lensingDemoElapsed += dt;
                if (lensingDemoElapsed >= kLensingDemoHoldSeconds) {
                    blackHole.returnBack();
                    lensingDemoReturning = true;
                }
            }
            if (lensingDemoReturning && blackHole.isIdle()) {
                lensingDemoActive = false;
                lensingDemoReturning = false;
                lensingDemoElapsed = 0.0f;
            }
        }
        updateGalaxy(dt, simulationDelta);
        if (selected >= 0 && bodyEffect(selected).swallowed) {
            selected = -1;
            distance = overviewDistance;
            flying = true;
            flyElapsed = 0.0f;
            startCamera = currentCamera;
            startTarget = cameraTarget;
        }
        const glm::vec3 modeCameraOffset = glm::vec3(
            glm::rotate(glm::mat4(1.0f), glm::radians(kMilkyWayCameraYawDeg) * galaxyBlend,
                        glm::vec3(0, 1, 0)) * glm::vec4(flyCameraOffset, 0.0f));
        if (flying) {
            flyElapsed += dt;
            const float t = std::clamp(flyElapsed / 1.5f, 0.0f, 1.0f);
            const float eased =
                t * t * (3.0f - 2.0f); // Smoothstep eases both ends of the camera path.
            const glm::vec3 endTarget = selected == 2 && earthMapTargetActive
                ? glm::vec3(earthModelMatrix() * glm::vec4(earthMapTargetLocal, 1.0f))
                : (selected >= 0 ? bodyEffect(selected).position : glm::vec3(0));
            const glm::vec3 endCamera = selected >= 0
                                            ? endTarget + modeCameraOffset
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
            currentCamera = target + modeCameraOffset;
        } else if (!freeFly) {
            currentCamera = cameraPosition();
        }
        int soundWorld = -1;
        float soundDistance = 1000.0f;
        float soundFade = 1.0f;
        if (selected >= 0) {
            soundWorld = selected == 8 ? 9 : selected < 3 ? selected : selected + 1;
            const BodyEffect effect = bodyEffect(selected);
            soundDistance = glm::length(currentCamera - effect.position);
            soundFade = effect.swallowed ? 0.0f : effect.opacity;
        } else if (gameplayActive) {
            for (int i = 0; i <= 8; ++i) {
                const BodyEffect effect = bodyEffect(i);
                const float candidate = glm::length(currentCamera - effect.position);
                if (!effect.swallowed && candidate < soundDistance) {
                    soundDistance = candidate;
                    soundWorld = i == 8 ? 9 : i < 3 ? i : i + 1;
                    soundFade = effect.opacity;
                }
            }
            const BodyEffect moonEffect = blackHole.affectBody(earthMoonPosition(), false);
            const float moonDistance = glm::length(currentCamera - moonEffect.position);
            if (!moonEffect.swallowed && moonDistance < soundDistance) {
                soundWorld = 3;
                soundDistance = moonDistance;
                soundFade = moonEffect.opacity;
            }
            if (soundDistance > 24.0f) soundWorld = -1;
        }
        audio.update(soundWorld, soundDistance, soundFade, dt);
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

        UiTheme::updateDpi(window);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        UiTheme::beginFrame();
        if (intro.active()) {
            intro.draw(static_cast<float>(windowWidth), static_cast<float>(windowHeight),
                       UiTheme::DpiScale);
        } else {
        const SidebarLayout sidebarLayout = sidebarLayoutForWindow(
            windowWidth, windowHeight, UiTheme::UiScale, sidebarCollapsed, sidebarFlyoutOpen);
        ImDrawList *hud = ImGui::GetForegroundDrawList();
        ImFont *readoutFont = UiTheme::SmallDataFont ? UiTheme::SmallDataFont : ImGui::GetFont();
        const float readoutSize = readoutFont->FontSize;
        const float hudPadding = 10.0f * UiTheme::UiScale;
        const float hudLeft = std::min(kSidebarMargin * UiTheme::UiScale,
                                       static_cast<float>(windowWidth) * 0.02f);
        const float hudTop = std::min(kSidebarMargin * UiTheme::UiScale,
                                      static_cast<float>(windowHeight) * 0.02f);
        const float hudTextWidth = std::max(0.0f, sidebarLayout.hudWidth - 2.0f * hudPadding);
        std::string teamLine = "TEAM  ";
        for (std::size_t i = 0; i < UiTheme::kTeamMembers.size(); ++i) {
            if (i != 0) teamLine += "  ·  ";
            teamLine += UiTheme::kTeamMembers[i];
        }
        const char *modeLabel = gameState == GameState::StartScreen ? "READY" :
            gameState == GameState::Mission ? "MISSION" : gameState == GameState::FreeExplore ? "FREE EXPLORE" :
            gameState == GameState::Won ? "MISSION COMPLETE" : "MISSION ENDED";
        char statusLine[96], toggleLineOne[96], toggleLineTwo[96];
        std::snprintf(statusLine, sizeof(statusLine), "MODE  %s   ·   SCORE  %d / %d",
                      modeLabel, missionScore, MISSION_TARGET_SCORE);
        std::snprintf(toggleLineOne, sizeof(toggleLineOne), "G %s   L %s   I %s   M %s",
                      bloom ? "ON" : "OFF", lensing ? "ON" : "OFF",
                      instancing ? "ON" : "OFF", meteorsEnabled ? "ON" : "OFF");
        std::snprintf(toggleLineTwo, sizeof(toggleLineTwo), "N %s   P %s   Q %s",
                      cityMarkersEnabled ? "ON" : "OFF", mapPanelOpen ? "ON" : "OFF",
                      highQuality ? "HIGH" : "LOW");
        const char *hudLines[] = {"EVENT HORIZON: SOLAR SYSTEM EXPLORER", teamLine.c_str(),
                                  statusLine, toggleLineOne, toggleLineTwo};
        ImVec2 hudLineSizes[5];
        float hudHeight = 2.0f * hudPadding;
        for (int i = 0; i < 5; ++i) {
            hudLineSizes[i] = readoutFont->CalcTextSizeA(readoutSize, FLT_MAX, hudTextWidth, hudLines[i]);
            hudHeight += hudLineSizes[i].y + 3.0f * UiTheme::UiScale;
        }
        if (sidebarLayout.hudWidth > 0.0f && windowHeight > 0) {
            const ImVec2 hudMin(hudLeft, hudTop);
            const ImVec2 hudMax(hudLeft + sidebarLayout.hudWidth, hudTop + hudHeight);
            const ImU32 hudBackground = ImGui::ColorConvertFloat4ToU32(UiTheme::Background);
            const ImU32 hudBorder = ImGui::ColorConvertFloat4ToU32(UiTheme::Hairline);
            hud->AddRectFilled(hudMin, hudMax, hudBackground, 14.0f * UiTheme::UiScale);
            hud->AddRect(hudMin, hudMax, hudBorder, 14.0f * UiTheme::UiScale, 0,
                         1.0f / UiTheme::DpiScale);
            float lineY = hudTop + hudPadding;
            const ImU32 titleColor = ImGui::ColorConvertFloat4ToU32(UiTheme::PanelText);
            const ImU32 infoColor = ImGui::ColorConvertFloat4ToU32(UiTheme::Secondary);
            for (int i = 0; i < 5; ++i) {
                hud->AddText(readoutFont, readoutSize, ImVec2(hudLeft + hudPadding, lineY),
                             i == 0 ? titleColor : infoColor, hudLines[i], nullptr,
                             hudTextWidth);
                lineY += hudLineSizes[i].y + 3.0f * UiTheme::UiScale;
            }
            if (lensingDemoActive) {
                const float demoY = hudTop + hudHeight + 5.0f * UiTheme::UiScale;
                const ImVec2 demoSize = readoutFont->CalcTextSizeA(readoutSize, FLT_MAX,
                    hudTextWidth, "LENSING DEMO  |  ANY KEY TO SKIP");
                hud->AddRectFilled(ImVec2(hudLeft, demoY - 4.0f * UiTheme::UiScale),
                    ImVec2(hudLeft + sidebarLayout.hudWidth, demoY + demoSize.y + 4.0f * UiTheme::UiScale),
                    hudBackground, 10.0f * UiTheme::UiScale);
                hud->AddText(readoutFont, readoutSize, ImVec2(hudLeft + hudPadding, demoY),
                    ImGui::ColorConvertFloat4ToU32(UiTheme::Accent),
                    "LENSING DEMO  |  ANY KEY TO SKIP", nullptr, hudTextWidth);
            }
        }
        drawCityLabels(view, projection);
        drawCityLabels(view, projection);
        const ImGuiWindowFlags sidebarFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        ImGui::SetNextWindowPos(ImVec2(sidebarLayout.sidebarX, sidebarLayout.sidebarY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(sidebarLayout.sidebarWidth, sidebarLayout.panelHeight), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(UiTheme::Background.w);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                            ImVec2((sidebarCollapsed ? 4.0f : 9.0f) * UiTheme::UiScale,
                                   9.0f * UiTheme::UiScale));
        if (ImGui::Begin("Solar Sidebar", nullptr, sidebarFlags)) {
            if (!sidebarCollapsed) {
                ImGui::TextColored(UiTheme::Secondary, "NAVIGATION");
                ImGui::Spacing();
            } else {
                ImGui::Dummy(ImVec2(0, 2.0f * UiTheme::UiScale));
            }
            const float footerHeight = 76.0f * UiTheme::UiScale;
            const float navHeight = std::max(0.0f, ImGui::GetContentRegionAvail().y - footerHeight);
            ImGui::BeginChild("sidebar_items", ImVec2(0, navHeight), false);
            const char *navLabels[] = {"OVERVIEW", "SIMULATION", "INSTRUMENTS", "MISSION", "CONTROLS"};
            for (int i = 0; i < 5; ++i) {
                if (drawSidebarItem(navLabels[i], navLabels[i], i,
                                    sidebarFlyoutOpen && sidebarSection == i,
                                    sidebarCollapsed, ImGui::GetContentRegionAvail().x,
                                    42.0f * UiTheme::UiScale)) {
                    if (sidebarFlyoutOpen && sidebarSection == i) {
                        sidebarFlyoutOpen = false;
                        sidebarSection = -1;
                    } else {
                        sidebarSection = i;
                        sidebarFlyoutOpen = true;
                    }
                }
                ImGui::Dummy(ImVec2(0, 3.0f * UiTheme::UiScale));
            }
            ImGui::EndChild();
            const float collapseHeight = 34.0f * UiTheme::UiScale;
            const ImVec2 collapsePosition = ImGui::GetCursorScreenPos();
            const float collapseWidth = std::max(0.0f, ImGui::GetContentRegionAvail().x);
            const bool collapseClicked = ImGui::InvisibleButton("sidebar_collapse",
                ImVec2(collapseWidth, collapseHeight));
            const bool collapseHovered = ImGui::IsItemHovered();
            ImDrawList *sidebarDraw = ImGui::GetWindowDrawList();
            if (collapseHovered)
                sidebarDraw->AddRectFilled(collapsePosition,
                    ImVec2(collapsePosition.x + collapseWidth, collapsePosition.y + collapseHeight),
                    IM_COL32(255, 255, 255, 12), 9.0f * UiTheme::UiScale);
            const float arrowX = collapsePosition.x + collapseHeight * 0.5f;
            const float arrowY = collapsePosition.y + collapseHeight * 0.5f;
            const float arrowDirection = sidebarCollapsed ? -1.0f : 1.0f;
            const ImU32 arrowColor = ImGui::ColorConvertFloat4ToU32(UiTheme::Secondary);
            sidebarDraw->AddLine(ImVec2(arrowX - arrowDirection * 3.0f * UiTheme::UiScale,
                                        arrowY - 5.0f * UiTheme::UiScale),
                                 ImVec2(arrowX + arrowDirection * 3.0f * UiTheme::UiScale, arrowY),
                                 arrowColor, 1.6f * UiTheme::UiScale);
            sidebarDraw->AddLine(ImVec2(arrowX + arrowDirection * 3.0f * UiTheme::UiScale, arrowY),
                                 ImVec2(arrowX - arrowDirection * 3.0f * UiTheme::UiScale,
                                        arrowY + 5.0f * UiTheme::UiScale),
                                 arrowColor, 1.6f * UiTheme::UiScale);
            if (!sidebarCollapsed) {
                ImFont *collapseFont = UiTheme::LabelFont ? UiTheme::LabelFont : ImGui::GetFont();
                const float collapseFontSize = collapseFont->FontSize;
                sidebarDraw->AddText(collapseFont, collapseFontSize,
                    ImVec2(arrowX + 13.0f * UiTheme::UiScale,
                    collapsePosition.y + (collapseHeight - collapseFontSize) * 0.5f), arrowColor,
                    "Collapse sidebar");
            } else if (collapseHovered) {
                ImGui::SetTooltip("Expand sidebar");
            }
            if (collapseClicked) sidebarCollapsed = !sidebarCollapsed;
            const float quickGap = 2.0f * UiTheme::UiScale;
            const float quickWidth = std::max(0.0f, ImGui::GetContentRegionAvail().x);
            const float quickDiameter = std::min(24.0f * UiTheme::UiScale,
                std::max(0.0f, (quickWidth - 2.0f * quickGap) / 3.0f));
            const char *quickNames[] = {"Bloom (G)", "Lensing (L)", "Mute ambience (K)"};
            for (int i = 0; i < 3 && quickDiameter > 0.0f; ++i) {
                if (i != 0) ImGui::SameLine(0, quickGap);
                const ImVec2 quickPosition = ImGui::GetCursorScreenPos();
                const bool quickClicked = ImGui::InvisibleButton(quickNames[i],
                    ImVec2(quickDiameter, quickDiameter));
                const bool quickHovered = ImGui::IsItemHovered();
                const bool active = i == 0 ? bloom : i == 1 ? lensing : !audio.muted();
                const ImVec2 center(quickPosition.x + quickDiameter * 0.5f,
                                    quickPosition.y + quickDiameter * 0.5f);
                const float radius = quickDiameter * 0.5f;
                sidebarDraw->AddCircleFilled(center, radius,
                    ImGui::ColorConvertFloat4ToU32(active ? UiTheme::Accent : UiTheme::Background));
                sidebarDraw->AddCircle(center, radius, ImGui::ColorConvertFloat4ToU32(UiTheme::Hairline),
                                       0, 1.0f / UiTheme::DpiScale);
                drawQuickIcon(sidebarDraw, center, radius,
                    i, ImGui::ColorConvertFloat4ToU32(active ? UiTheme::NearBlack : UiTheme::Secondary));
                if (quickHovered) ImGui::SetTooltip("%s", quickNames[i]);
                if (quickClicked) {
                    if (i == 0) bloom = !bloom;
                    else if (i == 1) lensing = !lensing;
                    else audio.toggleMute();
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();

        if (sidebarFlyoutOpen && sidebarLayout.flyoutWidth > 0.0f) {
            ImGui::SetNextWindowPos(ImVec2(sidebarLayout.flyoutX, sidebarLayout.sidebarY), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(sidebarLayout.flyoutWidth, sidebarLayout.panelHeight), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(UiTheme::Background.w);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                                ImVec2(14.0f * UiTheme::UiScale, 12.0f * UiTheme::UiScale));
            if (ImGui::Begin("Sidebar Flyout", nullptr, sidebarFlags)) {
                const char *flyoutTitle = sidebarSection == 0 ? "OVERVIEW" :
                    sidebarSection == 1 ? "SIMULATION" : sidebarSection == 2 ? "INSTRUMENTS" :
                    sidebarSection == 3 ? "MISSION" : "CONTROLS";
                ImGui::TextColored(UiTheme::Accent, "%s", flyoutTitle);
                const float closeX = std::max(ImGui::GetCursorPosX(),
                    ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - 28.0f * UiTheme::UiScale);
                ImGui::SameLine(closeX);
                if (UiTheme::GhostButton("×", ImVec2(24, 24))) {
                    sidebarFlyoutOpen = false;
                    sidebarSection = -1;
                }
                UiTheme::HairlineRule();
                ImGui::BeginChild("panel_content", ImVec2(0, 0), false);
        if (sidebarSection == 0) {
        UiTheme::SectionHeader("01", "OBJECT");
        auto drawWorldRow = [&](const char *name, const char *distanceText, const char *index, bool isSelected, auto onClick) {
            const float h = 30.0f * UiTheme::UiScale;
            const float w = std::max(0.0f, ImGui::GetContentRegionAvail().x);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::InvisibleButton(name, ImVec2(w, h));
            ImDrawList *dl = ImGui::GetWindowDrawList();
            const float dpi = UiTheme::DpiScale;
            auto snap = [dpi](float value) { return std::round(value * dpi) / dpi; };
            const ImVec2 start(snap(p.x), snap(p.y)), end(snap(p.x + w), snap(p.y + h));
            if (ImGui::IsItemHovered()) dl->AddRectFilled(start, end, IM_COL32(255,255,255,15));
            if (isSelected) dl->AddRectFilled(start, ImVec2(start.x + 2 * dpi, end.y),
                                               ImGui::ColorConvertFloat4ToU32(UiTheme::Accent));
            ImFont *mono = UiTheme::DataFont ? UiTheme::DataFont : ImGui::GetFont();
            dl->AddText(mono, mono->FontSize, ImVec2(p.x + 6 * UiTheme::UiScale, p.y + (h-mono->FontSize)*0.5f),
                        ImGui::ColorConvertFloat4ToU32(UiTheme::Secondary), index);
            dl->AddText(UiTheme::BodyFont, UiTheme::BodyFont->FontSize,
                        ImVec2(p.x + 42 * UiTheme::UiScale, p.y + (h-UiTheme::BodyFont->FontSize)*0.5f),
                        ImGui::ColorConvertFloat4ToU32(UiTheme::PanelText), name);
            const float vw = mono->CalcTextSizeA(mono->FontSize, FLT_MAX, 0, distanceText).x;
            dl->AddText(mono, mono->FontSize, ImVec2(end.x-vw, p.y+(h-mono->FontSize)*0.5f),
                        ImGui::ColorConvertFloat4ToU32(UiTheme::Secondary), distanceText);
            if (clicked) onClick();
        };
        if (selected < 0) {
            ImFont *tableLabel = UiTheme::LabelFont;
            const ImVec2 headPos = ImGui::GetCursorScreenPos();
            const float headSize = tableLabel->FontSize;
            const float contentWidth = ImGui::GetContentRegionAvail().x;
            const float distanceWidth = tableLabel->CalcTextSizeA(headSize, FLT_MAX, 0.0f, "FROM SUN · AU").x;
            ImDrawList *tableDraw = ImGui::GetWindowDrawList();
            tableDraw->AddText(tableLabel, headSize, ImVec2(headPos.x, headPos.y),
                ImGui::ColorConvertFloat4ToU32(UiTheme::Tertiary), "NO.");
            tableDraw->AddText(tableLabel, headSize, ImVec2(headPos.x + 42.0f * UiTheme::UiScale, headPos.y),
                ImGui::ColorConvertFloat4ToU32(UiTheme::Tertiary), "WORLD");
            tableDraw->AddText(tableLabel, headSize,
                ImVec2(headPos.x + contentWidth - distanceWidth, headPos.y),
                ImGui::ColorConvertFloat4ToU32(UiTheme::Tertiary), "FROM SUN · AU");
            ImGui::Dummy(ImVec2(0, headSize + 5.0f * UiTheme::UiScale));
            ImGui::BeginDisabled(!gameplayActive);
            for (std::size_t i=0; i<bodies.size(); ++i) {
                if (bodyEffect(static_cast<int>(i)).swallowed) continue;
                char idx[8], au[32]; std::snprintf(idx,sizeof(idx),"%02zu",i+1);
                std::snprintf(au,sizeof(au),"%.3f AU",bodies[i].distanceMillionKm/149.6);
                drawWorldRow(bodies[i].name.c_str(),au,idx,false,[&,i]{selectBody(static_cast<int>(i));});
            }
            if (!bodyEffect(8).swallowed) drawWorldRow("Sun","—","00",false,[&]{selectBody(8);});
            ImGui::EndDisabled();
        } else {
            const Body &b = selected==8 ? sunBody : bodies[static_cast<std::size_t>(selected)];
            if (UiTheme::GhostButton("< ALL WORLDS",ImVec2(142,26))) {
                selected=-1; factsExpanded=false; distance=overviewDistance; flying=true;
                flyElapsed=0; startCamera=currentCamera; startTarget=cameraTarget;
            }
            ImGui::Spacing();
            if (selected==8) UiTheme::Tag("G-TYPE STAR");
            else {
                const int ordinal=selected+1;
                const char *suffix=ordinal%100>=11 && ordinal%100<=13 ? "th" :
                    ordinal%10==1 ? "st" : ordinal%10==2 ? "nd" : ordinal%10==3 ? "rd" : "th";
                char category[128];
                std::snprintf(category,sizeof(category),"%s · %d%s FROM THE SUN",b.type.c_str(),ordinal,suffix);
                for(char *c=category;*c;++c)*c=static_cast<char>(std::toupper(static_cast<unsigned char>(*c)));
                UiTheme::Tag(category);
            }
            if(UiTheme::TitleFont)ImGui::PushFont(UiTheme::TitleFont);
            ImGui::TextWrapped("%s",b.name.c_str());
            if(UiTheme::TitleFont)ImGui::PopFont();
            std::string description;
            for(const std::string &fact:b.facts) {
                if(!description.empty())description+=" ";
                description+=fact;
                if(!fact.empty()&&fact.back()!='.'&&fact.back()!='!'&&fact.back()!='?')description+=".";
            }
            ImGui::PushTextWrapPos(); ImGui::TextColored(UiTheme::Secondary,"%s",description.c_str()); ImGui::PopTextWrapPos();
            ImGui::TextColored(UiTheme::Accent,factsExpanded?"LESS":"MORE");
            if(ImGui::IsItemClicked())factsExpanded=!factsExpanded;
            if(factsExpanded)for(const std::string &fact:b.facts)ImGui::BulletText("%s",fact.c_str());
            UiTheme::HairlineRule();
            char value[128];
            if(b.diameterKm>0){std::snprintf(value,sizeof(value),"%.0f",b.diameterKm);UiTheme::LabelValueRow("DIAMETER",value,"km");}
            if(b.massKg>0){std::snprintf(value,sizeof(value),"%.4g",b.massKg);UiTheme::LabelValueRow("MASS",value,"kg");}
            if(selected!=8&&b.distanceMillionKm>0){
                std::snprintf(value,sizeof(value),"%.1f",b.distanceMillionKm);UiTheme::LabelValueRow("MEAN DISTANCE",value,"10^6 km");
                std::snprintf(value,sizeof(value),"%.3f",b.distanceMillionKm/149.6);
                UiTheme::LabelValueRow("ORBITAL DISTANCE",value,"AU");
                std::snprintf(value,sizeof(value),"%.2f",b.distanceMillionKm*1000000.0/(299792.458*60.0));
                UiTheme::LabelValueRow("LIGHT TRAVEL TIME",value,"min");
            }
            if(b.orbitalDays>0){std::snprintf(value,sizeof(value),"%.5g",b.orbitalDays);UiTheme::LabelValueRow("ORBITAL PERIOD",value,"days");}
            if(b.rotationPeriodDays!=0){std::snprintf(value,sizeof(value),"%.5g",b.rotationPeriodDays);UiTheme::LabelValueRow("ROTATION PERIOD",value,"days");}
            if(b.gravity>0){std::snprintf(value,sizeof(value),"%.3g",b.gravity);UiTheme::LabelValueRow("SURFACE GRAVITY",value,"m/s²");}
            if(b.moons>=0){std::snprintf(value,sizeof(value),"%d",b.moons);UiTheme::LabelValueRow("MOONS",value,"moons");}
            if(b.name=="Earth"&&hasEarthCoordinate){
                char coord[96];std::snprintf(coord,sizeof(coord),"%.2f° %c / %.2f° %c",std::abs(pickedLatitude),pickedLatitude>=0?'N':'S',std::abs(pickedLongitude),pickedLongitude>=0?'E':'W');
                UiTheme::LabelValueRow("COORDINATES",coord,"");
            }
            if (b.name == "Earth" && distance <= 5.0f) {
                if (UiTheme::GhostButton(mapPanelOpen ? "HIDE EARTH MAP" : "EARTH MAP", ImVec2(-1, 28)))
                    mapPanelOpen = !mapPanelOpen;
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
                            mapDraw->AddCircleFilled(point, 2.0f, ImGui::ColorConvertFloat4ToU32(UiTheme::Accent));
                        }
                    }
                    const ImVec2 center((topLeft.x + bottomRight.x) * 0.5f, (topLeft.y + bottomRight.y) * 0.5f);
                    mapDraw->AddCircle(center, 5.0f, ImGui::ColorConvertFloat4ToU32(UiTheme::Accent), 0, 1.0f / UiTheme::DpiScale);
                    mapDraw->AddLine(ImVec2(center.x - 8, center.y), ImVec2(center.x + 8, center.y), ImGui::ColorConvertFloat4ToU32(UiTheme::Accent), 1.0f / UiTheme::DpiScale);
                    mapDraw->AddLine(ImVec2(center.x, center.y - 8), ImVec2(center.x, center.y + 8), ImGui::ColorConvertFloat4ToU32(UiTheme::Accent), 1.0f / UiTheme::DpiScale);
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
                    ImGui::TextWrapped("Wheel: zoom | Drag: pan | Click: fly to location");
                }
            }
            if (UiTheme::PrimaryButton("START QUIZ")) beginQuiz();
        }
        }
        if (sidebarSection == 1) {
        UiTheme::SectionHeader("02","SIMULATION");
        char days[48];std::snprintf(days,sizeof(days),"%.3f",simTime/86400.0);
        UiTheme::LabelValueRow("TIME",days,"days · stylised");
        ImGui::TextColored(UiTheme::Secondary,"SPEED");
        for(int option:{1,5,10}){
            if(option!=1)ImGui::SameLine(0,4*UiTheme::UiScale);
            char speedLabel[8];std::snprintf(speedLabel,sizeof(speedLabel),"%dx",option);
            if(speed==option)ImGui::TextColored(UiTheme::Accent,"%s",speedLabel);
            else if(UiTheme::GhostButton(speedLabel,ImVec2(48,25)))speed=option;
        }
        if(UiTheme::GhostButton(paused?"RESUME [SPACE]":"PAUSE [SPACE]",ImVec2(150,28)))paused=!paused;
        ImGui::TextColored(UiTheme::Secondary,"VIEW");
        const bool canChangeView=gameState==GameState::FreeExplore&&blackHole.isIdle();
        ImGui::BeginDisabled(!canChangeView);
        if (UiTheme::LabelFont) ImGui::PushFont(UiTheme::LabelFont);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f * UiTheme::UiScale,
                                                               6.0f * UiTheme::UiScale));
        if (ImGui::BeginTable("view_switch", 2, ImGuiTableFlags_SizingStretchSame |
                              ImGuiTableFlags_NoSavedSettings)) {
            ImGui::TableNextColumn();
            if (!galaxyViewTarget) ImGui::TextColored(UiTheme::Accent, "SOLAR SYSTEM");
            else if (UiTheme::GhostButton("SOLAR SYSTEM", ImVec2(-1, 27))) setGalaxyViewTarget(false);
            ImGui::TableNextColumn();
            if (galaxyViewTarget) ImGui::TextColored(UiTheme::Accent, "MILKY WAY");
            else if (UiTheme::GhostButton("MILKY WAY", ImVec2(-1, 27))) setGalaxyViewTarget(true);
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
        if (UiTheme::LabelFont) ImGui::PopFont();
        ImGui::EndDisabled();
        }
        if (sidebarSection == 2) {
        UiTheme::SectionHeader("03","INSTRUMENTS");
        if(UiTheme::ToggleRow("BLOOM","G",bloom))bloom=!bloom;
        if(UiTheme::ToggleRow("LENSING","L",lensing))lensing=!lensing;
        if(UiTheme::ToggleRow("INSTANCING","I",instancing))instancing=!instancing;
        if(UiTheme::ToggleRow("METEORS","M",meteorsEnabled))meteorsEnabled=!meteorsEnabled;
        if(UiTheme::ToggleRow("MARKERS","N",cityMarkersEnabled))cityMarkersEnabled=!cityMarkersEnabled;
        ImGui::BeginDisabled(selected!=2);
        if(UiTheme::ToggleRow("MAP","P",mapPanelOpen)){
            mapPanelOpen=!mapPanelOpen;
            if(mapPanelOpen&&distance>4.5f){
                distance=4.5f;flyCameraOffset=glm::vec3(distance*std::cos(pitch)*std::sin(yaw),distance*std::sin(pitch),distance*std::cos(pitch)*std::cos(yaw));
                startCamera=currentCamera;startTarget=cameraTarget;flyElapsed=0;flying=true;
            }
        }
        ImGui::EndDisabled();
        if(UiTheme::ToggleRow("QUALITY","Q",highQuality)){highQuality=!highQuality;applyQualitySettings();}
        const char *holeState=blackHole.getStateName();
        const bool danger=std::string(holeState)=="Consuming";
        ImGui::PushTextWrapPos();
        ImGui::TextColored(danger?UiTheme::Alert:UiTheme::Secondary,"BLACK HOLE  %s  %.0f%%",holeState,blackHole.getThreatProgress()*100);
        ImGui::PopTextWrapPos();
        const float holeButtonWidth=std::max(0.0f,
            (ImGui::GetContentRegionAvail().x/UiTheme::UiScale-4.0f)*0.5f);
        if(UiTheme::GhostButton("SUMMON [B]",ImVec2(holeButtonWidth,27))&&gameplayActive&&blackHole.isIdle()){
            if(galaxyViewTarget)setGalaxyViewTarget(false);freeExploreSwallowedDismissed=false;blackHole.summon();
        }
        ImGui::SameLine(0,4*UiTheme::UiScale);
        if(UiTheme::GhostButton("RETURN [R]",ImVec2(holeButtonWidth,27))){
            if(gameState==GameState::FreeExplore){if(blackHole.isSunSwallowed())startFreeExplore();else blackHole.returnBack();}
            else if(gameState==GameState::Mission||gameState==GameState::Won||gameState==GameState::Lost)startMission();
        }
        ImGui::TextColored(UiTheme::Secondary,"SOUND");
        if(audio.available()){
            ImGui::PushID("master_volume_track");
            const ImVec2 p=ImGui::GetCursorScreenPos();
            const float w=std::max(0.0f, ImGui::GetContentRegionAvail().x);
            const float y=p.y+6*UiTheme::DpiScale;
            ImGui::InvisibleButton("track",ImVec2(w,14*UiTheme::DpiScale));
            if(ImGui::IsItemActive()&&w>0.0f)audio.setMaster(std::clamp((ImGui::GetIO().MousePos.x-p.x)/w,0.0f,1.0f));
            const float fill=audio.master()*w;ImDrawList *dl=ImGui::GetWindowDrawList();
            dl->AddLine(ImVec2(p.x,y),ImVec2(p.x+w,y),ImGui::ColorConvertFloat4ToU32(UiTheme::Hairline),1/UiTheme::DpiScale);
            dl->AddLine(ImVec2(p.x,y),ImVec2(p.x+fill,y),ImGui::ColorConvertFloat4ToU32(UiTheme::Accent),2/UiTheme::DpiScale);
            dl->AddRectFilled(ImVec2(p.x+fill-4*UiTheme::DpiScale,y-4*UiTheme::DpiScale),ImVec2(p.x+fill+4*UiTheme::DpiScale,y+4*UiTheme::DpiScale),ImGui::ColorConvertFloat4ToU32(UiTheme::Accent));
            ImGui::PopID();
        }else ImGui::TextDisabled("%s",audio.status());
        if(UiTheme::ToggleRow("MUTE","K",audio.muted()))audio.toggleMute();
        }
        if (sidebarSection == 3) {
        UiTheme::SectionHeader("04","MISSION");
        ImGui::TextColored(UiTheme::Secondary,"STATUS");ImGui::SameLine();
        ImGui::TextColored(UiTheme::PanelText,"%s",gameState==GameState::StartScreen?"READY":gameState==GameState::Mission?"ACTIVE":gameState==GameState::FreeExplore?"FREE EXPLORE":gameState==GameState::Won?"COMPLETE":"ENDED");
        if(UiTheme::HeroFont)ImGui::PushFont(UiTheme::HeroFont);
        ImGui::Text("SCORE  %d / %d",missionScore,MISSION_TARGET_SCORE);
        if(UiTheme::HeroFont)ImGui::PopFont();
        if(gameState==GameState::Mission){
            char threat[32];std::snprintf(threat,sizeof(threat),"%.1f",activeThreat().getThreatProgress()*100.0f);
            UiTheme::LabelValueRow("THREAT",threat,"%");
        }
        if(gameState==GameState::StartScreen){
            if(UiTheme::PrimaryButton("START MISSION"))startMission();
            if(UiTheme::GhostButton("FREE EXPLORE"))startFreeExplore();
        } else if(gameState==GameState::FreeExplore){
            if(UiTheme::GhostButton("RESTART FREE EXPLORE"))startFreeExplore();
        } else {
            if(UiTheme::GhostButton("RESTART MISSION"))startMission();
        }
        }
        if (sidebarSection == 4) {
            UiTheme::SectionHeader("05", "CONTROLS");
            UiPanels::controlsList();
        }
        ImGui::EndChild();
            }
            ImGui::End();
            ImGui::PopStyleVar();
        }
        if (help) {
            UiPanels::beginCenteredCard("Controls",static_cast<float>(windowWidth),static_cast<float>(windowHeight),560,520);
            UiTheme::SectionHeader("","CONTROLS");
            ImGui::BeginChild("full_controls_list",ImVec2(0,0),false);
            UiPanels::controlsList();
            ImGui::EndChild();
            UiPanels::endCenteredCard();
        }
        if (gameState == GameState::StartScreen) {
            UiPanels::beginCenteredCard("Solar System Explorer",static_cast<float>(windowWidth),static_cast<float>(windowHeight),420,250);
            if(UiTheme::TitleFont)ImGui::PushFont(UiTheme::TitleFont);
            ImGui::TextColored(UiTheme::PanelText,"MISSION CONTROL");
            if(UiTheme::TitleFont)ImGui::PopFont();
            ImGui::PushTextWrapPos();
            ImGui::TextColored(UiTheme::Secondary,"Visit worlds and answer questions before the threat reaches the Sun.");
            ImGui::PopTextWrapPos();
            UiTheme::HairlineRule();
            if(UiTheme::PrimaryButton("START MISSION"))startMission();
            if(UiTheme::GhostButton("FREE EXPLORE"))startFreeExplore();
            UiPanels::endCenteredCard();
        }
        if(gameState==GameState::Won||gameState==GameState::Lost){
            UiPanels::beginCenteredCard("Mission Result",static_cast<float>(windowWidth),static_cast<float>(windowHeight),420,250);
            const bool lost=gameState==GameState::Lost;
            ImGui::TextColored(lost?UiTheme::Alert:UiTheme::Accent,lost?"THE SUN WAS SWALLOWED":"MISSION COMPLETE");
            if(UiTheme::TitleFont)ImGui::PushFont(UiTheme::TitleFont);
            ImGui::TextUnformatted(lost?"Mission ended":"Mission complete");
            if(UiTheme::TitleFont)ImGui::PopFont();
            if(UiTheme::DataFont)ImGui::PushFont(UiTheme::DataFont);
            ImGui::Text("SCORE  %d / %d",missionScore,MISSION_TARGET_SCORE);
            if(UiTheme::DataFont)ImGui::PopFont();
            if(UiTheme::PrimaryButton("RESTART"))startMission();
            if(lost&&UiTheme::GhostButton("BACK TO MENU"))returnToMenu();
            UiPanels::endCenteredCard();
        }
        if(gameState==GameState::FreeExplore&&blackHole.isSunSwallowed()&&!freeExploreSwallowedDismissed){
            UiPanels::beginCenteredCard("Free Explore Ending",static_cast<float>(windowWidth),static_cast<float>(windowHeight),420,230);
            ImGui::TextColored(UiTheme::Alert,"THE SUN WAS SWALLOWED");
            if(UiTheme::TitleFont)ImGui::PushFont(UiTheme::TitleFont);
            ImGui::TextUnformatted("Simulation ended");
            if(UiTheme::TitleFont)ImGui::PopFont();
            if(UiTheme::PrimaryButton("RESTART"))startFreeExplore();
            if(UiTheme::GhostButton("KEEP WATCHING"))freeExploreSwallowedDismissed=true;
            UiPanels::endCenteredCard();
        }
        if(quizOpen){
            UiPanels::beginCenteredCard("Planet Quiz",static_cast<float>(windowWidth),static_cast<float>(windowHeight),460,430,&quizOpen);
            if(quizFinished||quizQuestionIndex>=quizRound.size()){
                UiTheme::SectionHeader("","QUIZ COMPLETE");
                if(UiTheme::HeroFont)ImGui::PushFont(UiTheme::HeroFont);
                ImGui::Text("SCORE  %d / %d",quizScore,static_cast<int>(quizRound.size()));
                if(UiTheme::HeroFont)ImGui::PopFont();
                if(UiTheme::PrimaryButton("CLOSE")){quizOpen=false;paused=quizWasPaused;}
            }else{
                const QuizQuestion &q=questions[quizRound[quizQuestionIndex]];
                char title[96],progress[32];
                std::snprintf(title,sizeof(title),"QUIZ · %s",q.planet.c_str());
                std::snprintf(progress,sizeof(progress),"Q %02zu / %02zu",quizQuestionIndex+1,quizRound.size());
                UiTheme::Tag(title);
                ImGui::SameLine(ImGui::GetWindowWidth()-ImGui::CalcTextSize(progress).x-32*UiTheme::UiScale);
                if(UiTheme::DataFont)ImGui::PushFont(UiTheme::DataFont);
                ImGui::TextColored(UiTheme::PanelText,"%s",progress);
                if(UiTheme::DataFont)ImGui::PopFont();
                UiTheme::HairlineRule();
                ImGui::TextWrapped("%s",q.question.c_str());
                if(!quizAnswered){
                    for(int i=0;i<4;++i){
                        const char *answer=quizDisplayChoices[static_cast<std::size_t>(i)].c_str();
                        const float width=ImGui::GetContentRegionAvail().x;
                        const float textWidth=width-40*UiTheme::UiScale;
                        ImFont *font=UiTheme::BodyFont;
                        const ImVec2 measured=font->CalcTextSizeA(font->FontSize,FLT_MAX,textWidth,answer);
                        const float rowHeight=std::max(34*UiTheme::UiScale,measured.y+10*UiTheme::UiScale);
                        const ImVec2 pos=ImGui::GetCursorScreenPos();
                        const bool clicked=ImGui::InvisibleButton(answer,ImVec2(width,rowHeight));
                        ImDrawList *dl=ImGui::GetWindowDrawList();
                        if(ImGui::IsItemHovered()){
                            const float dpi=UiTheme::DpiScale;
                            auto snap=[dpi](float value){return std::round(value*dpi)/dpi;};
                            dl->AddRectFilled(ImVec2(snap(pos.x),snap(pos.y)),
                                ImVec2(snap(pos.x+width),snap(pos.y+rowHeight)),IM_COL32(255,255,255,15));
                        }
                        char letter[2]={static_cast<char>('A'+i),0};
                        dl->AddText(UiTheme::DataFont,UiTheme::DataFont->FontSize,
                            ImVec2(pos.x+4*UiTheme::UiScale,pos.y+5*UiTheme::UiScale),
                            ImGui::ColorConvertFloat4ToU32(UiTheme::Secondary),letter);
                        const ImVec2 textPos(pos.x+34*UiTheme::UiScale,pos.y+5*UiTheme::UiScale);
                        const ImVec4 clip(pos.x+32*UiTheme::UiScale,pos.y,pos.x+width,pos.y+rowHeight);
                        dl->AddText(font,font->FontSize,textPos,ImGui::ColorConvertFloat4ToU32(UiTheme::PanelText),
                                    answer,nullptr,textWidth,&clip);
                        dl->AddLine(ImVec2(pos.x,pos.y+rowHeight-0.5f/UiTheme::DpiScale),
                            ImVec2(pos.x+width,pos.y+rowHeight-0.5f/UiTheme::DpiScale),
                            ImGui::ColorConvertFloat4ToU32(UiTheme::Hairline),1/UiTheme::DpiScale);
                        if(clicked){
                            quizChoice=i;quizAnswered=true;
                            const bool correct=quizDisplayChoices[static_cast<std::size_t>(i)]==q.answer;
                            if(correct)++quizScore;
                            recordQuizAnswer(correct);
                        }
                    }
                }else{
                    const bool correct=quizChoice>=0&&quizDisplayChoices[static_cast<std::size_t>(quizChoice)]==q.answer;
                    const ImVec2 p=ImGui::GetCursorScreenPos();
                    ImGui::GetWindowDrawList()->AddLine(p,ImVec2(p.x+ImGui::GetContentRegionAvail().x,p.y),
                        ImGui::ColorConvertFloat4ToU32(correct?UiTheme::Accent:UiTheme::Alert),2/UiTheme::DpiScale);
                    ImGui::Dummy(ImVec2(0,6*UiTheme::UiScale));
                    ImGui::TextColored(correct?UiTheme::Accent:UiTheme::Alert,correct?"CORRECT":"INCORRECT");
                    if(!correct)ImGui::TextWrapped("Correct answer: %s",q.answer.c_str());
                    if(UiTheme::DataFont)ImGui::PushFont(UiTheme::DataFont);
                    ImGui::Text("SCORE %d / %d   THREAT SPEED %.2fx",quizScore,MISSION_TARGET_SCORE,threatSpeedMultiplier);
                    if(UiTheme::DataFont)ImGui::PopFont();
                    if(UiTheme::PrimaryButton(quizQuestionIndex+1==quizRound.size()?"SHOW RESULT":"NEXT QUESTION")){
                        ++quizQuestionIndex;quizAnswered=false;
                        if(quizQuestionIndex>=quizRound.size())quizFinished=true;
                        else{quizChoice=-1;shuffleDisplayedChoices();}
                    }
                }
                if(UiTheme::GhostButton("CLOSE")){quizOpen=false;paused=quizWasPaused;}
            }
            UiPanels::endCenteredCard();
            if(!quizOpen)paused=quizWasPaused;
        }
        }        ImGui::PopFont();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (screenshotRequested) {
            saveScreenshot();
            screenshotRequested = false;
        }
        glfwSwapBuffers(window);
        if (!glfwGetWindowAttrib(window, GLFW_VISIBLE))
            glfwShowWindow(window);
        glfwPollEvents();
    }
}
