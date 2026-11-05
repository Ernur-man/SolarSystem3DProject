#include "Application.h"

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
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <regex>
#include <sstream>

namespace {
constexpr float ORBIT_BASE_SECONDS = 120.0f;
constexpr float ORBIT_COMPRESSION_EXPONENT = 0.5f;
constexpr float ROTATION_SCALE = 0.0207f;
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
}

Application::~Application() {
    blackHole.release();
    postProcess.release();
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
        !orbitShader.load("shaders/orbit.vert", "shaders/orbit.frag"))
        return false;
    skyTexture.load("assets/textures/stars_milky_way.jpg", true, "procedural starfield");
    earthDayTexture.load("assets/textures/earth_day.jpg", true, "procedural Earth surface");
    earthNightTexture.load("assets/textures/earth_night.jpg", true, "procedural city lights");
    earthCloudTexture.load("assets/textures/earth_clouds.jpg", true, "procedural clouds");
    earthNormalTexture.load("assets/textures/earth_normal.jpg", false, "geometric normals");
    earthSpecularTexture.load("assets/textures/earth_specular.jpg", false,
                              "constant ocean and land specular values");
    saturnRingTexture.load("assets/textures/saturn_ring.png", true, "procedural ring bands");
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
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
    glfwGetFramebufferSize(window, &width, &height);
    if (!postProcess.initialize(width, height, highQuality ? 1.0f : 0.75f))
        return false;
    saturnRing.makeRing(1.35f, 2.6f);
    return blackHole.initialize(postProcess.getWidth(), postProcess.getHeight());
}

void Application::applyQualitySettings() {
    lowDetailSphere.makeSphere(highQuality ? 12u : 8u, highQuality ? 18u : 12u);
}

