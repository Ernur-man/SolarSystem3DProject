#include "Application.h"

#include <glad/glad.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
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
} // namespace

Application::~Application() {
    skyTexture.release();
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
            std::find_if(planetObjects.begin(), planetObjects.end(), [&](const std::string &item) {
                return field(item, "name") == body.name;
            });
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
        std::find_if(planetObjects.begin(), planetObjects.end(), [](const std::string &item) {
            return field(item, "name") == "Sun";
        });
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
        if (static_cast<size_t>(std::distance(uniqueQuestions.begin(), uniqueEnd)) < 6)
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
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    sphere.makeSphere(32, 48);
    highlightRing.makeOrbit(1.0f);
    if (!shader.load("shaders/planet.vert", "shaders/planet.frag") ||
        !sunShader.load("shaders/planet.vert", "shaders/sun.frag") ||
        !skyShader.load("shaders/sky.vert", "shaders/sky.frag") ||
        !orbitShader.load("shaders/orbit.vert", "shaders/orbit.frag"))
        return false;
    skyTexture.load("assets/textures/stars_milky_way.jpg");
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
    return true;
}

glm::vec3 Application::cameraPosition() const {
    return cameraTarget + glm::vec3(distance * std::cos(pitch) * std::sin(yaw),
                                    distance * std::sin(pitch),
                                    distance * std::cos(pitch) * std::cos(yaw));
}

