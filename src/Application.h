#pragma once
#include "Shader.h"
#include "Mesh.h"
#include "Texture.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
struct Body {
    std::string name;
    float orbit;
    float radius;
    float period;
    float rotationPeriodDays;
    glm::vec3 color;
};

class Application {
public:
    bool init();
    void run();
    ~Application();

private:
    GLFWwindow* window = nullptr;
    Shader shader;
    Shader sunShader;
    Shader skyShader;
    Mesh sphere;
    Texture skyTexture;
    std::vector<Body> bodies;

    int width = 1280;
    int height = 720;
    int speed = 1;
    int selected = -1;
    float simTime = 0;
    float last = 0;
    float yaw = 0;
    float pitch = 0.25f;
    float distance = 22;
    bool bloom = true;
    bool lensing = true;
    bool paused = false;
    bool spaceWasDown = false;
    bool help = false;

    glm::vec3 cameraPosition() const;
    void input(float deltaTime);
    void draw(const Body& body, bool sun = false);
    void drawSky(const glm::mat4& view, const glm::mat4& projection);
};
