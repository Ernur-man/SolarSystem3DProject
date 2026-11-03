#include "Application.h"

#include <glad/glad.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

Application::~Application() {
    if (window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

bool Application::init() {
    if (!glfwInit()) {
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, "Solar System 3D - Mission Control", nullptr, nullptr);
    if (!window) {
        return false;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        return false;
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    sphere.makeSphere(32, 48);

    if (!shader.load("shaders/planet.vert", "shaders/planet.frag") ||
        !sunShader.load("shaders/planet.vert", "shaders/sun.frag")) {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    bodies = {
        {"Mercury", 3, 0.22f, 0.24f, {0.62f, 0.57f, 0.52f}},
        {"Venus", 4.2f, 0.34f, 0.62f, {0.86f, 0.62f, 0.31f}},
        {"Earth", 5.7f, 0.38f, 1, {0.14f, 0.42f, 0.93f}},
        {"Mars", 7.1f, 0.30f, 1.88f, {0.85f, 0.27f, 0.12f}},
        {"Jupiter", 9.5f, 0.85f, 11.86f, {0.82f, 0.56f, 0.37f}},
        {"Saturn", 12.3f, 0.72f, 29.4f, {0.85f, 0.76f, 0.53f}},
        {"Uranus", 15, 0.58f, 84, {0.42f, 0.82f, 0.84f}},
        {"Neptune", 17.5f, 0.56f, 164.8f, {0.22f, 0.34f, 0.9f}}
    };
    return true;
}

glm::vec3 Application::cameraPosition() const {
    return {
        distance * std::cos(pitch) * std::sin(yaw),
        distance * std::sin(pitch),
        distance * std::cos(pitch) * std::cos(yaw)
    };
}

void Application::draw(const Body& body, bool sun) {
    const float angle = time * 6.2831853f / body.period;
    const glm::vec3 position = body.orbit
        ? glm::vec3(body.orbit * std::cos(angle), 0, body.orbit * std::sin(angle))
        : glm::vec3(0);
    glm::mat4 model = glm::translate(glm::mat4(1), position);
    model = glm::rotate(model, time * 0.5f, glm::vec3(0, 1, 0));
    model = glm::scale(model, glm::vec3(body.radius));

    Shader& activeShader = sun ? sunShader : shader;
    activeShader.use();
    activeShader.set("model", model);
    if (sun) {
        activeShader.set("time", time);
    } else {
        activeShader.set("color", body.color);
        activeShader.set("lightPos", glm::vec3(0));
        activeShader.set("cameraPos", cameraPosition());
        activeShader.set("emissive", 0.0f);
    }
    sphere.draw();
}

void Application::input(float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        selected = -1;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        paused = !paused;
    }
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
        speed = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
        speed = 5;
    }
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
        speed = 10;
    }
    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) {
        bloom = !bloom;
    }
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
        lensing = !lensing;
    }
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) {
        help = !help;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) {
        yaw -= deltaTime;
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) {
        yaw += deltaTime;
    }
}

void Application::run() {
    last = static_cast<float>(glfwGetTime());
    while (!glfwWindowShouldClose(window)) {
        const float now = static_cast<float>(glfwGetTime());
        const float deltaTime = now - last;
        last = now;
        glfwGetFramebufferSize(window, &width, &height);
        if (width == 0 || height == 0) {
            glfwWaitEvents();
            continue;
        }

        input(deltaTime);
        if (!paused) {
            time += deltaTime * static_cast<float>(speed);
        }
        glViewport(0, 0, width, height);
        glClearColor(0.004f, 0.006f, 0.025f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::vec3 camera = cameraPosition();
        const glm::mat4 view = glm::lookAt(camera, glm::vec3(0), glm::vec3(0, 1, 0));
        const glm::mat4 projection = glm::perspective(
            glm::radians(45.0f), static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);
        shader.use();
        shader.set("view", view);
        shader.set("projection", projection);
        for (const Body& body : bodies) {
            draw(body);
        }

        const Body sun{"Sun", 0, 1.25f, 1, {1, 0.62f, 0.12f}};
        sunShader.use();
        sunShader.set("view", view);
        sunShader.set("projection", projection);
        draw(sun, true);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Mission Control");
        ImGui::Text("Team: Your Team | Solar System 3D");
        ImGui::Text("FPS %.0f | Speed %dx | %s", deltaTime > 0 ? 1 / deltaTime : 0, speed, paused ? "Paused" : "Running");
        ImGui::Text("Score 0 | Free Explore");
        ImGui::Text("Bloom %s | Lensing %s | Instancing OFF", bloom ? "ON" : "OFF", lensing ? "ON" : "OFF");
        for (size_t index = 0; index < bodies.size(); ++index) {
            if (ImGui::Selectable(bodies[index].name.c_str(), selected == static_cast<int>(index))) {
                selected = static_cast<int>(index);
            }
        }
        if (selected >= 0) {
            ImGui::Separator();
            ImGui::Text("%s", bodies[static_cast<size_t>(selected)].name.c_str());
            ImGui::Text("Planetary world; orbital period %.2f Earth years", bodies[static_cast<size_t>(selected)].period);
            if (ImGui::Button("Start Quiz")) {
                ImGui::OpenPopup("Quiz");
            }
            if (ImGui::BeginPopupModal("Quiz", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Quiz content is available in assets/quiz.json");
                if (ImGui::Button("Close")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Back")) {
                selected = -1;
            }
        }
        ImGui::Text("1/2/3 speed | Space pause | G bloom | L lensing | F1 help | Esc back");
        ImGui::End();
        if (help) {
            ImGui::Begin("Controls");
            ImGui::Text("1/2/3 speed, Space pause, G bloom, L lensing, F1 help, Esc overview, [ ] rotate view");
            ImGui::End();
        }
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}