glm::vec3 Application::cameraPosition() const {
    return cameraTarget + glm::vec3(distance * std::cos(pitch) * std::sin(yaw),
                                    distance * std::sin(pitch),
                                    distance * std::cos(pitch) * std::cos(yaw));
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

void Application::selectBody(int index) {
    if (index < 0 || index > 8)
        return;
    const bool fromOverview = selected < 0;
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
    quizRound.clear();
    std::vector<std::size_t> candidates;
    const std::string name =
        selected == 8 ? "Sun" : bodies[static_cast<std::size_t>(selected)].name;
    for (std::size_t i = 0; i < questions.size(); ++i)
        if (questions[i].planet == name)
            candidates.push_back(i);
    std::mt19937 random(std::random_device{}());
    std::shuffle(candidates.begin(), candidates.end(), random);
    if (candidates.size() < 5)
        std::cerr << "Warning: fewer than five quiz questions available for " << name << '\n';
    if (candidates.size() > 5)
        candidates.resize(5);
    quizRound = candidates;
    quizQuestionIndex = 0;
    quizScore = 0;
    quizAnswered = false;
    quizFinished = quizRound.empty();
    quizWasPaused = paused;
    quizOpen = true;
}

void Application::startMission(bool summonHole) {
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
    quizAnswered = false;
    quizFinished = false;
    quizQuestionIndex = 0;
    quizScore = 0;
    quizChoice = -1;
    lightAngle = 0.0f;
    bloom = true;
    lensing = true;
    instancing = false;
    highQuality = true;
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
    const glm::mat4 baseTransform =
        glm::translate(glm::mat4(1.0f), effect.position) * glm::mat4(effect.basis) *
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
        activeShader.set("cameraPos", currentCamera);
        activeShader.set("emissive", 0.0f);
        const bool isSaturn = body.name == "Saturn";
        activeShader.set("ringEnabled", isSaturn ? 1 : 0);
        if (isSaturn) {
            const glm::mat4 ringOrientation =
                glm::mat4(effect.basis) *
                glm::rotate(glm::mat4(1.0f), rotationAngle, glm::vec3(0, 1, 0)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(27.0f), glm::vec3(1, 0, 0));
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
    const float rotationAngle = simTime * 6.2831853f * ROTATION_SCALE / saturn.rotationPeriodDays;
    const glm::mat4 orientation =
        glm::mat4(effect.basis) *
        glm::rotate(glm::mat4(1.0f), rotationAngle, glm::vec3(0, 1, 0)) *
        glm::rotate(glm::mat4(1.0f), glm::radians(27.0f), glm::vec3(1, 0, 0));
    const glm::mat4 model =
        glm::translate(glm::mat4(1.0f), effect.position) * orientation *
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
    ringShader.set("saturnCenter", effect.position);
    ringShader.set("ringNormal", glm::normalize(glm::vec3(orientation * glm::vec4(0, 0, 1, 0))));
    ringShader.set("saturnRadius", bodyRadius * effect.perpendicularScale);
    ringShader.set("innerRadius", 1.35f);
    ringShader.set("outerRadius", 2.6f);
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

void Application::drawOrbitLines(const glm::mat4 &view, const glm::mat4 &projection) {
    GLint oldProgram = 0;
    GLint oldVao = 0;
    GLint oldDepthFunction = GL_LESS;
    GLint oldSrcRgb = GL_ONE;
    GLint oldDstRgb = GL_ZERO;
    GLint oldSrcAlpha = GL_ONE;
    GLint oldDstAlpha = GL_ZERO;
    GLfloat oldLineWidth = 1.0f;
    GLboolean oldDepthWrite = GL_TRUE;
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
    glGetIntegerv(GL_DEPTH_FUNC, &oldDepthFunction);
    glGetIntegerv(GL_BLEND_SRC_RGB, &oldSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &oldDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &oldSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &oldDstAlpha);
    glGetFloatv(GL_LINE_WIDTH, &oldLineWidth);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &oldDepthWrite);
    const GLboolean oldDepthTest = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean oldCull = glIsEnabled(GL_CULL_FACE);
    const GLboolean oldBlend = glIsEnabled(GL_BLEND);

    // Guides are drawn below the opaque bodies so their silhouettes always cover them.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.0f);
    orbitShader.use();
    orbitShader.set("view", view);
    orbitShader.set("projection", projection);
    orbitShader.set("model", glm::mat4(1.0f));
    orbitShader.set("color", glm::vec3(0.26f, 0.31f, 0.42f));
    orbitShader.set("lineAlpha", 0.48f);
    for (const Mesh &orbitLine : orbitLines)
        orbitLine.drawOrbit();

    glDepthMask(oldDepthWrite);
    glDepthFunc(static_cast<GLenum>(oldDepthFunction));
    if (oldDepthTest)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (oldCull)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    glBlendFuncSeparate(static_cast<GLenum>(oldSrcRgb), static_cast<GLenum>(oldDstRgb),
                        static_cast<GLenum>(oldSrcAlpha), static_cast<GLenum>(oldDstAlpha));
    if (oldBlend)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    glLineWidth(oldLineWidth);
    glUseProgram(static_cast<GLuint>(oldProgram));
    glBindVertexArray(static_cast<GLuint>(oldVao));
}

void Application::drawSelectionRing(const glm::mat4 &view, const glm::mat4 &projection) {
    if (selected < 0)
        return;

    const BodyEffect effect = bodyEffect(selected);
    if (effect.swallowed || effect.opacity <= 0.0f)
        return;

    const float radius = selected == 8 ? 1.25f : bodies[static_cast<std::size_t>(selected)].radius;
    const float ringRadius = radius * effect.perpendicularScale * 1.35f;
    const glm::mat4 ringModel =
        glm::translate(glm::mat4(1.0f), effect.position) *
        glm::scale(glm::mat4(1.0f), glm::vec3(ringRadius, 1.0f, ringRadius));
    GLint oldProgram = 0;
    GLint oldVao = 0;
    GLint oldDepthFunction = GL_LESS;
    GLboolean oldDepthWrite = GL_TRUE;
    const GLboolean oldDepthTest = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean oldCull = glIsEnabled(GL_CULL_FACE);
    const GLboolean oldBlend = glIsEnabled(GL_BLEND);
    GLint oldSrcRgb = GL_ONE;
    GLint oldDstRgb = GL_ZERO;
    GLint oldSrcAlpha = GL_ONE;
    GLint oldDstAlpha = GL_ZERO;
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
    glGetIntegerv(GL_DEPTH_FUNC, &oldDepthFunction);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &oldDepthWrite);
    glGetIntegerv(GL_BLEND_SRC_RGB, &oldSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &oldDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &oldSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &oldDstAlpha);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    orbitShader.use();
    orbitShader.set("view", view);
    orbitShader.set("projection", projection);
    orbitShader.set("model", ringModel);
    orbitShader.set("color", glm::vec3(1.0f, 0.85f, 0.25f));
    orbitShader.set("lineAlpha", 0.92f * effect.opacity);
    highlightRing.drawOrbit();

    glDepthMask(oldDepthWrite);
    glDepthFunc(static_cast<GLenum>(oldDepthFunction));
    if (oldDepthTest)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (oldCull)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    glBlendFuncSeparate(static_cast<GLenum>(oldSrcRgb), static_cast<GLenum>(oldDstRgb),
                        static_cast<GLenum>(oldSrcAlpha), static_cast<GLenum>(oldDstAlpha));
    if (oldBlend)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    glUseProgram(static_cast<GLuint>(oldProgram));
    glBindVertexArray(static_cast<GLuint>(oldVao));
}

void Application::drawSky(const glm::mat4 &view, const glm::mat4 &projection) {
    skyShader.use();
    skyShader.set("view", glm::mat4(glm::mat3(view)));
    skyShader.set("projection", projection);
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
    const bool q = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
    const bool b = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
    const bool f1 = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
    const bool h = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
    const bool r = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
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
    if (q && !lastQDown) {
        highQuality = !highQuality;
        applyQualitySettings();
        std::cout << "Quality: " << (highQuality ? "High" : "Low") << '\n';
    }
    if (b && !lastBDown) {
        if (gameState == GameState::Mission || gameState == GameState::FreeExplore) {
            if (blackHole.isIdle())
                blackHole.summon();
            else
                blackHole.returnBack();
        }
    }
    if ((f1 && !lastF1Down) || (h && !lastHDown))
        help = !help;
    if (r && !lastRDown) {
        if (gameState == GameState::FreeExplore)
            blackHole.returnBack();
        else if (gameState == GameState::Mission || gameState == GameState::Won ||
                 gameState == GameState::Lost)
            startMission();
    }
    if (f12 && !lastF12Down)
        screenshotRequested = true;
    lastGDown = g;
    lastLDown = l;
    lastIDown = i;
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
    if (left && !lastLeftDown && !overUi && !quizOpen && gameplayActive) {
        int windowWidth = 1, windowHeight = 1;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        const float ndcX = 2.0f * static_cast<float>(x) / static_cast<float>(windowWidth) - 1.0f;
        const float ndcY = 1.0f - 2.0f * static_cast<float>(y) / static_cast<float>(windowHeight);
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
        if (hit >= 0)
            selectBody(hit);
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
        if (gameplayActive && !paused && !quizOpen)
            simTime += dt * static_cast<float>(speed);
        const float threatDeltaTime = !paused && !quizOpen ? dt * static_cast<float>(speed) : 0.0f;
        if (gameState == GameState::Mission)
            updateMission(threatDeltaTime);
        else if (gameState == GameState::FreeExplore)
            blackHole.update(threatDeltaTime);
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
            const glm::vec3 endTarget =
                selected >= 0 ? bodyEffect(selected).position : glm::vec3(0);
            const glm::vec3 endCamera = selected >= 0
                                            ? endTarget + flyCameraOffset
                                            : glm::vec3(distance * std::cos(pitch) * std::sin(yaw),
                                                        distance * std::sin(pitch),
                                                        distance * std::cos(pitch) * std::cos(yaw));
            cameraTarget = glm::mix(startTarget, endTarget, eased);
            currentCamera = glm::mix(startCamera, endCamera, eased);
            if (t >= 1.0f)
                flying = false;
        } else if (selected >= 0 && !freeFly) {
            const glm::vec3 target = bodyEffect(selected).position;
            cameraTarget = target;
            currentCamera = target + flyCameraOffset;
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
        shader.use();
        shader.set("view", view);
        shader.set("projection", projection);
        drawOrbitLines(view, projection);
        for (const Body &body : bodies)
            draw(body, view, projection);
        sunBody.radius = 1.25f;
        sunShader.use();
        sunShader.set("view", view);
        sunShader.set("projection", projection);
        draw(sunBody, view, projection, true);
        blackHole.draw(view, projection, currentCamera, simTime);
        drawSaturnRing(view, projection);
        drawSelectionRing(view, projection);

        postProcess.endScene();
        const GLuint hdrSceneTexture = postProcess.getHdrSceneColorTexture();
        const GLuint sceneTextureForComposite =
            blackHole.renderLensing(hdrSceneTexture, postProcess.getWidth(), postProcess.getHeight(),
                                    view, projection, lensing);
        postProcess.composite(sceneTextureForComposite, bloom, highQuality ? 8 : 4);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(static_cast<float>(width) - 350.0f, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(350, static_cast<float>(height)), ImGuiCond_Always);
        ImGui::Begin("Mission Control", nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        ImGui::Text("Team: Your Team");
        ImGui::Text("Event Horizon: Solar System Explorer");
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
        ImGui::Text("Status: %s | Score: %d / %d", missionStatus, missionScore,
                    MISSION_TARGET_SCORE);
        ImGui::Text("Bloom %s | Instancing %s", bloom ? "ON" : "OFF",
                    instancing ? "ON" : "OFF");
        ImGui::Text("Lensing: %s", lensing ? "ON" : "OFF");
        ImGui::Text("Quality: %s | Black hole: %s p=%.0f%% | Light %.1f deg",
                    highQuality ? "High" : "Low", blackHole.getStateName(),
                    blackHole.getThreatProgress() * 100.0f, lightAngle * 57.2957795f);
        if (gameState == GameState::Mission)
            ImGui::ProgressBar(activeThreat().getThreatProgress(), ImVec2(-1, 0),
                               "Threat approaching");
        ImGui::Separator();
        if (gameplayActive) {
            ImGui::Text("Select a world");
            for (std::size_t i = 0; i < bodies.size(); ++i) {
                if (!bodyEffect(static_cast<int>(i)).swallowed &&
                    ImGui::Selectable(bodies[i].name.c_str(), selected == static_cast<int>(i)))
                    selectBody(static_cast<int>(i));
            }
            if (!bodyEffect(8).swallowed && ImGui::Selectable("Sun", selected == 8))
                selectBody(8);
        }
        if (selected >= 0) {
            const Body &b = selected == 8 ? sunBody : bodies[static_cast<std::size_t>(selected)];
            ImGui::Separator();
            ImGui::Text("%s", b.name.c_str());
            ImGui::Text("Type: %s", b.type.c_str());
            ImGui::Text("Diameter: %.0f km", b.diameterKm);
            ImGui::Text("Mass: %.4g kg", b.massKg);
            ImGui::Text("Distance from Sun: %.1f million km", b.distanceMillionKm);
            ImGui::Text("Orbital period: %.2f days", b.orbitalDays);
            ImGui::Text("Rotation period: %.3f days", b.rotationPeriodDays);
            ImGui::Text("Gravity: %.2f m/s^2", b.gravity);
            ImGui::Text("Moons: %d", b.moons);
            for (const std::string &fact : b.facts)
                ImGui::BulletText("%s", fact.c_str());
            if (ImGui::Button("Start Quiz"))
                beginQuiz();
            ImGui::SameLine();
            if (ImGui::Button("Back")) {
                selected = -1;
                distance = overviewDistance;
                flying = true;
                flyElapsed = 0;
                startCamera = currentCamera;
                startTarget = cameraTarget;
            }
        }
        ImGui::Separator();
        if (gameState == GameState::FreeExplore)
            ImGui::Text("Esc: back to overview");
        ImGui::TextWrapped(
            "RMB drag: camera | Wheel: zoom | F: free-fly | WASD move | Shift faster");
        ImGui::End();
        if (help) {
            ImGui::Begin("Controls");
            ImGui::Text("F1 or H: toggle this help overlay");
            ImGui::Text("Space: pause simulation");
            ImGui::Text("1 / 2 / 3: simulation speed 1x / 5x / 10x");
            ImGui::Text("G: toggle bloom state | L: toggle lensing state");
            ImGui::Text("I: toggle instancing state | Q: toggle high / low sphere quality");
            ImGui::Text("B: summon / return black hole | R: return in Free Explore / restart mission");
            ImGui::Text("F: toggle free-fly | [ / ]: rotate Sun light");
            ImGui::Text("Esc: clear selection / return to overview");
            ImGui::Text("F12: save screenshot");
            ImGui::Text("Left click: select a body | Right drag: orbit camera | Wheel: zoom");
            ImGui::Text("WASD: move in free-fly | Shift: move faster");
            ImGui::End();
        }
        if (gameState == GameState::StartScreen) {
            ImGui::SetNextWindowPos(
                ImVec2(static_cast<float>(width) * 0.5f, static_cast<float>(height) * 0.5f),
                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::Begin("Solar System Explorer", nullptr,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::TextWrapped(
                "Visit worlds and answer questions before the threat reaches the Sun.");
            if (ImGui::Button("Start Mission", ImVec2(220, 0)))
                startMission();
            if (ImGui::Button("Free Explore", ImVec2(220, 0)))
                startFreeExplore();
            ImGui::End();
        }
        if (gameState == GameState::Won || gameState == GameState::Lost) {
            ImGui::SetNextWindowPos(
                ImVec2(static_cast<float>(width) * 0.5f, static_cast<float>(height) * 0.5f),
                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::Begin("Mission Result", nullptr,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::TextUnformatted(gameState == GameState::Won ? "Mission complete"
                                                               : "The Sun was swallowed");
            ImGui::Text("Final score: %d / %d", missionScore, MISSION_TARGET_SCORE);
            if (ImGui::Button("Restart", ImVec2(220, 0)))
                startMission();
            ImGui::End();
        }
        if (quizOpen) {
            ImGui::SetNextWindowSize(ImVec2(520, 300), ImGuiCond_Always);
            ImGui::Begin("Planet Quiz", &quizOpen, ImGuiWindowFlags_NoResize);
            if (quizFinished || quizQuestionIndex >= quizRound.size()) {
                ImGui::Text("Quiz complete");
                ImGui::Text("Final score: %d / %d", quizScore, static_cast<int>(quizRound.size()));
                if (ImGui::Button("Close")) {
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
                        if (ImGui::Button(q.choices[static_cast<std::size_t>(i)].c_str())) {
                            quizChoice = i;
                            quizAnswered = true;
                            const bool correct = q.choices[static_cast<std::size_t>(i)] == q.answer;
                            if (correct)
                                ++quizScore;
                            recordQuizAnswer(correct);
                        }
                } else {
                    ImGui::TextColored(
                        quizChoice >= 0 &&
                                q.choices[static_cast<std::size_t>(quizChoice)] == q.answer
                            ? ImVec4(0.3f, 1, 0.4f, 1)
                            : ImVec4(1, 0.35f, 0.3f, 1),
                        "%s",
                        quizChoice >= 0 &&
                                q.choices[static_cast<std::size_t>(quizChoice)] == q.answer
                            ? "Correct!"
                            : "Not quite.");
                    if (quizChoice < 0 ||
                        q.choices[static_cast<std::size_t>(quizChoice)] != q.answer)
                        ImGui::Text("Correct answer: %s", q.answer.c_str());
                    if (ImGui::Button(quizQuestionIndex + 1 == quizRound.size() ? "Show result"
                                                                                : "Next")) {
                        ++quizQuestionIndex;
                        quizAnswered = false;
                        if (quizQuestionIndex >= quizRound.size())
                            quizFinished = true;
                    }
                }
                if (ImGui::Button("Close")) {
                    quizOpen = false;
                    paused = quizWasPaused;
                }
            }
            ImGui::End();
            if (!quizOpen)
                paused = quizWasPaused;
        }
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