glm::vec3 Application::bodyPosition(int index, float time) const {
    if (index == 8 || index < 0)
        return glm::vec3(0);
    const Body &body = bodies[static_cast<size_t>(index)];
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
    std::vector<size_t> candidates;
    const std::string name = selected == 8 ? "Sun" : bodies[static_cast<size_t>(selected)].name;
    for (size_t i = 0; i < questions.size(); ++i)
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

void Application::draw(const Body &body, bool sun) {
    const int index = sun ? 8 : static_cast<int>(&body - bodies.data());
    const glm::vec3 position = bodyPosition(index, simTime);
    glm::mat4 model = glm::translate(glm::mat4(1), position);
    const float rotationAngle = simTime * 6.2831853f * ROTATION_SCALE / body.rotationPeriodDays;
    model = glm::rotate(model, rotationAngle, glm::vec3(0, 1, 0));
    model = glm::scale(model, glm::vec3(sun ? 1.25f : body.radius));
    Shader &activeShader = sun ? sunShader : shader;
    activeShader.use();
    activeShader.set("model", model);
    if (sun)
        activeShader.set("time", simTime);
    else {
        activeShader.set("color", body.color);
        activeShader.set("lightPos", glm::vec3(0));
        activeShader.set("cameraPos", currentCamera);
        activeShader.set("emissive", 0.0f);
    }
    sphere.draw();
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
    static bool gWasDown = false, lWasDown = false, f1WasDown = false;
    const bool g = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
    const bool l = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
    const bool f1 = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
    if (g && !gWasDown)
        bloom = !bloom;
    if (l && !lWasDown)
        lensing = !lensing;
    if (f1 && !f1WasDown)
        help = !help;
    gWasDown = g;
    lWasDown = l;
    f1WasDown = f1;

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
    if (left && !lastLeftDown && !overUi && !quizOpen) {
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
            const glm::vec3 center = bodyPosition(i, simTime);
            const float radius = i == 8 ? 1.25f : bodies[static_cast<size_t>(i)].radius;
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
        if (!paused && !quizOpen)
            simTime += dt * static_cast<float>(speed);
        if (flying) {
            flyElapsed += dt;
            const float t = std::clamp(flyElapsed / 1.5f, 0.0f, 1.0f);
            const float eased =
                t * t * (3.0f - 2.0f); // Smoothstep eases both ends of the camera path.
            const glm::vec3 endTarget =
                selected >= 0 ? bodyPosition(selected, simTime) : glm::vec3(0);
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
            const glm::vec3 target = bodyPosition(selected, simTime);
            cameraTarget = target;
            currentCamera = target + flyCameraOffset;
        } else if (!freeFly) {
            currentCamera = cameraPosition();
        }
        glViewport(0, 0, width, height);
        glClearColor(0.004f, 0.006f, 0.025f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        const glm::mat4 view = glm::lookAt(currentCamera, cameraTarget, glm::vec3(0, 1, 0));
        const glm::mat4 projection =
            glm::perspective(glm::radians(45.0f), static_cast<float>(width) / height, 0.1f, 100.0f);
        drawSky(view, projection);
        orbitShader.use();
        orbitShader.set("view", view);
        orbitShader.set("projection", projection);
        glDisable(GL_CULL_FACE);
        for (size_t i = 0; i < bodies.size(); ++i) {
            orbitShader.set("model", glm::mat4(1));
            orbitShader.set("color", glm::vec3(0.26f, 0.31f, 0.42f));
            orbitLines[i].drawOrbit();
        }
        if (selected >= 0) {
            const glm::vec3 position = bodyPosition(selected, simTime);
            const float radius =
                selected == 8 ? 1.25f : bodies[static_cast<size_t>(selected)].radius;
            const glm::mat4 ringModel =
                glm::translate(glm::mat4(1), position + glm::vec3(0, radius * 1.15f, 0)) *
                glm::scale(glm::mat4(1), glm::vec3(radius * 1.35f, 1, radius * 1.35f));
            orbitShader.set("model", ringModel);
            orbitShader.set("color", glm::vec3(1.0f, 0.85f, 0.25f));
            highlightRing.drawOrbit();
        }
        glEnable(GL_CULL_FACE);
        shader.use();
        shader.set("view", view);
        shader.set("projection", projection);
        for (const Body &body : bodies)
            draw(body);
        sunBody.radius = 1.25f;
        sunShader.use();
        sunShader.set("view", view);
        sunShader.set("projection", projection);
        draw(sunBody, true);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(static_cast<float>(width) - 350.0f, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(350, static_cast<float>(height)), ImGuiCond_Always);
        ImGui::Begin("Mission Control", nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        ImGui::Text("Team: Your Team | Event Horizon: Solar System Explorer");
        ImGui::Text("FPS %.0f | Speed %dx | %s", dt > 0 ? 1 / dt : 0, speed,
                    paused ? "Paused" : "Running");
        ImGui::Text("Bloom %s | Lensing %s", bloom ? "ON" : "OFF", lensing ? "ON" : "OFF");
        ImGui::Separator();
        ImGui::Text("Select a world");
        for (size_t i = 0; i < bodies.size(); ++i)
            if (ImGui::Selectable(bodies[i].name.c_str(), selected == static_cast<int>(i)))
                selectBody(static_cast<int>(i));
        if (ImGui::Selectable("Sun", selected == 8))
            selectBody(8);
        if (selected >= 0) {
            const Body &b = selected == 8 ? sunBody : bodies[static_cast<size_t>(selected)];
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
        ImGui::TextWrapped("RMB drag: camera | Wheel: zoom | F: free-fly | WASD move | Shift "
                           "faster | 1/2/3 speed | Space pause | Esc overview");
        ImGui::End();
        if (help) {
            ImGui::Begin("Controls");
            ImGui::TextWrapped("Orbit camera: right mouse drag and wheel. F toggles free-fly; WASD "
                               "moves and Shift increases speed. Select a planet to fly to it.");
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
                        if (ImGui::Button(q.choices[static_cast<size_t>(i)].c_str())) {
                            quizChoice = i;
                            quizAnswered = true;
                            if (q.choices[static_cast<size_t>(i)] == q.answer)
                                ++quizScore;
                        }
                } else {
                    ImGui::TextColored(
                        quizChoice >= 0 && q.choices[static_cast<size_t>(quizChoice)] == q.answer
                            ? ImVec4(0.3f, 1, 0.4f, 1)
                            : ImVec4(1, 0.35f, 0.3f, 1),
                        "%s",
                        quizChoice >= 0 && q.choices[static_cast<size_t>(quizChoice)] == q.answer
                            ? "Correct!"
                            : "Not quite.");
                    if (quizChoice < 0 || q.choices[static_cast<size_t>(quizChoice)] != q.answer)
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
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}
